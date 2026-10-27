"""Sanitized host tests of the actual MPEG-PS parser with memory-backed I/O.

Codec/DMA decoding is stubbed. This checks probing, packet boundaries and
failure handling, not PS2 decoding, device throughput or reporter acceptance.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/SMS_ContainerMPEG_PS.c").read_text()
source = re.sub(r'^#include "[^"]+"\s*$', '', source, flags=re.M)

prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef int64_t s64;
#define SMS_MAX_STREAMS 8
#define SMS_NOPTS_VALUE INT64_MIN
#define SMS_TIME_BASE 1000000
#define SMS_INT64(x) x##LL
#define SMS_SYNTH_BUFFER NULL
#define SMS_MKTAG(a,b,c,d) 0
#define SMS_STRM_FLAGS_AUDIO 1
#define SMS_CONT_FLAGS_SEEKABLE 1
#define AC3_LFE 16
#define SMS_bswap32 __builtin_bswap32
enum SMS_CodecType { SMS_CodecTypeUnknown, SMS_CodecTypeVideo, SMS_CodecTypeAudio };
enum { SMS_CodecID_NULL, SMS_CodecID_MP2, SMS_CodecID_MP3, SMS_CodecID_AC3,
       SMS_CodecID_DTS, SMS_CodecID_PCM16BE, SMS_CodecID_MPEG1, SMS_CodecID_MPEG2 };
typedef struct { int m_Type, m_ID, m_Tag, m_Width, m_Height, m_fWS;
                 int m_SampleRate, m_BitRate, m_Channels; } SMS_CodecContext;
typedef struct { SMS_CodecContext *m_pCodec; int m_SampleRate; unsigned m_ID, m_Flags;
                 void *m_pPktBuf; } SMS_Stream;
typedef struct { unsigned char *m_pData; int64_t m_PTS, m_DTS; int m_StmIdx; } SMS_AVPacket;
typedef struct FileContext FileContext;
struct FileContext {
 unsigned m_Pos, m_CurPos, m_Size;
 int (*Read)(FileContext *, void *, unsigned);
 int (*Seek)(FileContext *, unsigned);
};
typedef struct SMS_Container SMS_Container;
struct SMS_Container {
 SMS_Stream *m_pStm[SMS_MAX_STREAMS];
 unsigned m_nStm, m_Flags;
 FileContext *m_pFileCtx;
 void *m_pCtx;
 char *m_pName;
 int64_t m_Duration, m_StartTime;
 int (*ReadPacket)(SMS_Container *, int *);
 int (*Seek)(SMS_Container *, int, int, uint32_t);
 SMS_AVPacket *(*AllocPacket)(void *, int);
};
typedef struct { int m_FreeFmtFrameSize, m_SampleRate, m_BitRate, m_nChannels; } SMS_MPAInfo;
static int g_AC3Channels[8] = {2,1,2,3,3,4,4,5};
static unsigned char *g_pSynthBuffer;
static char g_pMPEGPS[] = "MPEG-PS";
static unsigned char input[200000];
static unsigned input_size, read_limit = ~0U, read_calls;
static int read_error, fail_alloc, fail_malloc, calloc_calls, fail_calloc_at;
static unsigned char *read_buffer;
static unsigned read_size;
static void *test_malloc(size_t n) { return fail_alloc || fail_malloc ? NULL : malloc(n); }
static void *test_calloc(size_t n, size_t s) {
 return ++calloc_calls == fail_calloc_at || fail_alloc ? NULL : calloc(n,s);
}
static int read_file(FileContext *ctx, void *p, unsigned n) {
 unsigned left = ctx->m_CurPos < input_size ? input_size - ctx->m_CurPos : 0;
 assert(++read_calls < 100000);
 if (read_error) return -1;
 if(n > left) n = left;
 if(n > read_limit) n = read_limit;
 memcpy(p, input + ctx->m_CurPos, n);
 ctx->m_CurPos += n;
 read_buffer = p; read_size = n;
 return n;
}
static int seek_file(FileContext *ctx, unsigned n) { ctx->m_CurPos = n; return n; }
#define FILE_EOF(f) ((f)->m_CurPos >= (f)->m_Size)
static unsigned File_GetByte(FileContext *f) { unsigned char b=0; read_file(f,&b,1); return b; }
static int File_GetShortBE(FileContext *f) { unsigned a=File_GetByte(f); return (a<<8)|File_GetByte(f); }
static unsigned File_GetUInt(FileContext *f) { unsigned v=0; read_file(f,&v,4); return v; }
static void File_Skip(FileContext *f, unsigned n) {
 unsigned char tmp[512];
 while(n) { unsigned k=n>512?512:n; int got=read_file(f,tmp,k); if(got<=0) break; n-=got; }
}
static uint32_t SMS_unaligned32(const void *p) { uint32_t v; memcpy(&v,p,4); return v; }
static void check_header(const unsigned char *p, unsigned n) {
 assert(p >= read_buffer && p+n <= read_buffer+read_size);
}
static int MP123_CheckHeader(uint32_t h) { return h == 0xfffb9000; }
static int MP123_DecodeHeader(uint32_t h, SMS_MPAInfo *i) {
 (void)h; i->m_SampleRate=44100; i->m_BitRate=128000; i->m_nChannels=2; return 0;
}
static int AC3_SyncInfo(unsigned char *p, int *flags, int *rate, int *bitrate) {
 check_header(p,7);
 if(p[0]!=0x0b || p[1]!=0x77) return 0;
 *flags=2; *rate=48000; *bitrate=192000; return 768;
}
static int DTS_SyncInfo(unsigned char *p, int *flags, int *rate, int *bitrate, int *len) {
 check_header(p,16);
 if(p[0]!=0x7f || p[1]!=0xfe) return 0;
 *flags=2; *rate=48000; *bitrate=768000; *len=512; return 1024;
}
static int DTS_Channels(int f) { (void)f; return 2; }
static void SMSContainer_SetPTSInfo(SMS_Stream *s, int a, int b) { (void)s;(void)a;(void)b; }
static void SMSContainer_CalcPktFields(SMS_Stream *s, SMS_AVPacket *p) { (void)s;(void)p; }
static int64_t SMS_Rescale(int64_t a, int64_t b, int64_t c) { (void)b; return a/c; }
static void SMS_DestroyContainer(SMS_Container *c, int all) {
 unsigned i; (void)all;
 for(i=0;i<c->m_nStm;++i) if(c->m_pStm[i]) {
  free(c->m_pStm[i]->m_pCodec); free(c->m_pStm[i]); c->m_pStm[i]=NULL;
 }
 free(c->m_pCtx); c->m_pCtx=NULL;
}
#define malloc test_malloc
#define calloc test_calloc
"""

