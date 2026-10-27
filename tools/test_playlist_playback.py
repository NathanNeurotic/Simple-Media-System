"""Exercise the actual folder-playlist function with failed opens and shuffle."""
import os
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "src/SMS_GUICmdProc.c").read_text()
start = source.index("static void _start_playlist ( SMS_List* apList ) {")
end = source.index("}  /* end _start_playlist */", start) + len("}  /* end _start_playlist */")
function = source[start:end]
harness = r"""
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define SMS_PF_RAND 1
#define SMS_FA_FLAGS_AVI 1
#define _STR(n) ((n)->name)
#define GUIRedrawMethod_Redraw 0
typedef struct SMS_ListNode SMS_ListNode;
struct SMS_ListNode { SMS_ListNode *m_pPrev,*m_pNext; int m_Param; const char *name; };
typedef struct { SMS_ListNode *m_pHead,*m_pTail; unsigned m_Size; } SMS_List;
typedef void FileContext;
typedef int SubtitleFormat;
typedef struct SMS_Player SMS_Player;
struct SMS_Player { int (*Play)(SMS_Player *); void (*Destroy)(SMS_Player *); };
static struct { unsigned m_PlayerFlags; } g_Config;
static int g_RandSeed, g_Timer=123;
static int opens, plays, destroys, wrappers, menu_cancel;
static unsigned SMS_rand(void) { return g_RandSeed=(g_RandSeed*17+3)&32767; }
static void GUI_Initialize(int n) { (void)n; }
static void GUI_Redraw(int n) { (void)n; }
static int _playlist_menu(SMS_List *list,SMS_ListNode **item) {
 *item=list->m_pHead;return menu_cancel;
}
static void **SMS_OpenMediaFile(const char *name,int flags) {
 void **p;(void)flags;++opens;
 if(!strcmp(name,"missing")) return NULL;
 p=malloc(3*sizeof(void *));++wrappers;
 p[0]=(void *)name;p[1]=NULL;p[2]=NULL;return p;
}
static int play(SMS_Player *p) { (void)p;++plays;return 0; }
static void destroy(SMS_Player *p) { (void)p;++destroys; }
static SMS_Player player={play,destroy};
static SMS_Player *SMS_InitPlayer(FileContext *f,FileContext *s,SubtitleFormat n) {
 (void)s;(void)n;return strcmp(f,"unsupported")?&player:NULL;
}
static void wrapper_free(void *p) { --wrappers;free(p); }
/* The shuffle allocation is separate from OpenMediaFile's wrapper. */
static void *shuffle_alloc;
static void *test_malloc(size_t n) { return shuffle_alloc=malloc(n); }
static void test_free(void *p) {
 if(p==shuffle_alloc) { shuffle_alloc=NULL;free(p); }
 else wrapper_free(p);
}
#define malloc test_malloc
#define free test_free
""" + function + r"""
int main(void) {
 int random;
 for(random=0;random<=1;++random) {
  SMS_ListNode nodes[4]={{0}};
  const char *names[]={"good1","missing","unsupported","good2"};
  SMS_List list={nodes,nodes+3,4};unsigned seen=0,count=0;
  SMS_ListNode *p;
  for(int i=0;i<4;++i) {
   nodes[i].m_pPrev=i?nodes+i-1:NULL;
   nodes[i].m_pNext=i<3?nodes+i+1:NULL;
   nodes[i].name=names[i];
  }
  opens=plays=destroys=wrappers=0;g_Config.m_PlayerFlags=random;
  _start_playlist(&list);
  assert(opens==4 && plays==2 && destroys==2 && wrappers==0);
  for(p=list.m_pHead;p;p=p->m_pNext) {
   assert(++count<=4);seen|=1U<<(p-nodes);
   if(p->m_pNext) assert(p->m_pNext->m_pPrev==p);
  }
  assert(seen==15 && count==4 && list.m_pHead->m_pPrev==NULL && list.m_pTail->m_pNext==NULL);
 }
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / "playlist.c", Path(tmp) / "playlist"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
print("Playlist: failed opens, unsupported items, wrapper lifetime and shuffled links passed")
