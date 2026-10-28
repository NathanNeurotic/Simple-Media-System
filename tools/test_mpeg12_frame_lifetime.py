"""Actual MPEG-1/2 codec frame ownership under a delayed renderer.

The decoder and renderer are deterministic stubs: this checks that an owned
RGB buffer is not written or freed, not color coefficients or console output.
"""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path)
parser.add_argument("--case", default="all", choices=("all", "busy", "resize", "normal", "failure"))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (args.source or root / "src/SMS_MPEG12.c").read_text()


def function(name):
    marker = "}  /* end " + name + " */"
    if marker not in source:
        return ""
    end = source.index(marker) + len(marker)
    start = source.rfind("\nstatic ", 0, source.index(marker)) + 1
    # Skip a forward declaration, if any: locate the implementation nearest its end.
    return source[start:end]


state_start = source.index("typedef struct MPEGState")
state_end = source.index("} MPEGState;") + len("} MPEGState;")
harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define SMS_THREAD_PRIORITY 64
typedef struct { void *m_pBase,*m_pData;int m_FrameType;int64_t m_StartPTS; } SMS_FrameBuffer;
typedef struct { int m_Width,m_Height,m_MSPerFrame; } MPEGSequenceInfo;
typedef struct { int unused; } SMS_RingBuffer;
typedef struct { int m_HardwareError; } SMS_CodecContext;
typedef struct { int m_Size; } SMS_AVPacket;
""" + source[state_start:state_end] + r"""
static MPEGState s_MPEGState;
static unsigned char pixels[3][32];
static SMS_FrameBuffer *queued;
static unsigned yields,pictures,posted,allocations,destroys,release_after;
static unsigned holds[3];
static int picture_result,frame_failure,freed_packets;
static int RotateThreadReadyQueue(int priority) {
 assert(priority==SMS_THREAD_PRIORITY);assert(++yields<100);
 for(unsigned i=0;i<3;++i) {
  SMS_FrameBuffer *f=&s_MPEGState.m_Pics[i];
  if(f->m_pBase && f->m_FrameType>=0) {
   if(++holds[i]>=release_after) f->m_FrameType=-1;
   break;
  }
 }
 return 0;
}
static int MPEG_Picture(void *base,int64_t *pts) {
 ++pictures;
 for(unsigned i=0;i<3;++i) if(base && base==s_MPEGState.m_Pics[i].m_pBase)
  assert(s_MPEGState.m_Pics[i].m_FrameType<0 && "decoder overwrites a renderer-owned frame");
 *pts=100;return picture_result;
}
static void *SMS_RingBufferAlloc(SMS_RingBuffer *q,int size) {
 (void)q;assert(size==4 && pictures);++allocations;return &queued;
}
static void SMS_RingBufferPost(SMS_RingBuffer *q) { (void)q;++posted; }
static void SMS_RingBufferFree(SMS_RingBuffer *q,int size) { (void)q;assert(size==80);++freed_packets; }
static void SMS_FrameBufferDestroy(SMS_FrameBuffer *frames,int count) {
 ++destroys;
 for(int i=0;i<count;++i) {
  assert((!frames[i].m_pBase || frames[i].m_FrameType<0) && "resize frees an in-flight frame");
  frames[i].m_pBase=NULL;
 }
}
static int SMS_FrameBufferInit(SMS_FrameBuffer *frames,int count,int w,int h,int depth) {
 assert(count==3 && w==640 && h==480 && depth==0);
 if(frame_failure)return 0;
 for(int i=0;i<count;++i) { frames[i].m_pBase=pixels[i];frames[i].m_FrameType=-1; }
 return 1;
}
static void FlushCache(int mode) { assert(mode==0); }
static void IPU_FRST(void) {}
""" + function("_wait_frame") + function("_init_cb") + function("MPEG12_Decode") + r"""
static MPEGSequenceInfo info={640,480,33};
static SMS_RingBuffer input,output;
static SMS_CodecContext codec;
static void reset(void) {
 memset(&s_MPEGState,0,sizeof(s_MPEGState));memset(holds,0,sizeof(holds));
 s_MPEGState.m_pInfo=&info;
 queued=NULL;yields=pictures=posted=allocations=destroys=0;
 release_after=3;picture_result=1;frame_failure=freed_packets=codec.m_HardwareError=0;
 for(int i=0;i<3;++i) {
  s_MPEGState.m_Pics[i].m_pBase=pixels[i];s_MPEGState.m_Pics[i].m_FrameType=-1;
 }
}
static void busy(void) {
 reset();s_MPEGState.m_Pics[0].m_FrameType=0;
 assert(MPEG12_Decode(&codec,&output,&input)==1);
 assert(yields==3 && pictures==1 && posted==1 && allocations==1);
 assert(queued==&s_MPEGState.m_Pics[0] && queued->m_FrameType==0 && queued->m_StartPTS==100);
 assert(s_MPEGState.m_PicIdx==1);
}
static void resize(void) {
 reset();
 for(int i=0;i<3;++i) s_MPEGState.m_Pics[i].m_FrameType=0;
 assert(_init_cb(&s_MPEGState,&info)==pixels[0]);
 assert(yields==9 && destroys==1 && s_MPEGState.m_Width==640 && s_MPEGState.m_Height==480);
 for(int i=0;i<3;++i) assert(s_MPEGState.m_Pics[i].m_FrameType<0);
}
static void normal(void) {
 reset();assert(MPEG12_Decode(&codec,&output,&input)==1 && !yields);
 reset();picture_result=0;
 assert(MPEG12_Decode(&codec,&output,&input)==0 && !posted && !allocations && !s_MPEGState.m_PicIdx);
 reset();s_MPEGState.m_PicIdx=2;
 assert(MPEG12_Decode(&codec,&output,&input)==1 && !s_MPEGState.m_PicIdx);
 reset();s_MPEGState.m_Width=640;s_MPEGState.m_Height=480;
 s_MPEGState.m_Pics[0].m_FrameType=0;
 assert(_init_cb(&s_MPEGState,&info)==pixels[0] && !destroys && !yields);
 reset();picture_result=0;
 for(int i=0;i<3;++i) { s_MPEGState.m_Pics[i].m_pBase=NULL;s_MPEGState.m_Pics[i].m_FrameType=0; }
 assert(MPEG12_Decode(&codec,&output,&input)==0 && !yields);
 assert(_init_cb(&s_MPEGState,&info)==pixels[0] && !yields && destroys==1);
}
static void failure(void) {
 reset();frame_failure=1;
 assert(!_init_cb(&s_MPEGState,&info));
 assert(s_MPEGState.m_AllocationFailed && !s_MPEGState.m_Width && !s_MPEGState.m_Height);
 SMS_AVPacket packet={16};s_MPEGState.m_pPacket=&packet;picture_result=-1;
 assert(!MPEG12_Decode(&codec,&output,&input));
 assert(codec.m_HardwareError && freed_packets==1 && !s_MPEGState.m_pPacket && !posted);
 reset();picture_result=-1;
 assert(!MPEG12_Decode(&codec,&output,&input) && codec.m_HardwareError && !posted);
 unsigned previous=pictures;
 assert(!MPEG12_Decode(&codec,&output,&input) && pictures==previous && !posted);
}
int main(int argc,char **argv) {
 const char *which=argc>1?argv[1]:"all";
 if(!strcmp(which,"all") || !strcmp(which,"busy")) busy();
 if(!strcmp(which,"all") || !strcmp(which,"resize")) resize();
 if(!strcmp(which,"all") || !strcmp(which,"normal")) normal();
 if(!strcmp(which,"all") || !strcmp(which,"failure")) failure();
 return 0;
}
"""
with tempfile.TemporaryDirectory() as temp:
    c, exe = Path(temp) / "frames.c", Path(temp) / "frames"
    c.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe), args.case], check=True, timeout=5)
print("MPEG-1/2 frames: delayed renderer, resize drain, first allocation, EOF and pool wrap passed")