tests = r"""
#undef malloc
#undef calloc
static FileContext file;
static void reset(unsigned n) {
 memset(input,0,sizeof(input)); input_size=n;
 file=(FileContext){4096,0,n,read_file,seek_file};
 read_limit=~0U; read_calls=0; read_error=fail_alloc=fail_malloc=calloc_calls=fail_calloc_at=0;
}
static void test_audio(void) {
 SMS_CodecContext codec={0}; SMS_Stream stream={0};
 const int ids[]={SMS_CodecID_MP2,SMS_CodecID_MP3,SMS_CodecID_AC3,SMS_CodecID_DTS,SMS_CodecID_PCM16BE};
 const unsigned sizes[]={4,4,7,16,3};
 unsigned i,n;
 stream.m_pCodec=&codec;
 for(i=0;i<5;++i) {
  codec.m_ID=ids[i];
  for(n=0;n<24;++n) {
   reset(100+n); file.m_CurPos=100;
   assert(_fill_audio_parameters(&file,&stream,n)==(ids[i]==SMS_CodecID_PCM16BE && n>=3));
   assert(file.m_CurPos==100);
  }
  /* A header ending exactly at the payload boundary must be accepted. */
  reset(103+sizes[i]); file.m_CurPos=100;
  if(i<2) { input[103]=0xff;input[104]=0xfb;input[105]=0x90; }
  if(i==2) { input[103]=0x0b;input[104]=0x77; }
  if(i==3) { input[103]=0x7f;input[104]=0xfe; }
  assert(_fill_audio_parameters(&file,&stream,3+sizes[i])==1);
  assert(file.m_CurPos==100);
  for(n=0;n<sizes[i];++n) {
   reset(100+sizes[i]);file.m_CurPos=100;read_limit=n;
   assert(_fill_audio_parameters(&file,&stream,sizes[i])==0);
   assert(file.m_CurPos==100);
  }
 }
 reset(100);file.m_CurPos=10;codec.m_ID=SMS_CodecID_AC3;
 read_error=1; assert(_fill_audio_parameters(&file,&stream,90)==0 && file.m_CurPos==10);
 fail_alloc=1; assert(_fill_audio_parameters(&file,&stream,90)==0 && file.m_CurPos==10);
 assert(_fill_audio_parameters(&file,&stream,-1)==0);
 assert(_fill_audio_parameters(&file,&stream,65536)==0);
 reset(65535); assert(_fill_audio_parameters(&file,&stream,65535)==0 && file.m_CurPos==0);
}
static void test_video(void) {
 SMS_CodecContext codec={0}; SMS_Stream stream={0}; SMS_Container c={0}; unsigned n;
 c.m_pFileCtx=&file;stream.m_pCodec=&codec;
 for(n=0;n<=4;++n) {
  reset(4+n);
  memcpy(input,"\0\0\1\xb3\x28\x01\xe0\x34",4+n);
  assert(_fill_video_parameters(&c,&stream,2000,0x1e0)==(n==4));
  assert(file.m_CurPos==0);
 }
 assert(codec.m_Width==640 && codec.m_Height==480 && codec.m_fWS==1);
}
static SMS_AVPacket *packet_alloc(void *unused,int n) {
 static unsigned char data[16];static SMS_AVPacket packet;
 (void)unused;assert(n<=16);packet.m_pData=data;return &packet;
}
static void pes(unsigned sid, const unsigned char *p, unsigned n) {
 unsigned at=input_size;
 assert(at+n+7 < sizeof(input));
 input[at]=0; input[at+1]=0; input[at+2]=1; input[at+3]=sid;
 input[at+4]=(n+1)>>8; input[at+5]=n+1; input[at+6]=0x0f;
 memcpy(input+at+7,p,n); input_size+=n+7;
}
static void test_probe_and_packets(void) {
 const unsigned char video[]={0,0,1,0xb3,0x28,0x01,0xe0,0x34};
 const unsigned char ac3[]={0x80,1,0,0,0x0b,0x77,0,0,0,0x40,0x40};
 SMS_Container c={0}; _MPEGPSContainer private={0}; int idx=0;
 SMS_CodecContext codec={0};SMS_Stream stream={0};
 reset(0);pes(0xe0,video,sizeof(video));pes(0xbd,ac3,sizeof(ac3));file.m_Size=input_size;
 c.m_pFileCtx=&file;
 assert(SMS_GetContainerMPEG_PS(&c)==1);
 assert(c.m_nStm==2 && c.m_pStm[1]->m_pCodec->m_SampleRate==48000);
 SMS_DestroyContainer(&c,0);
 /* Inject failure into each context/stream/codec allocation. */
 for(int failure=1;failure<=6;++failure) {
  memset(&c,0,sizeof(c));c.m_pFileCtx=&file;reset(0);
  pes(0xe0,video,sizeof(video));pes(0xbd,ac3,sizeof(ac3));file.m_Size=input_size;
  if(failure==6) fail_malloc=1; else fail_calloc_at=failure;
  assert(SMS_GetContainerMPEG_PS(&c)==0 && c.m_pCtx==NULL && c.m_nStm==0);
 }
 /* Valid stream headers followed by an I/O stall before advertised EOF. */
 memset(&c,0,sizeof(c));c.m_pFileCtx=&file;c.m_pCtx=&private;c.m_Duration=1000;
 reset(0);file.m_Size=100;
 assert(_ReadPacket(&c,&idx)<0);
 assert(_Seek(&c,0,0,0)==0);
 /* A declared video payload cut short before its boundary must stop seeking. */
 reset(0);pes(0xe0,(const unsigned char *)"x",1);
 input[5]=101;file.m_Size=1000;
 codec.m_Type=SMS_CodecTypeVideo;stream.m_pCodec=&codec;stream.m_ID=0x1e0;stream.m_pPktBuf=&codec;
 c.m_pStm[0]=&stream;c.m_nStm=1;
 assert(_Seek(&c,0,0,0)==0);
 /* Flush the buffered alignment tail once, with defined timestamps. */
 reset(0);file.m_Size=1000;c.AllocPacket=packet_alloc;
 private.m_BufLen=3;memcpy(private.m_Buff,"abc",3);
 assert(_ReadPacket(&c,&idx)==3);
 assert(private.m_BufLen==0 && _ReadPacket(&c,&idx)<0);
 /* Reject overfull sets without running off NULL-terminated stream arrays. */
 memset(&c,0,sizeof(c));c.m_pFileCtx=&file;reset(0);
 for(unsigned i=0;i<16;++i) pes(0xbd,(unsigned char[]){0x80+i,1,0,0,0x0b,0x77,0,0,0,0x40,0x40},11);
 file.m_Size=input_size;
 assert(SMS_GetContainerMPEG_PS(&c)==0);
 assert(c.m_pCtx==NULL && c.m_nStm==0);
 SMS_DestroyContainer(&c,0);
 memset(&c,0,sizeof(c));c.m_pFileCtx=&file;reset(0);pes(0xe0,video,sizeof(video));
 for(unsigned i=0;i<8;++i) pes(0xbd,(unsigned char[]){0x80+i,1,0,0,0x0b,0x77,0,0,0,0x40,0x40},11);
 file.m_Size=input_size;
 assert(SMS_GetContainerMPEG_PS(&c)==1 && c.m_nStm==7 && c.m_pStm[7]==NULL);
 SMS_DestroyContainer(&c,0);
 memset(&c,0,sizeof(c));c.m_pFileCtx=&file;fail_alloc=1;
 assert(SMS_GetContainerMPEG_PS(&c)==0 && c.m_pCtx==NULL);
}
int main(void) {
 test_audio();test_video();test_probe_and_packets();
 puts("MPEG-PS: cursor restoration, bounded headers, short/error reads, stream limits and stalled packets passed");
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    cfile = Path(tmp) / "probe.c"
    exe = Path(tmp) / "probe"
    cfile.write_text(prefix + source + tests)
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu99", "-g", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    str(cfile), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=20)
