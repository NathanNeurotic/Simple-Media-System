"""Actual M3U creation and generic-probe ownership under host sanitizers.

The real list helpers and container destructor are included. File I/O, media
decoders and playback callbacks are controlled host adapters. PS2 pointer casts
are widened for the host; this does not exercise encoded folder-list pointers.
"""
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
m3u = (args.source or root / "src/SMS_ContainerM3U.c").read_text()
container = (root / "src/SMS_Container.c").read_text()
lists = (root / "src/SMS_List.c").read_text()


def extract(source, signature, name):
    start = source.index(signature)
    marker = "}  /* end " + name + " */"
    return source[start:source.index(marker, start) + len(marker)]


types = m3u[m3u.index("#define MYCONT"):m3u.index("static int _SelectFile")]
helpers = extract(m3u, "static void _m3u_destroy_list", "_m3u_destroy_list")
helpers += extract(m3u, "static int _check_name", "_check_name")
helpers += extract(m3u, "static FileContext* _open_file", "_open_file")
helpers += extract(m3u, "static int _SelectFile ( SMS_Container* apCont", "_SelectFile")
helpers += extract(m3u, "static void _Destroy (", "_Destroy")
creator = extract(m3u, "int SMS_GetContainerM3U (", "SMS_GetContainerM3U")
generic = extract(container, "SMS_Container* SMS_GetContainer (", "SMS_GetContainer")
destroy = extract(container, "void SMS_DestroyContainer (", "SMS_DestroyContainer")
table = container[container.index("typedef int ( *ContainerCreator"):container.index("static SMS_AVPacket* _AllocPacket")]
list_helpers = "".join(extract(lists, signature, name) for signature, name in (
    ("void SMS_ListPop (", "SMS_ListPop"),
    ("void SMS_ListPopBack (", "SMS_ListPopBack"),
    ("SMS_ListNode* SMS_ListPushBack (", "SMS_ListPushBack"),
    ("void SMS_ListDestroy (", "SMS_ListDestroy"),
    ("void SMS_ListRemove (", "SMS_ListRemove"),
    ("SMS_ListNode* SMS_ListAt (", "SMS_ListAt"),
    ("SMS_List* SMS_ListInit (", "StringList_Init"),
    ("static SMS_ListNode* _find (", "_find"),
    ("SMS_ListNode* SMS_ListFind (", "SMS_ListFind"),
))

prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define SMS_MAX_STREAMS 8
#define SMS_CONT_FLAGS_SEEKABLE 1
#define SMS_PF_RAND 0x40
#define _STR(n) ((char *)((n)+1))
enum { SMS_CONTAINER_AVI,SMS_CONTAINER_ASF,SMS_CONTAINER_OGG,SMS_CONTAINER_M4A,
       SMS_CONTAINER_FLAC,SMS_CONTAINER_AAC,SMS_CONTAINER_MP3,SMS_CONTAINER_AC3,
       SMS_CONTAINER_MPEG_PS,SMS_CONTAINER_JPG,SMS_CONTAINER_M3U,SMS_CONTAINER_MOV };
enum { SMS_SUBCONTAINER_M4A,SMS_SUBCONTAINER_OGG,SMS_SUBCONTAINER_ASF,
       SMS_SUBCONTAINER_FLAC,SMS_SUBCONTAINER_AAC,SMS_SUBCONTAINER_AC3,SMS_SUBCONTAINER_MP3 };
typedef struct SMS_ListNode SMS_ListNode;
struct SMS_ListNode { uint64_t m_Param;SMS_ListNode *m_pNext,*m_pPrev; };
typedef struct { SMS_ListNode *m_pHead,*m_pTail;unsigned m_Size; } SMS_List;
typedef struct FileContext FileContext;
struct FileContext {
 char *m_pPath;unsigned m_CurPos,m_Size;const char *data;int original;
 void *m_pOpenParam;FileContext *(*Open)(const char *,void *);
 int (*Seek)(FileContext *,unsigned);void (*Destroy)(FileContext *);
};
typedef struct { int unused; } SMS_CodecContext;
typedef struct SMS_Stream SMS_Stream;
struct SMS_Stream { SMS_CodecContext *m_pCodec;void *m_pPktBuf,*m_pCtx;
 char *m_pName;void (*Destroy)(SMS_Stream *); };
