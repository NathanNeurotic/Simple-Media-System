"""Actual frame allocators, MPEG4 hardware init and MPEG12 sequence failure paths.

PS2 tags/interrupts are stubs; allocation ownership and cleanup use ASan/LSan.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def function(path, name):
    source = (root / path).read_text()
    end = source.index('}  /* end ' + name + ' */') + len('}  /* end ' + name + ' */')
    matches = list(re.finditer(r'^.*\b' + re.escape(name) + r'\s*\([^;]*?\)\s*\{', source[:end], re.M))
    return source[matches[-1].start():end]

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define __SMS_H
#define __SMS_RingBuffer_H
#define __SMS_List_H
#define __SMS_Hash_H
#define __SMS_Bitio_H
#define __SMS_Container_H
#define __SMS_FileContext_H
#define __SMS_DSP_H
#define __SMS_Codec_H
#define __SMS_MPEG_H
typedef int64_t s64;
typedef uint64_t u64;
typedef struct { unsigned char bytes[384]; } SMS_MacroBlock;
#include "SMS_VideoBuffer.h"
static int fail_at, allocations, live, handler_fail, handlers, resumes;
static void *allocate(size_t n) {
 if (++allocations==fail_at) return NULL;
 void *p=malloc(n);assert(p);++live;return p;
}
static void *test_calloc(size_t n,size_t size) { void *p=allocate(n*size);if(p)memset(p,0,n*size);return p; }
static void *test_memalign(size_t align,size_t size) { (void)align;return allocate(size); }
static void test_free(void *p) { if(p){--live;free(p);} }
#define malloc allocate
#define calloc test_calloc
#define memalign test_memalign
#define free test_free
static struct {unsigned m_VRAM,m_TBW,m_TexFmt;} g_IPUCtx;
#define GSPixelFormat_PSMCT16 2
#define GSPixelFormat_PSMCT32 0
#define GS_SET_BITBLTBUF(...) 0ULL
#define GIF_TAG(...) 0ULL
#define DMA_TAG(...) 0ULL
#define GIFTAG_REGS_AD 0
#define GS_SET_TRXREG(...) 0ULL
#define GS_TRXREG 0
#define GS_BITBLTBUF 0
#define GS_SET_TRXPOS(...) 0ULL
#define GS_TRXPOS 0
#define GS_SET_TRXDIR(...) 0ULL
#define GS_TRXDIR 0
static int _csc_handler(int a,void*b,void*c){return 0;}
static int AddDmacHandler2(int a,void*b,int c,void*d){if(handler_fail)return -1;++handlers;return 1;}
static void DisableDmac(int n){assert(n==3);}
static void RemoveDmacHandler(int n,int id){assert(n==3&&id==1);--handlers;}
void SMS_CSCResume(void){++resumes;}
void SMS_CSCSuspend(void){}
void SMS_CSCSync(void){}
typedef enum {SMS_HWC_Init,SMS_HWC_Destroy,SMS_HWC_Suspend,SMS_HWC_Resume,SMS_HWC_Reset} SMS_CodecHWCtl;
typedef struct {int m_Width,m_Height,m_HardwareError;void *m_pIntBuf,*m_pHWData;} SMS_CodecContext;
typedef struct {SMS_FrameBuffer m_Buf[3];int m_BufIdx;} SMS_MPEGCSC;
'''
for name in ('SMS_FrameBufferDestroy', 'SMS_VideoBufferDestroy', 'SMS_FrameBufferInit', 'SMS_VideoBufferInit'):
    harness += function('src/SMS_VideoBuffer.c', name)
harness += function('src/SMS_MPEG4.c', 'MPEG4_CommonHWCtl')
harness += r'''
typedef struct {unsigned char bytes[384];} _MPEGMacroBlock8;
typedef struct {int m_X,m_Y;} _MPEGMBXY;
typedef struct {int m_Width,m_Height,m_FrameCnt;} MPEGSequenceInfo;
#define _MPEG_PS_FRAME 3
static struct {
 int m_fMPEG2,m_fProgSeq,m_PictStruct,m_fFPFrmDCT;
 int m_MBWidth,m_MBHeight,m_MBStride,m_MBCount,m_fSecField;
 MPEGSequenceInfo m_SI;
 _MPEGMacroBlock8 *m_pFwdFrame,*m_pBckFrame,*m_pAuxFrame;
 _MPEGMBXY *m_pMBXY;
} s_MPEG12Ctx;
static int decoded,callback_fail;
static void *s_pInitCBParam;
static int _get_first_picture(void *,s64 *);
static int (*MPEG_Picture)(void *,s64 *)=_get_first_picture;
static int _get_hdr(void){return 1;}
static void *_init_cb(void *p,MPEGSequenceInfo *i){return callback_fail?NULL:(void *)1;}
static void _mpeg12_picture_data(void){++decoded;}
static int _get_next_picture(void *p,s64 *pts){assert(p);return 1;}
'''
for name in ('_destroy_seq', '_init_seq', '_get_first_picture'):
    harness += function('src/libmpeg.c', name)
harness += r'''
static void reset(int fail) {
 assert(!live&&!handlers);
 fail_at=fail;allocations=resumes=handler_fail=decoded=callback_fail=0;
 memset(&s_MPEG12Ctx,0,sizeof(s_MPEG12Ctx));
 s_MPEG12Ctx.m_SI.m_Width=640;s_MPEG12Ctx.m_SI.m_Height=480;
}
int main(void) {
 for(int depth=0;depth<2;++depth) for(int fail=0;fail<=3;++fail) {
  SMS_FrameBuffer frames[3]={0};reset(fail);
  assert(SMS_FrameBufferInit(frames,3,640,480,depth)==!fail);
  if(fail) for(int i=0;i<3;++i)assert(!frames[i].m_pBase&&!frames[i].m_pData&&frames[i].m_FrameType==-1);
  SMS_FrameBufferDestroy(frames,3);SMS_FrameBufferDestroy(frames,3);assert(!live);
 }
 for(int fail=0;fail<=4;++fail) {
  reset(fail);SMS_VideoBuffer *b=SMS_VideoBufferInit(640,480);
  assert((b!=NULL)==!fail);SMS_VideoBufferDestroy(b);assert(!live&&!handlers);
 }
 reset(0);handler_fail=1;assert(!SMS_VideoBufferInit(640,-480));assert(!live&&!handlers);
 for(int fail=0;fail<=8;++fail) {
  SMS_CodecContext c={640,480,0,NULL,NULL};reset(fail);
  MPEG4_CommonHWCtl(&c,SMS_HWC_Init);
  assert(c.m_HardwareError==!!fail);assert(resumes==!fail);
  if(fail)assert(!c.m_pHWData&&!c.m_pIntBuf&&!live&&!handlers);
  MPEG4_CommonHWCtl(&c,SMS_HWC_Reset);
  MPEG4_CommonHWCtl(&c,SMS_HWC_Destroy);MPEG4_CommonHWCtl(&c,SMS_HWC_Destroy);
  assert(!live&&!handlers);
 }
 for(int fail=0;fail<=4;++fail) {
  s64 pts;reset(fail);assert(_get_first_picture(NULL,&pts)==(fail?-1:1));
  assert(decoded==!fail);_destroy_seq();assert(!live);
 }
 for(int fail=1;fail<=4;++fail) {
  s64 pts;reset(0);assert(_get_first_picture(NULL,&pts)==1);
  fail_at=allocations+fail;s_MPEG12Ctx.m_SI.m_Width=320;s_MPEG12Ctx.m_SI.m_Height=240;
  assert(_get_first_picture(NULL,&pts)==-1&&decoded==1&&!live);_destroy_seq();
 }
 reset(0);callback_fail=1;s64 pts;assert(_get_first_picture(NULL,&pts)==-1&&!decoded);_destroy_seq();assert(!live);
 reset(0);SMS_FrameBuffer frames[3]={0};
 assert(!SMS_FrameBufferInit(frames,3,0,480,0));
 assert(!SMS_FrameBufferInit(frames,3,INT_MAX,480,0));
 assert(!SMS_FrameBufferInit(frames,3,1000000,1000000,0));
 assert(!SMS_VideoBufferInit(0,480));assert(!SMS_VideoBufferInit(640,INT_MIN));assert(!live);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    c, exe = Path(temp) / 'allocation.c', Path(temp) / 'allocation'
    c.write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), '-g', '-no-pie', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-I', str(root / 'include'), str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=10)
print('Frame allocation: RGB/YUV first/later failures, hardware init/handler failure, sequence abort, cleanup and valid controls passed')
