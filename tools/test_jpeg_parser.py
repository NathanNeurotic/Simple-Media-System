"""Actual JPEG marker/table/entropy parser with the PS2 VU/rescaler stubbed.

Valid baseline JPEG controls and truncated/malformed inputs run under ASan,
UBSan and LSan. This does not validate console pixels or VU DMA scheduling.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/SMS_JPEG.c').read_text()

def function(name):
    marker = '}  /* end ' + name + ' */'
    end = source.index(marker) + len(marker)
    matches = list(re.finditer(r'^.*\b' + re.escape(name) + r'\s*\([^;]*?\)\s*\{', source[:end], re.M))
    result = source[matches[-1].start():end]
    if name == '_unpack':
        result = re.sub(r' __asm__ __volatile__\(.*?\);', ' memset(apTbl,0,128);', result, flags=re.S)
    return result

constants = source[source.index('#define MK_SOI'):source.index('#define BLK_RB_SLOT')]
harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
typedef uint64_t u64;
#define __SMS_H
#include "SMS_JPEG.h"
#include "SMS_Rescale.h"
#define UNCACHED_SEG(p) (p)
''' + constants + r'''
static unsigned char scratch[64];
static unsigned finalizations,blocks;
static int bitmap_fail,rescale_fail;
static void process_buffer(SMS_RescaleContext *p,void *dst){assert(dst);++finalizations;}
static SMS_RescaleContext *SMS_RescaleInit(SMS_RescaleContext *p,int w,int h,int sh,int sw);
'''
# Header declares the public rescaler, so provide the same linkage.
harness = harness.replace('static SMS_RescaleContext *SMS_RescaleInit', 'SMS_RescaleContext *SMS_RescaleInit')
harness += r'''
SMS_RescaleContext *SMS_RescaleInit(SMS_RescaleContext *p,int w,int h,int sh,int sw) {
 if(rescale_fail)return NULL;
 if(!p)p=calloc(1,sizeof(*p));assert(p);
 p->m_NewWidth=w>16?16:w;p->m_NewHeight=h>16?16:h;
 p->m_pStripePtr=scratch;p->ProcessBuffer=process_buffer;return p;
}
static void _init_clr_fmt(_jpeg_clr_fmt fmt){}
static void _pack_8(void*p,int w,int h){}
static void _pack_16(void*p,int w,int h){}
static void *bitmap_allocate(size_t align,size_t size){return bitmap_fail?NULL:memalign(align,size);}
#define memalign bitmap_allocate
static unsigned char g_SMS_DSP_zigzag_direct[64]={
0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
'''
for name in ('_get_short_be', '_skip', '_get_byte', '_segment', '_next_MK', '_reset_pred',
             '_ceil_div', '_floor_div', '_init_MCU', '_load_qtbls', '_load_huff', '_bits', '_huff_sym', '_unpack'):
    harness += function(name)
harness += r'''
static int _process_MCU(SMS_JPEGContext *p,unsigned n) {
 short coeff[64];
 for(unsigned i=0;i<n;++i) for(unsigned j=0;j<p->m_nMCU;++j) {
  if(!_unpack(p,coeff,p->m_CompDesc+p->m_MCUIdx[j]))return 0;
  ++blocks;
 }
 return 1;
}
'''
harness += function('SMS_JPEGLoad')
harness += r'''
static void init(SMS_JPEGContext *p) {
 memset(p,0,sizeof(*p));p->m_Format=JCF_Invalid;
 p->m_pHTbl[0]=p->m_DCTbl[0];p->m_pHTbl[1]=p->m_DCTbl[1];
 p->m_pHTbl[2]=p->m_ACTbl[0];p->m_pHTbl[3]=p->m_ACTbl[1];
}
static void clear(SMS_JPEGContext *p){free(p->m_pRC);free(p->m_pBitmap);}
static void entropy_controls(void) {
 SMS_JPEGContext p;init(&p);short coeff[64];unsigned char bits[]={0};
 memset(p.m_QTable[0],1,64);
 p.m_DCTbl[0][0]=0x201; /* DC category 1, followed by a zero sign bit. */
 p.m_ACTbl[0][0]=0x200; /* EOB. */
 p.m_pData=bits;p.m_pDataEnd=bits+1;
 assert(_unpack(&p,coeff,p.m_CompDesc)&&coeff[0]==-1&&p.m_CompDesc[0].m_Pred==-1);
 for(int i=1;i<64;++i)assert(!coeff[i]);
 p.m_DCTbl[0][0]=0x200;p.m_ACTbl[0][0]=0x2f0;
 assert(!_unpack(&p,coeff,p.m_CompDesc));
 p.m_ACTbl[0][0]=0x2f1;p.m_nBits=0;p.m_pData=bits;
 assert(!_unpack(&p,coeff,p.m_CompDesc));
 p.m_ACTbl[0][0]=0x101;p.m_ACTbl[0][2]=0x101;p.m_ACTbl[0][3]=0x101;
 p.m_nBits=0;p.m_pData=bits;
 assert(!_unpack(&p,coeff,p.m_CompDesc));
}
static int load(SMS_JPEGContext *p,const unsigned char *data,size_t n) {
 unsigned char *exact=malloc(n?n:1);assert(exact);if(n)memcpy(exact,data,n);
 int result=SMS_JPEGLoad(p,exact,n);free(exact);
 if(!result)assert(!p->m_nBitmapBytes&&!p->m_nBitmapQWC);
 return result;
}
int main(int argc,char **argv) {
 entropy_controls();
 assert(argc==2||argc==3);FILE *f=fopen(argv[1],"rb");assert(f);
 fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);
 unsigned char *data=malloc(n);assert(data&&fread(data,1,n,f)==n);fclose(f);
 SMS_JPEGContext p;init(&p);
 if(argc==3){assert(load(&p,data,n)==!strcmp(argv[2],"valid-only"));clear(&p);free(data);return 0;}
 assert(!SMS_JPEGLoad(NULL,data,n));assert(!SMS_JPEGLoad(&p,NULL,0));
 assert(load(&p,data,n)==1);assert(p.m_nBitmapBytes&&finalizations&&blocks);
 const unsigned char eoi[]={255,216,255,217};
 assert(!load(&p,eoi,sizeof(eoi)));assert(!p.m_nComp&&!p.m_Width&&!p.m_Height);
 for(size_t i=0;i<n;++i) assert(!load(&p,data,i));
 assert(load(&p,data,n));
 /* Mutate each length, selector, sampling or table class at its real marker. */
 for(size_t i=2;i+3<n;) {
  if(data[i]!=255){++i;continue;}
  unsigned marker=data[i+1];if(marker==0xda) {
   unsigned char *bad=malloc(n);memcpy(bad,data,n);bad[i+6]=0xff;
   assert(!load(&p,bad,n));free(bad);break;
  }
  unsigned len=(data[i+2]<<8)|data[i+3];assert(len>=2&&i+len+2<=n);
  unsigned char *bad=malloc(n);memcpy(bad,data,n);
  bad[i+2]=255;bad[i+3]=255;assert(!load(&p,bad,n));
  bad[i+2]=0;bad[i+3]=1;assert(!load(&p,bad,n));memcpy(bad,data,n);
  if(marker==0xdb){bad[i+4]=0x0f;assert(!load(&p,bad,n));}
  if(marker==0xc4){
   bad[i+4]=0x20;assert(!load(&p,bad,n));memcpy(bad,data,n);
   bad[i+5]=255;assert(!load(&p,bad,n));
  }
  if(marker==0xc0){bad[i+11]=0;assert(!load(&p,bad,n));bad[i+11]=0xff;assert(!load(&p,bad,n));}
  if(marker==0xdb||marker==0xc4){
   memcpy(bad,data,i);memcpy(bad+i,data+i+len+2,n-i-len-2);
   assert(!load(&p,bad,n-len-2));
  }
  free(bad);i+=len+2;
 }
 /* Deterministic garbage, including SOI followed by short marker tails. */
 unsigned state=7;
 for(unsigned k=0;k<500;++k) {
  unsigned char bad[96];size_t len=k%sizeof(bad);
  for(size_t j=0;j<len;++j){state=state*1664525+1013904223;bad[j]=state>>24;}
  if(len>1){bad[0]=255;bad[1]=216;}
  assert(!load(&p,bad,len));
 }
 SMS_RescaleContext *previous=p.m_pRC;rescale_fail=1;
 assert(!load(&p,data,n)&&p.m_pRC==previous);rescale_fail=0;
 assert(load(&p,data,n));clear(&p);init(&p);rescale_fail=1;
 assert(!load(&p,data,n)&&!p.m_pRC);rescale_fail=0;
 bitmap_fail=1;assert(!load(&p,data,n));bitmap_fail=0;
 assert(load(&p,data,n));unsigned char *old_bitmap=p.m_pBitmap;p.m_nAlloc=0;bitmap_fail=1;
 assert(!load(&p,data,n)&&p.m_pBitmap==old_bitmap);bitmap_fail=0;
 assert(load(&p,data,n));clear(&p);free(data);return 0;
}
'''

def segment(marker, payload):
    return bytes((255, marker)) + (len(payload)+2).to_bytes(2, 'big') + payload

# A three-component 1x1 baseline image. Each block uses DC category 0 and EOB.
tiny = b'\xff\xd8' + segment(0xdb, b'\0'+bytes([1])*64)
tiny += segment(0xc4, b'\0\1'+bytes(15)+b'\0'+b'\x10\1'+bytes(15)+b'\0')
tiny += segment(0xc0, bytes([8,0,1,0,1,3,1,0x11,0,2,0x11,0,3,0x11,0]))
tiny += segment(0xda, bytes([3,1,0,2,0,3,0,0,63,0])) + b'\x03\xff\xd9'
with tempfile.TemporaryDirectory() as temp:
    temp = Path(temp)
    c, exe = temp / 'jpeg.c', temp / 'jpeg'
    c.write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), '-g', '-no-pie', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-I', str(root / 'include'), str(c), '-o', str(exe)], check=True)
    fixture = temp / 'tiny.jpg';fixture.write_bytes(tiny)
    subprocess.run([str(exe), str(fixture)], check=True, timeout=15)
    restart = bytearray(tiny)
    restart[restart.index(b'\xff\xc0') + 8] = 16
    sos = restart.index(b'\xff\xda')
    restart[sos:sos] = segment(0xdd, b'\0\1')
    restart[-3:-2] = b'\x03\xff\xd0\x03'
    fixture = temp / 'restart.jpg';fixture.write_bytes(restart)
    subprocess.run([str(exe), str(fixture)], check=True, timeout=15)
    restart[restart.index(b'\xff\xd0') + 1] = 0xd7
    fixture = temp / 'restart-invalid.jpg';fixture.write_bytes(restart)
    subprocess.run([str(exe), str(fixture), 'wrong-restart-marker'], check=True, timeout=15)
    for sampling, count in ((0x21, 4), (0x12, 4), (0x22, 6)):
        control = bytearray(tiny)
        sof = control.index(b'\xff\xc0')
        control[sof + 11] = sampling
        control[-3] = (1 << (8 - count * 2 % 8)) - 1 if count * 2 % 8 else 0
        if count > 4:
            control[-3:-3] = b'\0'
        fixture = temp / ('sampling-' + str(sampling) + '.jpg')
        fixture.write_bytes(control)
        subprocess.run([str(exe), str(fixture)], check=True, timeout=15)
    if not shutil.which('ffmpeg'):
        raise SystemExit('ffmpeg is required for independent baseline controls')
    for pix_fmt in ('yuvj444p', 'yuvj422p', 'yuvj420p'):
        fixture = temp / (pix_fmt + '.jpg')
        subprocess.run(['ffmpeg', '-loglevel', 'error', '-f', 'lavfi', '-i', 'testsrc2=size=34x18:rate=1',
                        '-frames:v', '1', '-threads', '1', '-c:v', 'mjpeg', '-pix_fmt', pix_fmt, str(fixture)], check=True)
        # FFmpeg's 4:2:2 encoder uses eight blocks per MCU, beyond this decoder's
        # six-entry MCUIdx. Reject it before _init_MCU can overwrite the context.
        command = [str(exe), str(fixture)]
        if pix_fmt == 'yuvj422p':
            command.append('unsupported-eight-block-mcu')
        subprocess.run(command, check=True, timeout=15)
    for name in ('main_bg_mini.jpg', 'jellyfish.jpg', 'splash.jpg'):
        subprocess.run([str(exe), str(root / 'images' / name), 'valid-only'], check=True, timeout=15)
print('JPEG parser: four MCU layouts, FFmpeg 4:4:4/4:2:0 controls, unsupported eight-block MCU, every truncated prefix, malformed segments/tables/selectors/sampling, repeated loads, EOI-only and bitmap failure passed')
