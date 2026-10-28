"""Sanitized tests of actual MOV packet selection and composition-time cursors."""
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
    kind = {"_mov_sync_ctime": "void", "_mov_locate_sample": "unsigned int"}.get(name, "int")
    signature = f"static {kind} {name} ("
    if signature not in source:
        return ""
    start = source.index(signature)
    marker = "}  /* end " + name + " */"
    return source[start:source.index(marker, start) + len(marker)]

types = source[source.index("typedef struct MOVIndex"):source.index("struct MOVContext;")]
if "m_TimeOffset" not in types:
    # Test-only extension lets the old seek implementation run the same tests.
    types = types.replace("} MOVStream;", "int64_t m_TimeOffset; } MOVStream;")
harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define SMS_MAX_STREAMS 8
#define SMS_TIME_BASE 1
#define SMS_MAXINT64 INT64_MAX
#define SMS_PKT_FLAG_KEY 1
typedef struct FileContext FileContext;
struct FileContext {
 unsigned m_Size,m_CurPos;
 int (*Read)(FileContext *,void *,unsigned);
 int (*Seek)(FileContext *,unsigned);
};
typedef struct { void *m_pCtx,*m_pPktBuf; } SMS_Stream;
typedef struct { unsigned char *m_pData;int m_Size,m_StmIdx,m_Flags;int64_t m_DTS,m_PTS; } SMS_AVPacket;
typedef struct {
 FileContext *m_pFileCtx;
 SMS_Stream *m_pStm[8];unsigned m_nStm;
 SMS_AVPacket *(*AllocPacket)(void *,int);
} SMS_Container;
static int64_t SMS_Rescale(int64_t a,int64_t b,int64_t c) { assert(c>0);return a*b/c; }
static void SMSContainer_CalcPktFields(SMS_Stream *s,SMS_AVPacket *p) {(void)s;(void)p;}
static int short_read,seek_fail,seek_wrong,allocations,reserved;
static SMS_AVPacket packet;
static unsigned char payload[64];
static int read_file(FileContext *c,void *p,unsigned n) {
 if(short_read && n) --n;
 memset(p,0x5a,n);c->m_CurPos+=n;return n;
}
static int seek_file(FileContext *c,unsigned p) {
 if(seek_fail) return -1;
 c->m_CurPos=p+(seek_wrong && p!=777);return c->m_CurPos;
}
static void File_Skip(FileContext *c,unsigned n) { seek_file(c,c->m_CurPos+n); }
static SMS_AVPacket *allocate(void *buffer,int n) {
 (void)buffer;assert(n>=0 && n<=64);++allocations;
 reserved+=n+64;
 memset(&packet,0,sizeof(packet));packet.m_Size=n;packet.m_pData=payload;return &packet;
}
static void SMS_RingBufferUnalloc(void *buffer,int n) {
 (void)buffer;assert(n>0 && n<=reserved);reserved-=n;
}
""" + types + extract("_mov_next_ctime") + extract("_ReadPacket") + extract("_mov_sync_ctime") + extract("_mov_locate_sample") + extract("_Seek") + r"""
static FileContext file;
static MOVStream streams[2];
static MOVIndex index_data[2][3];
static SMS_Stream sms_streams[2];
static SMS_Container container;
static void reset(void) {
 memset(&container,0,sizeof(container));memset(streams,0,sizeof(streams));
 memset(index_data,0,sizeof(index_data));
 file=(FileContext){1000,0,read_file,seek_file};
 container.m_pFileCtx=&file;container.m_nStm=2;container.AllocPacket=allocate;
 short_read=seek_fail=seek_wrong=allocations=reserved=0;
 for(int i=0;i<2;++i) {
  streams[i].m_TimeRate=streams[i].m_TimeScale=1;
  streams[i].m_pIdx=index_data[i];streams[i].m_nSamples=streams[i].m_nIdx=1;
  index_data[i][0]=(MOVIndex){1,0,0,4};
  sms_streams[i]=(SMS_Stream){streams+i,streams+i};container.m_pStm[i]=sms_streams+i;
 }
}
static void disabled(void) {
 int selected;
 reset();sms_streams[0].m_pPktBuf=sms_streams[1].m_pPktBuf=NULL;
 assert(_ReadPacket(&container,&selected)==-1 && allocations==0);
 assert(streams[0].m_CurSample==1 && streams[1].m_CurSample==1);
 reset();sms_streams[0].m_pPktBuf=NULL;index_data[0][0].m_Pos=1001;
 assert(_ReadPacket(&container,&selected)==4 && selected==1);
}
static void composition(void) {
 int selected;MOVStts times[2]={{1,10},{0,99}};
 reset();container.m_nStm=1;
 streams[0].m_nSamples=streams[0].m_nIdx=2;
 index_data[0][1]=(MOVIndex){1,1,4,4};
 streams[0].m_pCtts=malloc(sizeof(MOVStts));streams[0].m_pCtts[0]=times[0];streams[0].m_nCtts=1;
 assert(_ReadPacket(&container,&selected)==4 && packet.m_PTS==10);
 assert(_ReadPacket(&container,&selected)==4 && packet.m_PTS==1);
 streams[0].m_CurSample=1;_mov_sync_ctime(streams);
 assert(streams[0].m_Sample2CTimeIdx==1);
 free(streams[0].m_pCtts);
 reset();container.m_nStm=1;streams[0].m_pCtts=times;streams[0].m_nCtts=0;
 assert(_ReadPacket(&container,&selected)==4 && packet.m_PTS==0);
 reset();streams[0].m_pCtts=times;streams[0].m_nCtts=2;sms_streams[0].m_pPktBuf=NULL;
 assert(_ReadPacket(&container,&selected)==4 && selected==1);
 assert(streams[0].m_Sample2CTimeIdx==1);
}
static void ordering(void) {
 int selected;
 reset();index_data[0][0].m_DTS=0x10000000;
 streams[0].m_TimeRate=streams[0].m_TimeScale=16;index_data[1][0].m_DTS=100;
 assert(_ReadPacket(&container,&selected)==4 && selected==1);
 assert(_ReadPacket(&container,&selected)==4 && selected==0);
}
static void failures(void) {
 int selected;
 reset();container.m_nStm=1;index_data[0][0].m_Pos=999;
 assert(_ReadPacket(&container,&selected)==-1 && allocations==0);
 reset();container.m_nStm=1;short_read=1;
 assert(_ReadPacket(&container,&selected)==-1 && !reserved);
 reset();container.m_nStm=1;index_data[0][0].m_Pos=20;seek_fail=1;
 assert(_ReadPacket(&container,&selected)==-1 && !reserved);
 reset();container.m_nStm=1;streams[0].m_TimeScale=0;
 assert(_ReadPacket(&container,&selected)==-1);
 reset();container.m_nStm=1;index_data[0][0].m_Size=0;
 assert(_ReadPacket(&container,&selected)==-1 && allocations==0);
}
static void sample_search(void) {
 MOVIndex index[8]={{0}};
 assert(_mov_locate_sample(NULL,0,0)==0);
 for(int n=1;n<=8;++n) {
  for(int i=0;i<n;++i) index[i].m_DTS=5+10*i;
  for(unsigned target=0;target<100;++target) {
   unsigned expected=0;
   for(int i=0;i<n;++i) if(index[i].m_DTS<=target) expected=i;
   assert(_mov_locate_sample(index,n,target)==expected);
  }
 }
}
static void seeking(void) {
 MOVStts offsets[2]={{1,-5},{2,7}};
 reset();file.m_CurPos=777;
 for(int i=0;i<2;++i) {
  MOVStream *m=streams+i;m->m_nSamples=m->m_nIdx=3;m->m_CurSample=1;
  m->m_pCtts=offsets;m->m_nCtts=2;_mov_sync_ctime(m);
  for(int j=0;j<3;++j) index_data[i][j]=(MOVIndex){j!=1,10*j,100+100*i+4*j,4};
 }
 seek_fail=1;assert(_Seek(&container,0,1,20)==0 && file.m_CurPos==777);
 for(int i=0;i<2;++i) assert(streams[i].m_CurSample==1 && streams[i].m_Sample2CTimeIdx==1 && !streams[i].m_Sample2CTimeSample);
 seek_fail=0;seek_wrong=1;assert(_Seek(&container,0,1,20)==0 && file.m_CurPos==777);
 for(int i=0;i<2;++i) assert(streams[i].m_CurSample==1 && !streams[i].m_Sample2CTimeSample);
 seek_wrong=0;assert(_Seek(&container,0,1,20)==1 && file.m_CurPos==108);
 for(int i=0;i<2;++i) assert(streams[i].m_CurSample==2 && streams[i].m_Sample2CTimeSample==1);
 assert(_Seek(&container,0,1,15)==1 && streams[0].m_CurSample==2);
 assert(_Seek(&container,0,-1,15)==1 && streams[0].m_CurSample==0);
 assert(_Seek(&container,0,0,15)==1 && streams[0].m_CurSample==0);
 assert(_Seek(&container,0,1,21)==0 && streams[0].m_CurSample==0);
 /* A movie-time offset must affect both the video target and other streams. */
 streams[0].m_TimeOffset=20;
 assert(_Seek(&container,0,-1,25)==1 && streams[0].m_CurSample==0 && streams[1].m_CurSample==2);
 assert(_Seek(&container,0,-1,0)==1 && streams[0].m_CurSample==0);
}
int main(int argc,char **argv) {
 const char *name=argc>1?argv[1]:"all";
 if(!strcmp(name,"all")||!strcmp(name,"disabled")) disabled();
 if(!strcmp(name,"all")||!strcmp(name,"composition")) composition();
 if(!strcmp(name,"all")||!strcmp(name,"ordering")) ordering();
 if(!strcmp(name,"all")||!strcmp(name,"failures")) failures();
 if(!strcmp(name,"all")||!strcmp(name,"sample-search")) sample_search();
 if(!strcmp(name,"all")||!strcmp(name,"seeking")) seeking();
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / "mov.c", Path(tmp) / "mov"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe), args.case], check=True, timeout=5)
print("MOV packets: disabled-stream EOF, bounded composition timing, 64-bit ordering, seek and short-read failures passed")
