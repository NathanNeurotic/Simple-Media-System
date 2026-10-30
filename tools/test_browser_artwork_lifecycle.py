"""Actual browser navigation, reset and redraw cleanup under ASan/UBSan/LSan.

Uses the production cache, layouts, folder actions/event handler and cleanup.
Directory I/O and GS/DMA are host adapters; this does not execute a PS2 display
chain or establish console acceptance after returning from a folder or player.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
browser = (root / "src/SMS_GUIFileMenu.c").read_text()
gui = (root / "src/SMS_GUI.c").read_text()
lists = (root / "src/SMS_List.c").read_text()


def function(source, name):
    match = re.search(r"^[^\n;{}]*\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", source, re.M)
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


types = browser[browser.index("typedef struct _LevelInfo"):browser.index("static u64", browser.index("typedef struct GUIFileMenu"))]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "SMS_GUI.h"
#include "SMS_List.h"
#include "SMS_Artwork.h"
#include "SMS_PAD.h"
#include "SMS_RC.h"
static unsigned allocation_count,fail_at;
static void *checked_malloc(size_t n) {
 ++allocation_count;if(fail_at==allocation_count)return NULL;return malloc(n);
}
static void *checked_calloc(size_t n,size_t size) {
 ++allocation_count;if(fail_at==allocation_count)return NULL;return calloc(n,size);
}
#define malloc checked_malloc
#define calloc checked_calloc
'''
cwd_defines = "\n".join(line for line in (root / "include/SMS_Sounds.h").read_text().splitlines()
                         if line.startswith("#define SMS_CWD_CAPACITY") or line.startswith("#define g_CWD "))
adapters = cwd_defines + r'''
static GUIFileMenu menu;
static SMS_List filtered,packets,objects,listing,levels;
static SMS_ListNode desktop_node,menu_node,other_node;
static GUIObject desktop,other;
GUIObject *g_pFileMenu=(GUIObject *)&menu;
static SMS_List *s_pObjectList=&objects;
static SMS_List *g_pFileList=&listing;
static unsigned char g_SMSounds[2048];
static char expected_path[256];
static char g_EmptyStr[]="",g_DotStr[]=".";
static int g_CMedia,g_PD,g_SMBU;
static struct { int m_PlayerVolume;char m_Partition[64]; } g_Config;
static unsigned drains,generic_cleanups,unloads,redraws;
static unsigned directory_reads,progress;
static void _artwork_drain(void) { ++drains; }
static void GSContext_DeleteList(u64 *p) { free(p); }
static void GUI_UnloadIcons(void) { ++unloads; }
void SMS_ListDestroy(SMS_List *list,int destroy) {
 assert(!destroy);
 if(list==&filtered)assert(!menu.m_Artwork.m_Count && drains);
 SMS_ListNode *node=list->m_pHead;
 while(node){SMS_ListNode *next=node->m_pNext;free(node);node=next;}
 memset(list,0,sizeof(*list));
}
SMS_ListNode *SMS_ListAt(SMS_List *list,int index) {
 SMS_ListNode *node=list->m_pHead;while(node && index-->0)node=node->m_pNext;return node;
}
static void generic_cleanup(GUIObject *p) { assert(p==&desktop || p==&other);++generic_cleanups; }
static void SMS_PgIndStart(void) { assert(!progress);progress=1; }
static void SMS_PgIndStop(void) { assert(progress);progress=0; }
static int _filter_item(SMS_ListNode *p) { (void)p;return 0; }
static void SMS_FileDirInit(char *path);
#define SMSound_PAD 0
static void SPU_PlaySound(int sound,int volume) { (void)sound;(void)volume; }
static void CDVD_FlushCache(void) {}
void GUI_PostMessage(u64 event) { (void)event; }
static int _step_up(GUIFileMenu *p,int roll) { (void)p;(void)roll;assert(0);return 0; }
static int _step_down(GUIFileMenu *p,int roll) { (void)p;(void)roll;assert(0);return 0; }
static int _page_scroll(int (*step)(GUIFileMenu *,int),GUIFileMenu *p) {
 (void)step;(void)p;assert(0);return 0;
}
static void _context_action_file(GUIFileMenu *p,int popup) { (void)p;(void)popup;assert(0); }
static void _action_partition(GUIFileMenu *p,int popup) { (void)p;(void)popup;assert(0); }
static void _action_folder(GUIFileMenu *p,int popup);
static void (*Action[9])(GUIFileMenu *,int)={_action_folder};
static void (*ContextAction[9])(GUIFileMenu *,int)={_context_action_file};
'''
reset_adapter = r'''
static void _redraw(GUIFileMenu *p,int all) {
 (void)all;++redraws;
 if(!p->m_pFiltList->m_Size) {
  assert(!p->m_pFiltList->m_pHead && !p->m_pFiltList->m_pTail);
  assert(!p->m_pCurrent && !p->m_pFirst && !p->m_pLast && !p->m_YOffset);
  assert(!p->m_Artwork.m_Count);
 }
 /* Production grid rendering recomputes the page containing the selection. */
 if(p->m_ArtworkMode)_artwork_position(p);
 GUIFileMenu_CleanupPackets((GUIObject *)p);
}
'''
post = r'''
static void SMS_FileDirInit(char *path) {
 assert(progress && !strcmp(path,expected_path));++directory_reads;
 SMS_ListDestroy(&listing,0);
 if(!path[0])strcpy(g_CWD,"mass0:/");
 else if(strcmp(path,".")) {
  if(g_CWD[strlen(g_CWD)-1]!='/' && path[0]!='/')strcat(g_CWD,"/");
  strcat(g_CWD,path);
 }
 if(!strcmp(g_CWD,"mass0:/")) {
  for(unsigned i=0;i<9;++i) {
   char name[16];sprintf(name,"item%u.avi",i);
   SMS_ListPushBack(&listing,i==7?"MoviesDirectory":name)->m_Param=i==7?0:2;
  }
 } else if(!strcmp(g_CWD,"mass0:/MoviesDirectory")) {
  SMS_ListPushBack(&listing,"episode.avi")->m_Param=2;
  SMS_ListPushBack(&listing,"SeasonDirectory")->m_Param=0;
 } else {
  assert(!strcmp(g_CWD,"mass0:/MoviesDirectory/SeasonDirectory"));
  SMS_ListPushBack(&listing,"finale.avi")->m_Param=2;
 }
}
static unsigned char *load(const void *key,void *arg) {
 (void)key;++*(unsigned *)arg;return malloc(SMS_ARTWORK_BYTES);
}
static void populate(void) {
 SMS_ListNode *node=calloc(1,sizeof(*node)+10);assert(node);strcpy(_STR(node),"movie.avi");
 filtered.m_pHead=filtered.m_pTail=node;filtered.m_Size=1;
 menu.m_pFiltList=&filtered;menu.m_pFileList=&packets;
 menu.m_pCurrent=menu.m_pFirst=menu.m_pLast=node;menu.m_YOffset=4;
 menu.Cleanup=GUIFileMenu_Cleanup;menu.m_pGSPacket=malloc(16);
 const void *keys[]={node};unsigned calls=0;
 SMS_ArtworkPage(&menu.m_Artwork,keys,1);
 assert(SMS_ArtworkGet(&menu.m_Artwork,0,load,&calls) && calls==1);
}
static void folder_navigation(int artwork) {
 menu.m_pLevels=&levels;menu.m_ArtworkMode=artwork;
 strcpy(expected_path,"");_fill(&menu,g_EmptyStr,0);
 assert(!strcmp(g_CWD,"mass0:/") && filtered.m_Size==9);
 menu.m_pCurrent=SMS_ListAt(&filtered,7);
 menu.m_pFirst=SMS_ListAt(&filtered,2);menu.m_YOffset=5;
 if(artwork)_artwork_position(&menu);
 const void *keys[]={menu.m_pCurrent};unsigned calls=0;
 SMS_ArtworkPage(&menu.m_Artwork,keys,1);
 assert(SMS_ArtworkGet(&menu.m_Artwork,0,load,&calls) && calls==1);

 /* Rejected long paths must keep the listing and navigation levels intact. */
 char oversized[SMS_CWD_CAPACITY+1];memset(oversized,'x',sizeof(oversized)-1);oversized[sizeof(oversized)-1]=0;
 unsigned old_reads=directory_reads,old_count=filtered.m_Size;
 _fill(&menu,oversized,0);
 assert(directory_reads==old_reads && filtered.m_Size==old_count && !levels.m_Size);
 SMS_ListNode *long_node=SMS_ListPushBack(&filtered,oversized);long_node->m_Param=0;
 menu.m_pCurrent=long_node;
 assert(GUIFileMenu_HandleEvent((GUIObject *)&menu,SMS_PAD_CROSS)==GUIHResult_Handled);
 assert(directory_reads==old_reads && !levels.m_Size && menu.m_pCurrent==long_node);
 menu.m_pCurrent=SMS_ListAt(&filtered,7);

 /* Each state-save allocation must fail before directory I/O or list reset. */
 for(unsigned failure=1;failure<=3;++failure) {
  SMS_ListNode *selected=menu.m_pCurrent;
  unsigned reads=directory_reads,count=filtered.m_Size;
  fail_at=allocation_count+failure;
  GUIFileMenu_HandleEvent((GUIObject *)&menu,SMS_PAD_CROSS);
  fail_at=0;
  assert(directory_reads==reads && filtered.m_Size==count && !levels.m_Size);
  assert(menu.m_pCurrent==selected && !strcmp(g_CWD,"mass0:/"));
 }

 /* CROSS invokes the real folder action; its argument aliases the node that
  * _reset frees. The directory adapter reads that name after the reset. */
 strcpy(expected_path,"MoviesDirectory");
 assert(GUIFileMenu_HandleEvent((GUIObject *)&menu,SMS_PAD_CROSS)==GUIHResult_Handled);
 assert(!strcmp(g_CWD,"mass0:/MoviesDirectory") && levels.m_Size==1);
 assert(!menu.m_Artwork.m_Count && filtered.m_Size==2);
 menu.m_pCurrent=SMS_ListAt(&filtered,1);menu.m_YOffset=1;
 strcpy(expected_path,"SeasonDirectory");
 assert(GUIFileMenu_HandleEvent((GUIObject *)&menu,SMS_PAD_CROSS)==GUIHResult_Handled);
 assert(!strcmp(g_CWD,"mass0:/MoviesDirectory/SeasonDirectory") && levels.m_Size==2);

 /* Nested Back passes a stack path; root Back passes the shared empty string.
  * Exercise production state restoration for both list and grid modes. */
 strcpy(expected_path,"MoviesDirectory");
 GUIFileMenu_HandleEvent((GUIObject *)&menu,SMS_PAD_TRIANGLE);
 assert(!strcmp(g_CWD,"mass0:/MoviesDirectory") && levels.m_Size==1);
 assert(!strcmp(_STR(menu.m_pCurrent),"SeasonDirectory") && menu.m_YOffset==1);
 strcpy(expected_path,"");
 GUIFileMenu_HandleEvent((GUIObject *)&menu,SMS_PAD_TRIANGLE);
 assert(!strcmp(g_CWD,"mass0:/") && !levels.m_Size);
 assert(!strcmp(_STR(menu.m_pCurrent),"MoviesDirectory"));
 assert(!strcmp(_STR(menu.m_pFirst),artwork?"item6.avi":"item2.avi"));
 assert(menu.m_YOffset==(artwork?1:5));

 /* Refresh takes a shared dot string, then media removal must clear entries
  * so a grid redraw cannot select the old filtered head again. */
 strcpy(expected_path,".");
 GUIFileMenu_HandleEvent((GUIObject *)&menu,GUI_MSG_RELOAD_BROWSER);
 assert(directory_reads==6 && !strcmp(g_CWD,"mass0:/"));
 GUIFileMenu_HandleEvent((GUIObject *)&menu,GUI_MSG_MEDIA_REMOVED);
 assert(!filtered.m_Size && !menu.m_pCurrent && !menu.m_pFirst && !menu.m_pLast);
 menu.Cleanup((GUIObject *)&menu);SMS_ListDestroy(&listing,0);
}
int main(int argc,char **argv) {
 assert(argc==2);populate();
 if(!strcmp(argv[1],"redraw")) {
  /* Production uses EE addresses in m_Param. Static non-PIE objects keep
   * these addresses below 4 GiB on this host without truncating heap pointers. */
  assert((uintptr_t)&menu<=UINT32_MAX && (uintptr_t)&other<=UINT32_MAX);
  desktop.Cleanup=other.Cleanup=generic_cleanup;
  desktop_node.m_Param=(uintptr_t)&desktop;desktop_node.m_pNext=&menu_node;
  menu_node.m_Param=(uintptr_t)&menu;menu_node.m_pNext=&other_node;
  other_node.m_Param=(uintptr_t)&other;
  objects.m_pHead=&desktop_node;objects.m_pTail=&other_node;objects.m_Size=3;
  unsigned char *pixels=menu.m_Artwork.m_Slots[0].m_pPixels;
  for(int all=0;all<2;++all) {
   _cleanup(all);
   assert(!menu.m_pGSPacket && menu.m_Artwork.m_Count==1);
   assert(menu.m_Artwork.m_Slots[0].m_pPixels==pixels);
   unsigned calls=0;assert(SMS_ArtworkGet(&menu.m_Artwork,0,load,&calls)==pixels && !calls);
  }
  assert(generic_cleanups==3 && unloads==1 && drains==2);
  /* The real destruction callback must still release cached pixels. */
  menu.Cleanup((GUIObject *)&menu);
  assert(!menu.m_Artwork.m_Count && drains==3);
  SMS_ListDestroy(&filtered,0);
 } else if(!strcmp(argv[1],"reset")) {
  _reset(&menu,0);
  assert(redraws==1 && drains>=2 && !filtered.m_Size && !menu.m_Artwork.m_Count);
  menu.Cleanup((GUIObject *)&menu);
 } else {
  assert(!strcmp(argv[1],"folder-list") || !strcmp(argv[1],"folder-grid"));
  folder_navigation(!strcmp(argv[1],"folder-grid"));
 }
 return 0;
}
'''

source = prefix + types + adapters
source += "\n".join(function(lists, name) for name in ("SMS_ListPushBack", "SMS_ListPopBack", "_find", "SMS_ListFind"))
source += function(gui, "GUIObject_Cleanup")
source += function(browser, "GUIFileMenu_CleanupPackets") + function(browser, "GUIFileMenu_Cleanup")
source += function(browser, "_artwork_position") + reset_adapter
source += function(browser, "_reset") + function(browser, "_fill")
# Only widen the EE pointer transport used by the level stack for this host;
# production filenames, node allocation/free and restoration code are unchanged.
source += function(browser, "_push_state").replace("( unsigned int )lpInfo", "( uintptr_t )lpInfo")
source += function(browser, "_pop_state").replace("( _LevelInfo* )( unsigned int )", "( _LevelInfo* )( uintptr_t )")
source += function(browser, "_action_folder") + function(browser, "_artwork_move")
source += function(browser, "GUIFileMenu_HandleEvent").replace("( _LevelInfo* )( unsigned int )", "( _LevelInfo* )( uintptr_t )")
source += function(gui, "_cleanup") + post
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    (tmp / "tamtypes.h").write_text("#include <stdint.h>\ntypedef uint64_t u64;typedef int64_t s64;\n")
    fixture = tmp / "artwork_lifecycle.c"
    fixture.write_text(source)
    exe = tmp / "artwork_lifecycle"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-g", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    "-Wno-int-to-pointer-cast", "-I", str(tmp), "-I", str(root / "include"),
                    str(fixture), str(root / "src/SMS_Artwork.c"), "-o", str(exe)], check=True)
    failures = []
    for case in ("redraw", "reset", "folder-list", "folder-grid"):
        if subprocess.run([str(exe), case], timeout=15).returncode:
            failures.append(case)
    if failures:
        raise SystemExit("Artwork lifecycle controls failed: " + ", ".join(failures))
print("Artwork lifecycle: redraw reuse, destruction, folder entry/back, refresh and empty-media reset passed")
