"""Actual MOV index/table lifetime and allocation-failure regressions; no decoder."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path)
parser.add_argument("--case", default="all")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (args.source or root / "src/SMS_ContainerMOV.c").read_text()


def extract(name):
    marker = "}  /* end " + name + " */"
    end = source.index(marker) + len(marker)
    start = source.rfind("\nstatic ", 0, source.index(marker)) + 1
    if name == "SMS_M4AProbe":
        start = source.index("uint64_t SMS_M4AProbe")
    return source[start:end]


types = source[source.index("typedef struct MOVIndex"):source.index("static uint32_t _read_int_be")]
free_tables = extract("_mov_free_tables") if "end _mov_free_tables" in source else ""
prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define SMS_MAX_STREAMS 8
#define SMS_NOPTS_VALUE INT64_MIN
#define SMS_MAXINT64 INT64_MAX
#define SMS_CONT_FLAGS_SEEKABLE 1
enum { SMS_CodecTypeUnknown, SMS_CodecTypeVideo, SMS_CodecTypeAudio };
typedef struct { int m_Type,m_FrameSize; } SMS_CodecContext;
typedef struct SMS_Stream SMS_Stream;
struct SMS_Stream { void *m_pCtx,*m_pPktBuf; SMS_CodecContext *m_pCodec;
 int64_t m_Duration; void (*Destroy)(SMS_Stream *); };
typedef struct FileContext { unsigned m_Size,m_CurPos;
 int (*Seek)(struct FileContext *,unsigned); } FileContext;
typedef struct SMS_Container SMS_Container;
struct SMS_Container {
 SMS_Stream *m_pStm[8]; unsigned m_nStm,m_Flags; void *m_pCtx;
 FileContext *m_pFileCtx; char *m_pName;
 int (*ReadPacket)(SMS_Container *,int *);
 int (*Seek)(SMS_Container *,int,int,uint32_t);
};
static int fail_realloc, realloc_calls, fail_calloc, calloc_calls;
static void *test_realloc(void *p,size_t n) {
 if (++realloc_calls == fail_realloc) return NULL;
 return realloc(p,n);
}
static void *test_calloc(size_t n,size_t s) {
 if (++calloc_calls == fail_calloc) return NULL;
 return calloc(n,s);
}
/* Match SMS_Realloc's size publication, including its failure behavior. */
static void *SMS_Realloc(void *p,unsigned *size,unsigned minimum) {
 if (minimum < *size) return p;
 *size = 17 * minimum / 16 + 32;
 return test_realloc(p,*size);
}
#define realloc test_realloc
#define calloc test_calloc
static void _free(void *p) { free(p); }
static int seek_file(FileContext *f,unsigned n) { f->m_CurPos=n;return n; }
static void File_Skip(FileContext *f,unsigned n) { f->Seek(f,f->m_CurPos+n); }
static void SMSContainer_SetPTSInfo(SMS_Stream *s,int n,int d) { (void)s;assert(n>0 && d>0); }
static int64_t SMS_Rescale(int64_t a,int64_t b,int64_t c) { assert(c>0);return a*b/c; }
static uint32_t _mov_probe(FileContext *f) { (void)f;return 1; }
static int _ReadPacket(SMS_Container *c,int *i) { (void)c;(void)i;return -1; }
static int _Seek(SMS_Container *c,int i,int d,uint32_t p) { (void)c;(void)i;(void)d;(void)p;return 0; }
static char g_pMOV[]="MOV";
""" + types + r"""
static int _mov_read_dflt(MOVContext *c,FileContext *f,MOVAtom *atom) {
 (void)f;(void)atom;
 c->m_fMOOV=c->m_fMDAT=1;c->m_TimeScale=1;
 if(!c->m_pMDAT) {
  c->m_pMDAT=malloc(sizeof(MOVMDat));assert(c->m_pMDAT);
  c->m_pMDAT[0]=(MOVMDat){0,1000};c->m_nMDAT=1;
 }
 return 1;
}
"""
functions = free_tables + extract("_DestroyStream") + extract("_mov_read_trak") + extract("_mov_read_mdat")
functions += extract("_gcd") + extract("_add_idx_entry") + extract("_mov_build_index")
if "end _mov_edit_fits" in source:
    functions += extract("_mov_edit_fits")