typedef struct { int m_Size; } SMS_AVPacket;
typedef struct SMS_Container SMS_Container;
struct SMS_Container {
 SMS_Stream *m_pStm[8];unsigned m_nStm,m_Flags;void *m_pCtx;
 FileContext *m_pFileCtx;char *m_pName;SMS_List *m_pPlayList;SMS_ListNode *m_pPlayItem;
 int m_DefPackSize,m_DefPackIdx;int64_t m_Duration;
 SMS_AVPacket *(*AllocPacket)(void *,int);int (*ReadPacket)(SMS_Container *,int *);
 int (*Seek)(SMS_Container *,int,int,uint32_t);void (*Destroy)(SMS_Container *,int);
};
typedef struct { int m_SampleRate,m_nChannels,m_BitRate; } SMS_AudioInfo;
static char g_CWD[1024]="mass0:/music",g_SlashStr[]="/",g_pM3UStr[]="M3U";
static char g_pExtM3UStr[]="#EXTM3U",g_pExtInfStr[]="#EXTINF",g_pPercDStr[]="%u";
static struct { unsigned m_PlayerFlags; } g_Config;
static int g_CMedia,g_RandSeed,g_Timer;
static void *g_pCDDACtx;
static unsigned SMS_rand(void) { return 0; }
static unsigned allocations,fail_at,original_closes,child_closes,original_seeks,mov_probes;
static int child_kind;
static void *test_calloc(size_t n,size_t size) {
 if(++allocations==fail_at) return NULL;
 return calloc(n,size);
}
#define calloc test_calloc
static int seek_file(FileContext *f,unsigned pos) {
 assert(pos<=f->m_Size);if(f->original) ++original_seeks;f->m_CurPos=pos;return pos;
}
static void close_file(FileContext *f) {
 if(f->original) ++original_closes;else ++child_closes;
 free(f->m_pPath);free(f);
}
static FileContext *STIO_InitFileContext(const char *path,void *param) {
 (void)param;if(!child_kind) return NULL;
 FileContext *f=calloc(1,sizeof(*f));assert(f);
 f->m_pPath=strdup(path);f->m_Size=1;f->data="x";
 f->Seek=seek_file;f->Destroy=close_file;f->Open=STIO_InitFileContext;return f;
}
static FileContext *CDDA_InitFileContext(const char *path,void *param) { return STIO_InitFileContext(path,param); }
#define FILE_EOF(f) ((f)->m_CurPos>=(f)->m_Size)
static void File_GetString(FileContext *f,char *out,unsigned max) {
 unsigned n=0;
 while(f->m_CurPos<f->m_Size && n+1<max) {
  char c=f->data[f->m_CurPos++];if(c=='\n') break;out[n++]=c;
 }
 out[n]=0;
}
static int _ReadFirstPacket(SMS_Container *c,int *idx) { (void)c;(void)idx;return -1; }
static int _Seek(SMS_Container *c,int idx,int dir,uint32_t pos) { (void)c;(void)idx;(void)dir;(void)pos;return 0; }
static SMS_AVPacket *_AllocPacket(void *p,int n) { (void)p;(void)n;return NULL; }
static void SMS_CodecDestroy(SMS_CodecContext *c) { free(c); }
static void SMS_RingBufferDestroy(void *p) { free(p); }
static int SMS_ContID(const char *path) { (void)path;return SMS_CONTAINER_M3U; }
static void SMS_DestroyContainer(SMS_Container *,int);
static SMS_Container *SMS_GetContainer(FileContext *,int);
static int SMS_GetContainerM3U(SMS_Container *);
static uint64_t probe_audio(FileContext *f,SMS_AudioInfo *i) { (void)f;(void)i;return 0; }
#define SMS_M4AProbe probe_audio
#define SMS_OGGVProbe probe_audio
#define SMS_WMAProbe probe_audio
#define SMS_FLACProbe probe_audio
#define SMS_AACProbe probe_audio
#define SMS_AC3Probe probe_audio
#define SMS_MP3Probe probe_audio
#define PROBE_CRUMB(...) ((void)0)
"""
stub_creators = r"""
static int reject(SMS_Container *c) { (void)c;return 0; }
#define SMS_GetContainerAVI reject
#define SMS_GetContainerASF reject
#define SMS_GetContainerOGG reject
#define SMS_GetContainerM4A reject
#define SMS_GetContainerFLAC reject
#define SMS_GetContainerAAC reject
#define SMS_GetContainerAC3 reject
#define SMS_GetContainerMPEG_PS reject
#define SMS_GetContainerJPG reject
static int SMS_GetContainerMP3(SMS_Container *c) {
 if(c->m_pFileCtx->original || child_kind!=2) return 0;
 c->m_pName="MP3";c->m_nStm=1;c->m_pStm[0]=calloc(1,sizeof(SMS_Stream));
 c->m_pStm[0]->m_pCodec=calloc(1,sizeof(SMS_CodecContext));
 c->m_pPlayList=SMS_ListInit();SMS_ListPushBack(c->m_pPlayList,"track");
 return 1;
}
static int SMS_GetContainerMOV(SMS_Container *c) {
 ++mov_probes;assert(c->m_pFileCtx->original && !original_closes);
 assert(!c->m_pCtx && !c->m_pPlayList && !c->m_pPlayItem && !c->m_pName);
 assert(!c->Destroy && !c->ReadPacket && !c->Seek);return 0;
}
"""
test = r"""
static char expected_path[2048];
static unsigned path_opens;
static FileContext path_result;
static FileContext *open_path(const char *path,void *param) {
 assert(param==&path_result && !strcmp(path,expected_path));++path_opens;return &path_result;
}
static void path_bounds(void) {
 const unsigned lengths[]={0,1,2,255,510,511,512};
 char name[513];_M3UEntry entry={0,name};SMS_ListNode node={0};
 node.m_Param=(uintptr_t)&entry;
 for(unsigned iteration=0;iteration<1024;++iteration) {
  unsigned prefix=(iteration+1023)%1024;
  for(unsigned trailing=0;trailing<2;++trailing) {
   memset(g_CWD,'d',prefix);g_CWD[prefix]=0;
   if(prefix && trailing)g_CWD[prefix-1]='/';
   for(unsigned i=0;i<sizeof(lengths)/sizeof(lengths[0]);++i) {
    unsigned length=lengths[i];
    for(unsigned leading=0;leading<2;++leading) {
     memset(name,'n',length);name[length]=0;if(length && leading)name[0]='/';
     unsigned slash=prefix && g_CWD[prefix-1]!='/' && name[0]!='/';
     unsigned total=prefix+slash+length;
     memcpy(expected_path,g_CWD,prefix);
     if(slash)expected_path[prefix]='/';
     memcpy(expected_path+prefix+slash,name,length+1);
     path_opens=0;
     FileContext *result=_open_file(NULL,&node,open_path,&path_result);
     assert(result==(total<1024?&path_result:NULL));
     assert(path_opens==(unsigned)(total<1024));
     assert(strlen(g_CWD)==prefix && !memcmp(g_CWD,expected_path,prefix));
    }
   }
  }
 }
}
int main(int argc,char **argv) {
 assert(argc==2);const char *name=argv[1];
 if(!strcmp(name,"path-bounds")) { path_bounds();return 0; }
 int automatic=strstr(name,"automatic")!=NULL,selecting=!strncmp(name,"select-",7);
 int success=strstr(name,"success")!=NULL || selecting;
 int overlong=strstr(name,"overlong")!=NULL;
 unsigned expected_child_closes;
 child_kind=selecting?2:strstr(name,"missing")?0:success?2:1;
 if(strstr(name,"allocation")) fail_at=4;
 FileContext *input=malloc(sizeof(*input));assert(input);memset(input,0,sizeof(*input));
 input->m_pPath=strdup("mass0:/music/list.m3u");
 input->data=selecting?"track.mp3\nnext.mp3\n":"track.mp3\n";
 input->m_Size=strlen(input->data);input->Seek=seek_file;input->Destroy=close_file;
 input->Open=STIO_InitFileContext;input->original=1;
 if(overlong && !selecting) { memset(g_CWD,'d',1023);g_CWD[1023]=0; }
 SMS_Container *result=SMS_GetContainer(input,automatic?-1:SMS_CONTAINER_M3U);
 assert((result!=NULL)==success);
 expected_child_closes=success?1U:child_kind && !fail_at && !overlong?1U:0U;
 if(success) {
  assert(original_closes==1 && !child_closes && result->m_pFileCtx);
  assert(result->m_pCtx && result->m_pPlayList && result->m_pPlayItem);
  assert(result->m_nStm==1 && result->m_pStm[0] && result->Destroy==_Destroy);
  if(selecting) {
   FileContext *old=result->m_pFileCtx;_M3UContainer *m=MYCONT(result);
   result->m_pPlayItem=result->m_pPlayItem->m_pNext;assert(result->m_pPlayItem);
   child_kind=strstr(name,"missing")?0:2;
   if(overlong) { memset(g_CWD,'d',1023);g_CWD[1023]=0; }
   if(!child_kind || overlong) {
    assert(_SelectFile(result,m)==0);old->Seek(old,0);
    assert(result->m_pFileCtx==old && m->m_pSubCont->m_pFileCtx==old && !child_closes);
   } else {
    assert(_SelectFile(result,m)==1 && result->m_pFileCtx!=old && child_closes==1);
    assert(m->m_pSubCont->m_pFileCtx==result->m_pFileCtx);result->m_pFileCtx->Seek(result->m_pFileCtx,0);
    ++expected_child_closes;
   }
  }
  result->Destroy(result,1);assert(original_closes==1 && child_closes==expected_child_closes);
 } else if(automatic) {
  assert(original_closes==1 && mov_probes==1 && original_seeks>0);
 } else {
  input->Seek(input,0);assert(!original_closes && !mov_probes);input->Destroy(input);
  assert(original_closes==1);
 }
 assert(child_closes==expected_child_closes);
 return 0;
}
"""

# The PS2 stores pointer-valued list parameters in an unsigned 32-bit word.
adapted = types + helpers + creator + generic
adapted = adapted.replace("( unsigned int )", "( uintptr_t )")
adapted = adapted.replace("( int )lpFileCtx", "( intptr_t )lpFileCtx")
adapted = adapted.replace("( int )apFileCtx", "( intptr_t )apFileCtx")
adapted = adapted.replace("( int )apCont -> m_pFileCtx", "( intptr_t )apCont -> m_pFileCtx")
types_end = adapted.index("static void _m3u_destroy_list")
adapted_types, adapted_functions = adapted[:types_end], adapted[types_end:]
cases = ("automatic-missing", "automatic-unsupported", "explicit-missing", "explicit-unsupported",
         "automatic-allocation", "explicit-allocation", "automatic-success", "explicit-success",
         "select-missing", "select-success", "automatic-overlong", "explicit-overlong",
         "select-overlong", "path-bounds")
with tempfile.TemporaryDirectory() as temporary:
    src, exe = Path(temporary) / "m3u.c", Path(temporary) / "m3u"
    src.write_text("\n".join((prefix, list_helpers, stub_creators, table, destroy, adapted_types, adapted_functions, test)))
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-Wall", "-Wextra",
                    "-Wno-sign-compare", "-Wno-unused-function", "-Wno-unused-parameter", "-Wno-unused-variable",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all", str(src), "-o", str(exe)], check=True)
    for case in cases if args.case == "all" else (args.case,):
        subprocess.run([str(exe), case], check=True, timeout=10)
        print(f"M3U probe: {case} passed")
