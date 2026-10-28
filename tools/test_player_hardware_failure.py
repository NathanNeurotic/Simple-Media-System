"""Actual video decoder worker drains input after codec hardware failure.

Controlled kernel/ring stubs stop the persistent worker after a released
packet. This checks producer progress, not PS2 thread scheduling.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/SMS_Player.c').read_text()
marker = '}  /* end _sms_video_decoder */'
end = source.index(marker) + len(marker)
start = source.rfind('\nstatic ', 0, source.index(marker)) + 1
harness = r'''
#include <assert.h>
#include <setjmp.h>
#include <stdint.h>
#include <stddef.h>
#define SMS_THREAD_PRIORITY 64
typedef struct {int unused;} SMS_RingBuffer;
typedef struct {volatile int m_HardwareError;} SMS_CodecContext;
typedef struct {int m_Size;} SMS_AVPacket;
typedef struct {SMS_CodecContext *m_pCodec;SMS_RingBuffer *m_pPktBuf;} SMS_Stream;
typedef struct {SMS_Stream **m_pStm;} SMS_Container;
typedef struct {int (*Decode)(SMS_CodecContext *,SMS_RingBuffer *,SMS_RingBuffer *);} SMS_Codec;
static struct {int m_VideoIdx;SMS_Container *m_pCont;SMS_Codec *m_pVideoCodec;} s_Player;
static SMS_RingBuffer *s_pVideoBuffer;
static jmp_buf stop;
static unsigned decoded,waited,freed,yielded;
static SMS_AVPacket packet={16};
static int decode(SMS_CodecContext *c,SMS_RingBuffer *out,SMS_RingBuffer *in) {
 assert(!c->m_HardwareError);assert(++decoded==1);c->m_HardwareError=1;return 0;
}
static SMS_AVPacket *SMS_RingBufferWait(SMS_RingBuffer *q) { ++waited;return &packet; }
static void SMS_RingBufferFree(SMS_RingBuffer *q,int size) {assert(size==80);++freed;}
static void RotateThreadReadyQueue(int n) {assert(n==SMS_THREAD_PRIORITY);++yielded;longjmp(stop,1);}
''' + source[start:end] + r'''
int main(void) {
 SMS_CodecContext context={0};SMS_RingBuffer input,output;
 SMS_Stream stream={&context,&input},*streams[]={&stream};SMS_Container container={streams};SMS_Codec codec={decode};
 s_Player.m_VideoIdx=0;s_Player.m_pCont=&container;s_Player.m_pVideoCodec=&codec;s_pVideoBuffer=&output;
 if(!setjmp(stop))_sms_video_decoder(NULL);
 assert(context.m_HardwareError&&decoded==1&&waited==1&&freed==1&&yielded==1);
 decoded=waited=freed=yielded=0;
 if(!setjmp(stop))_sms_video_decoder(NULL);
 assert(!decoded&&waited==1&&freed==1&&yielded==1);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    c, exe = Path(temp) / 'hardware.c', Path(temp) / 'hardware'
    c.write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), '-g', '-no-pie', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=5)
print('Player hardware failure: worker stops decoding and releases queued input to unblock the producer')