functions += extract("_mov_read_header")
cleanup = r"""
static void SMS_DestroyContainer(SMS_Container *c,int all) {
 (void)all;
 for(unsigned i=0;i<c->m_nStm;++i) {
  SMS_Stream *s=c->m_pStm[i];if(!s) continue;
  s->Destroy(s);free(s->m_pCtx);free(s->m_pCodec);free(s);c->m_pStm[i]=NULL;
 }
 free(c->m_pCtx);c->m_pCtx=NULL;
}
"""
probe_fixture = r"""
typedef struct { int m_SampleRate,m_nChannels,m_BitRate; } SMS_AudioInfo;
static MOVParseEntry s_M4AProbeTbl[1];
static int probe_header(SMS_Container *c) {
 MOVContext *m=c->m_pCtx;assert(m->m_pBase==c && !c->m_nStm && !c->m_pName);
 for(int i=0;i<SMS_MAX_STREAMS;++i) assert(!c->m_pStm[i]);
 SMS_AudioInfo *info=(SMS_AudioInfo *)m->m_pMDAT;
 info->m_SampleRate=48000;info->m_nChannels=2;info->m_BitRate=128000;
 return 0;
}
""" + extract("SMS_M4AProbe").replace("_mov_read_header (", "probe_header (")
tests = r"""
static SMS_Container container;
static FileContext file;
static MOVContext context;
static void reset(void) {
 memset(&container,0,sizeof(container));memset(&context,0,sizeof(context));
 file=(FileContext){1000,0,seek_file};container.m_pFileCtx=&file;
 container.m_pCtx=&context;context.m_pBase=&container;
 fail_realloc=realloc_calls=fail_calloc=calloc_calls=0;
}
static SMS_Stream *make_stream(int type,int chunks) {
 SMS_Stream *s=calloc(1,sizeof(*s));MOVStream *m=calloc(1,sizeof(*m));
 s->m_pCodec=calloc(1,sizeof(SMS_CodecContext));s->m_pCodec->m_Type=type;
 s->m_pCtx=m;s->m_Duration=SMS_NOPTS_VALUE;s->Destroy=_DestroyStream;
 m->m_TimeRate=m->m_TimeScale=1;m->m_nSamples=chunks;m->m_SampleSize=type==SMS_CodecTypeVideo?4:1;
 m->m_nChunks=chunks;m->m_pChunkOffsets=calloc(chunks,sizeof(int64_t));
 for(int i=0;i<chunks;++i) m->m_pChunkOffsets[i]=100+100*i;
 m->m_nSample2Chunk=1;m->m_pSample2Chunk=malloc(sizeof(MOVStsc));
 m->m_pSample2Chunk[0]=(MOVStsc){1,1,1};
 m->m_nStts=1;m->m_pStts=malloc(sizeof(MOVStts));m->m_pStts[0]=(MOVStts){chunks,10};
 container.m_pStm[container.m_nStm++]=s;
 return s;
}
static void destroy_fixture(void) {
 for(unsigned i=0;i<container.m_nStm;++i) {
  SMS_Stream *s=container.m_pStm[i];s->Destroy(s);free(s->m_pCtx);free(s->m_pCodec);free(s);
 }
 free(context.m_pMDAT);
}
static void lifetime(void) {
 reset();make_stream(SMS_CodecTypeVideo,1);
 MOVStream *audio=make_stream(SMS_CodecTypeAudio,1)->m_pCtx;audio->m_pChunkOffsets[0]=800;
 assert(_mov_read_header(&container)==1);
 assert(audio->m_nIdx==1 && audio->m_pIdx[0].m_Size==200);
 for(unsigned i=0;i<container.m_nStm;++i) {
  MOVStream *m=container.m_pStm[i]->m_pCtx;
  assert(!m->m_pChunkOffsets && !m->m_pStts && !m->m_pSample2Chunk);
 }
 assert(!context.m_pMDAT);destroy_fixture();
}
static void timing(void) {
 reset();SMS_Stream *s=make_stream(SMS_CodecTypeAudio,2);MOVStream *m=s->m_pCtx;
 m->m_SampleSize=2;m->m_nSamples=10;m->m_pSample2Chunk[0].m_Count=5;
 free(m->m_pStts);m->m_nStts=3;m->m_pStts=malloc(3*sizeof(MOVStts));
 m->m_pStts[0]=(MOVStts){2,10};m->m_pStts[1]=(MOVStts){3,20};m->m_pStts[2]=(MOVStts){5,30};
 _mov_build_index(&container,m,s);
 assert(m->m_nIdx==2 && m->m_pIdx[1].m_DTS==80);destroy_fixture();
}
static void timing_coverage(void) {
 SMS_Stream *s;MOVStream *m;
 /* Every fixed-size audio chunk needs timing for all of its samples. */
 for(int covered=0;covered<4;++covered) {
  reset();s=make_stream(SMS_CodecTypeAudio,2);m=s->m_pCtx;
  m->m_SampleSize=2;m->m_nSamples=4;m->m_pSample2Chunk[0].m_Count=2;
  m->m_pStts[0].m_Count=covered;
  assert(_mov_build_index(&container,m,s)==0);destroy_fixture();
 }
 /* A zero-count entry is harmless if later entries cover the entire chunk. */
 reset();s=make_stream(SMS_CodecTypeAudio,2);m=s->m_pCtx;
 m->m_SampleSize=2;free(m->m_pStts);m->m_nStts=2;m->m_pStts=calloc(2,sizeof(MOVStts));
 m->m_pStts[1]=(MOVStts){2,20};
 assert(_mov_build_index(&container,m,s)==1 && m->m_pIdx[1].m_DTS==20);destroy_fixture();
}
static void sample_timing_coverage(void) {
 SMS_Stream *s;MOVStream *m;
 /* The per-sample path must not reuse an exhausted final timing entry. */
 reset();s=make_stream(SMS_CodecTypeVideo,2);m=s->m_pCtx;m->m_pStts[0].m_Count=1;
 assert(_mov_build_index(&container,m,s)==0);destroy_fixture();
 reset();s=make_stream(SMS_CodecTypeVideo,2);m=s->m_pCtx;
 free(m->m_pStts);m->m_nStts=2;m->m_pStts=calloc(2,sizeof(MOVStts));
 m->m_pStts[0].m_Duration=10;m->m_pStts[1]=(MOVStts){2,20};
 assert(_mov_build_index(&container,m,s)==1 && m->m_pIdx[1].m_DTS==20);destroy_fixture();
}
static void timing_probe_cleanup(void) {
 SMS_Stream *s;MOVStream *m;
 /* Reject through the public probe path, including cleanup of a prior index. */
 for(int prior=0;prior<=1;++prior) {
  reset();if(prior) make_stream(SMS_CodecTypeVideo,1);
  s=make_stream(SMS_CodecTypeAudio,2);m=s->m_pCtx;
  m->m_SampleSize=2;m->m_pStts[0].m_Count=1;
  assert(_GetContainerMOV(&container,NULL)==0 && !container.m_nStm && !container.m_pCtx);
 }
}
static void allocations(void) {
 MOVAtom atom={0,0,100};MOVStream index={0};MOVMDat *old;
 reset();fail_realloc=1;_add_idx_entry(&index,1,0,0,4);
 assert(!index.m_pIdx && !index.m_nIdx && !index.m_IdxAllocSize);
 fail_realloc=0;for(int i=0;i<3;++i) _add_idx_entry(&index,1,i,4*i,4);
 MOVIndex *saved=index.m_pIdx;unsigned size=index.m_IdxAllocSize;
 fail_realloc=realloc_calls+1;_add_idx_entry(&index,1,3,12,4);
 assert(index.m_pIdx==saved && index.m_nIdx==3 && index.m_IdxAllocSize==size);free(saved);
 reset();context.m_pMDAT=old=malloc(sizeof(MOVMDat));context.m_nMDAT=1;
 old[0]=(MOVMDat){10,20};fail_realloc=1;
 assert(_mov_read_mdat(&context,&file,&atom)==-1);
 assert(context.m_pMDAT==old && context.m_nMDAT==1 && old[0].m_Offset==10);free(old);
 reset();fail_calloc=1;
 assert(_GetContainerMOV(&container,NULL)==0 && container.m_pCtx==&context);
 for(int fail=1;fail<=2;++fail) {
  reset();fail_calloc=fail;
  assert(_mov_read_trak(&context,&file,&atom)==-1 && !container.m_nStm);
 }
 for(int fail=1;fail<=2;++fail) {
  reset();make_stream(SMS_CodecTypeVideo,1);make_stream(SMS_CodecTypeAudio,1);
  fail_realloc=fail;
  assert(_GetContainerMOV(&container,NULL)==0 && !container.m_pCtx && !container.m_nStm);
 }
}
static void stream_limit(void) {
 reset();MOVAtom atom={0,0,100};container.m_nStm=7;
 assert(_mov_read_trak(&context,&file,&atom)==0);
 assert(container.m_nStm==7 && !container.m_pStm[7] && file.m_CurPos==100 && !calloc_calls);
}
static void audio_probe(void) {
 reset();SMS_AudioInfo info={0};file.m_CurPos=20;
 assert(SMS_M4AProbe(&file,&info)==1 && info.m_SampleRate==48000 && !file.m_CurPos);
}
int main(int argc,char **argv) {
 const char *name=argc>1?argv[1]:"all";
 if(!strcmp(name,"all")||!strcmp(name,"lifetime")) lifetime();
 if(!strcmp(name,"all")||!strcmp(name,"timing")) timing();
 if(!strcmp(name,"all")||!strcmp(name,"timing-coverage")) timing_coverage();
 if(!strcmp(name,"all")||!strcmp(name,"timing-coverage")||!strcmp(name,"sample-timing")) sample_timing_coverage();
 if(!strcmp(name,"all")||!strcmp(name,"timing-coverage")||!strcmp(name,"timing-cleanup")) timing_probe_cleanup();
 if(!strcmp(name,"all")||!strcmp(name,"allocations")) allocations();
 if(!strcmp(name,"all")||!strcmp(name,"limit")) stream_limit();
 if(!strcmp(name,"all")||!strcmp(name,"probe")) audio_probe();
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / "mov.c", Path(tmp) / "mov"
    src.write_text(prefix + functions + cleanup + extract("_GetContainerMOV") + probe_fixture + tests)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe), args.case], check=True, timeout=5)
print("MOV index: cross-stream table lifetime, chunk timing, allocation failure/recovery and stream sentinel passed")
