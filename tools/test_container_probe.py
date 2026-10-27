"""Actual generic container allocation/probe ownership under host sanitizers.

Creator callbacks are controlled stubs; these tests do not certify their parser
or cleanup contracts. Set SMS_CONTAINER_SOURCE_REF for a pre-fix Git baseline.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ref = os.environ.get("SMS_CONTAINER_SOURCE_REF")
if ref:
    source = subprocess.check_output(["git", "show", f"{ref}:src/SMS_Container.c"], cwd=ROOT, text=True)
else:
    source = (ROOT / "src/SMS_Container.c").read_text()
def extract(name, signature):
    start = source.index(signature)
    end_marker = "}  /* end " + name + " */"
    return source[start:source.index(end_marker, start) + len(end_marker)]


destroy_function = extract("SMS_DestroyContainer", "void SMS_DestroyContainer (")
function = extract("SMS_GetContainer", "SMS_Container* SMS_GetContainer (")
harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct FileContext FileContext;
struct FileContext {
 char *m_pPath;
 int (*Seek)(FileContext *,int);
 void (*Destroy)(FileContext *);
};
typedef struct SMS_Container SMS_Container;
typedef struct SMS_Stream SMS_Stream;
struct SMS_Stream {
 void (*Destroy)(SMS_Stream *);
 void *m_pCodec, *m_pName, *m_pPktBuf;
};
struct SMS_Container {
 SMS_Stream *m_pStm[8];
 uint32_t m_nStm;
 void *m_pCtx, *m_pPlayList;
 FileContext *m_pFileCtx;
 void (*AllocPacket)(void);
 char *m_pName;
 void (*Destroy)(SMS_Container *,int);
};
typedef int (*ContainerCreator)(SMS_Container *);
static int fail_allocation, allocations, live, closes, seeks, calls, custom_destroys;
static int statuses[3], hint=-1, order[3];
static int partial_mode, streams_destroyed, codecs_destroyed, packets_destroyed, lists_destroyed;
static void *test_calloc(size_t count,size_t size) {
 ++allocations;
 if(fail_allocation) return NULL;
 void *p=calloc(count,size);assert(p);++live;return p;
}
static void test_free(void *p) { if(p) { --live;free(p); } }
#define calloc test_calloc
#define free test_free
static void _AllocPacket(void) {}
static void close_file(FileContext *file) { (void)file;++closes; }
static int seek_file(FileContext *file,int pos) { (void)file;assert(pos==0);++seeks;return 0; }
static FileContext file={"mass0:/test.unknown",seek_file,close_file};
static void SMS_CodecDestroy(void *codec) { if(codec) { ++codecs_destroyed;free(codec); } }
static void SMS_RingBufferDestroy(void *packet) { ++packets_destroyed;free(packet); }
static void SMS_ListDestroy(void *list,int all) { assert(all==1);++lists_destroyed;free(list); }
""" + destroy_function + r"""
static void destroy_stream(SMS_Stream *stream) { (void)stream;++streams_destroyed; }
static void make_partial_payload(SMS_Container *container) {
 SMS_Stream *stream=calloc(1,sizeof(*stream));
 stream->m_pCodec=calloc(1,4);stream->m_pName=calloc(1,8);stream->m_pPktBuf=calloc(1,4);
 stream->Destroy=destroy_stream;
 container->m_pStm[container->m_nStm++]=stream;
 container->m_pCtx=calloc(1,8);container->m_pPlayList=calloc(1,8);
}
static void custom_destroy(SMS_Container *container,int all) {
 ++custom_destroys;SMS_DestroyContainer(container,all);
}
static int SMS_ContID(const char *path) { (void)path;return hint; }
static int create(SMS_Container *container,int id) {
 order[calls++]=id;
 if(partial_mode && id==0) {
  make_partial_payload(container);
  /* ASF's rejected-header path releases its payload before returning zero. */
  SMS_DestroyContainer(container,0);
  assert(container->m_nStm==0 && !container->m_pCtx && !container->m_pPlayList);
  for(unsigned i=0;i<8;++i) assert(!container->m_pStm[i]);
  assert(live==1 && closes==0 && container->m_pFileCtx==&file);
  /* Partial destruction remains safe to repeat and keeps the source open. */
  SMS_DestroyContainer(container,0);
  assert(live==1 && closes==0);
  return 0;
 }
 if(statuses[id]) {
  if(partial_mode) {
   assert(container->m_nStm==0);
   make_partial_payload(container);
   assert(container->m_nStm==1 && container->m_pStm[0] && !container->m_pStm[1]);
  }
  container->m_pName="accepted";
  if(id==2) container->Destroy=custom_destroy;
 }
 return statuses[id];
}
static int creator0(SMS_Container *container) { return create(container,0); }
static int creator1(SMS_Container *container) { return create(container,1); }
static int creator2(SMS_Container *container) { return create(container,2); }
static const ContainerCreator s_CCreator[]={creator0,creator1,creator2,NULL};
#define PROBE_CRUMB(...) ((void)0)
""" + function + r"""
int main(int argc,char **argv) {
 assert(argc==2);
 FileContext *input=&file;
 int preferred=-1, expected=0, expected_closes=1, expected_calls=0;
 if(!strcmp(argv[1],"automatic-allocation")) fail_allocation=1;
 else if(!strcmp(argv[1],"explicit-allocation")) {
  fail_allocation=1;preferred=1;expected_closes=0;
 } else if(!strcmp(argv[1],"sentinel-allocation")) {
  fail_allocation=1;input=(FileContext *)(intptr_t)-1;expected_closes=0;
 } else if(!strcmp(argv[1],"null-allocation")) {
  fail_allocation=1;input=NULL;expected_closes=0;
 } else if(!strcmp(argv[1],"automatic-rejection")) expected_calls=3;
 else if(!strcmp(argv[1],"explicit-rejection")) {
  preferred=1;expected_closes=0;expected_calls=1;
 } else if(!strcmp(argv[1],"automatic-fallback")) {
  statuses[1]=1;expected=1;expected_calls=2;expected_closes=0;
 } else if(!strcmp(argv[1],"automatic-preference")) {
  hint=1;statuses[1]=1;expected=1;expected_calls=1;expected_closes=0;
 } else if(!strcmp(argv[1],"explicit-success")) {
  preferred=1;statuses[1]=1;expected=1;expected_calls=1;expected_closes=0;
 } else if(!strcmp(argv[1],"automatic-fatal")) {
  statuses[0]=-1;expected_calls=1;
 } else if(!strcmp(argv[1],"custom-success")) {
  hint=2;statuses[2]=1;expected=1;expected_calls=1;expected_closes=0;
 } else if(!strcmp(argv[1],"custom-fatal")) {
  hint=2;statuses[2]=-1;expected_calls=1;
 } else if(!strcmp(argv[1],"partial-fallback")) {
  partial_mode=1;statuses[1]=1;expected=1;expected_calls=2;expected_closes=0;
 } else if(!strcmp(argv[1],"partial-explicit")) {
  partial_mode=1;preferred=0;expected_calls=1;expected_closes=0;
 } else assert(0);
 SMS_Container *result=SMS_GetContainer(input,preferred);
 assert((result!=NULL)==expected);
 assert(allocations==(partial_mode ? expected ? 13 : 7 : 1));
 assert(live==(partial_mode && expected ? 7 : expected) && calls==expected_calls);
 assert(closes==expected_closes);
 if(!strcmp(argv[1],"automatic-fallback")) assert(seeks==1 && order[0]==0 && order[1]==1);
 if(!strcmp(argv[1],"automatic-rejection")) assert(seeks==3);
 if(!strcmp(argv[1],"automatic-preference")) assert(order[0]==1);
 if(expected) {
  assert(result->m_pFileCtx==input && result->AllocPacket==_AllocPacket && result->Destroy);
  result->Destroy(result,1);
  assert(closes==1 && live==0);
 }
 if(!strcmp(argv[1],"custom-success")||!strcmp(argv[1],"custom-fatal")) assert(custom_destroys==1);
 if(preferred>=0 && !expected) {
  /* Explicit failures leave this context available for the caller's next probe. */
  assert(closes==0);input->Seek(input,0);input->Destroy(input);assert(closes==1);
 }
 assert(live==0);
 if(partial_mode) {
  int payloads=expected ? 2 : 1;
  assert(streams_destroyed==payloads && codecs_destroyed==payloads);
  assert(packets_destroyed==payloads && lists_destroyed==payloads);
 }
 return 0;
}
"""
cases = sys.argv[1:] or ["automatic-allocation", "explicit-allocation", "sentinel-allocation", "null-allocation",
                         "automatic-rejection", "explicit-rejection", "automatic-fallback", "automatic-preference",
                         "explicit-success", "automatic-fatal", "custom-success", "custom-fatal",
                         "partial-fallback", "partial-explicit"]
with tempfile.TemporaryDirectory() as temp:
    src, exe = Path(temp) / "container.c", Path(temp) / "container"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-Wall", "-Wextra",
                    "-Wno-pointer-to-int-cast", "-Wno-sign-compare", "-Werror",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all", str(src), "-o", str(exe)], check=True)
    for case in cases:
        subprocess.run([str(exe), case], check=True, timeout=10)
print(f"Container probe: {len(cases)} allocation, automatic/explicit ownership and creator controls passed")
