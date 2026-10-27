/* Real codec/ring/IPU control with a deliberately delayed RGB consumer.
 * Replaces GS upload with a 1,000 H-SYNC tick hold and captures the frame. */
#include <kernel.h>
#include <tamtypes.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "SMS_IPU.h"
#include "../../src/SMS_MPEG12.c"

#ifndef COLOR16
#define COLOR16 0
#endif
extern unsigned char probe_m2v[];
extern unsigned int size_probe_m2v;
SMS_MPEGContext g_MPEGCtx;
IPUContext g_IPUCtx;
char g_pMPEG12[] = "MPEG-1/2 pool control";

static SMS_RingBuffer *input_queue, *output_queue;
static SMS_CodecContext codec;
static unsigned char output_storage[192] __attribute__((aligned(64)));
static unsigned frame_bytes, overwrites, changed, produced;
static int producer_done;
static unsigned char stack[16384] __attribute__((aligned(16)));
static int (*real_picture)(void *,s64 *);

void SMS_FrameBufferInit(SMS_FrameBuffer *frames,int count,int width,int height,int depth) {
 int i;
 width=(width+15)&~15;height=(height+15)&~15;
 frame_bytes=width*height*(depth?2:4);
 for(i=0;i<count;++i) {
  frames[i].m_pBase=memalign(128,frame_bytes);
  if(!frames[i].m_pBase) { printf("[sms-pool] allocation failure\n");fflush(stdout);SleepThread(); }
  frames[i].m_pData=NULL;frames[i].m_FrameType=-1;
 }
 printf("[sms-pool] sequence=%dx%d mode=%d bytes=%u\n",width,height,depth?16:32,frame_bytes);
 fflush(stdout);
}

void SMS_FrameBufferDestroy(SMS_FrameBuffer *frames,int count) {
 int i;
 for(i=0;i<count;++i) { free(frames[i].m_pBase);frames[i].m_pBase=NULL; }
}

static int picture(void *base,s64 *pts) {
 int i, result;
 for(i=0;i<3;++i)
  if(base && base==s_MPEGState.m_Pics[i].m_pBase && s_MPEGState.m_Pics[i].m_FrameType>=0)
   ++overwrites;
 result=real_picture(base,pts);
 if(MPEG_Picture!=picture) real_picture=MPEG_Picture;
 MPEG_Picture=picture;
 return result;
}

static unsigned checksum(const void *data,unsigned size) {
 const unsigned char *bytes=(const unsigned char *)((unsigned)data|0x20000000);
 unsigned hash=2166136261U,i;
 for(i=0;i<size;++i) hash=(hash^bytes[i])*16777619U;
 return hash;
}

static void wake_consumer(s32 alarm_id,u16 ticks,void *thread_id) {
 (void)alarm_id;(void)ticks;iWakeupThread((int)thread_id);
}

static void producer(void *unused) {
 int i;(void)unused;
 for(i=0;i<5;++i) {
  if(!MPEG12_Decode(&codec,output_queue,input_queue)) break;
  ++produced;RotateThreadReadyQueue(SMS_THREAD_PRIORITY);
 }
 SignalSema(producer_done);
 SleepThread();
}

int main(void) {
 ee_thread_t thread;
 ee_sema_t semaphore;
 SMS_AVPacket *packet;
 unsigned input_bytes=(size_probe_m2v+64+63)&~63;
 void *input_storage;
 int thread_id,frames=0;
 FILE *out;
 if(!size_probe_m2v || size_probe_m2v>65535*16) {
  printf("[sms-pool] exit=2 invalid input\n");fflush(stdout);SleepThread();
 }
 input_storage=memalign(64,input_bytes);
 if(!input_storage) { printf("[sms-pool] exit=2 input allocation\n");fflush(stdout);SleepThread(); }
 memset(input_storage,0,input_bytes);
 input_queue=SMS_RingBufferInit(input_storage,input_bytes);
 output_queue=SMS_RingBufferInit(output_storage,sizeof(output_storage));
 packet=SMS_RingBufferAlloc(input_queue,size_probe_m2v+64);
 packet->m_Size=size_probe_m2v;packet->m_pData=(unsigned char *)packet+64;
 packet->m_PTS=packet->m_DTS=0;
 memcpy(packet->m_pData,probe_m2v,size_probe_m2v);
 FlushCache(0);SMS_RingBufferPost(input_queue);
 memset(&codec,0,sizeof(codec));
 SMS_Codec_MPEG12_Open(&codec);MPEG12_Init(&codec);
 g_IPUCtx.m_TexFmt=COLOR16?GSPixelFormat_PSMCT16:GSPixelFormat_PSMCT24;
 _hwctl(&codec,SMS_HWC_Init);
 real_picture=MPEG_Picture;MPEG_Picture=picture;
 memset(&semaphore,0,sizeof(semaphore));semaphore.max_count=1;
 producer_done=CreateSema(&semaphore);
 memset(&thread,0,sizeof(thread));thread.func=producer;thread.stack=stack;
 thread.stack_size=sizeof(stack);thread.initial_priority=SMS_THREAD_PRIORITY;thread.gp_reg=&_gp;
 thread_id=CreateThread(&thread);
 out=fopen(COLOR16?"host:decoded16.rgb16":"host:decoded.rgba","wb");
 if(!out || thread_id<0) { printf("[sms-pool] exit=2 setup\n");fflush(stdout);SleepThread(); }
 if(ChangeThreadPriority(GetThreadId(),SMS_THREAD_PRIORITY)<0) {
  printf("[sms-pool] exit=2 consumer priority\n");fflush(stdout);SleepThread();
 }
 StartThread(thread_id,NULL);
 while(frames<5) {
  SMS_FrameBuffer *frame=*(SMS_FrameBuffer **)SMS_RingBufferWait(output_queue);
  unsigned before=checksum(frame->m_pBase,frame_bytes);
  if(SetAlarm(1000,wake_consumer,(void *)GetThreadId())<0) {
   printf("[sms-pool] exit=2 alarm setup\n");fflush(stdout);SleepThread();
  }
  SleepThread();
  if(checksum(frame->m_pBase,frame_bytes)!=before) ++changed;
  if(fwrite((void *)((unsigned)frame->m_pBase|0x20000000),1,frame_bytes,out)!=frame_bytes) break;
  frame->m_FrameType=-1;
  SMS_RingBufferFree(output_queue,4);
  ++frames;
 }
 fclose(out);WaitSema(producer_done);TerminateThread(thread_id);DeleteThread(thread_id);
 MPEG12_Destroy(&codec);free(codec.m_pCodec);
 printf("[sms-pool] exit=%d frames=%d produced=%u overwrites=%u changed=%u\n",
        frames==5 && !overwrites && !changed?0:1,frames,produced,overwrites,changed);
 fflush(stdout);SleepThread();
 return 0;
}
