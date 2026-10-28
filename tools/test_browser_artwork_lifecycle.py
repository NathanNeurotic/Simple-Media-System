"""Actual browser reset and GUI redraw cleanup under ASan/UBSan/LSan.

Uses the production cache, object layouts and cleanup/reset functions. List
storage and GS/DMA are host adapters; this does not execute a PS2 display chain
or prove selection restoration after returning from a folder or player.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
browser = (root / "src/SMS_GUIFileMenu.c").read_text()
gui = (root / "src/SMS_GUI.c").read_text()


def function(source, name):
    match = re.search(r"^[^\n;{}]*\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", source, re.M)
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


types = browser[browser.index("typedef struct GUIFileMenu"):browser.index("static u64", browser.index("typedef struct GUIFileMenu"))]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "SMS_GUI.h"
#include "SMS_List.h"
#include "SMS_Artwork.h"
'''
adapters = r'''
static GUIFileMenu menu;
static SMS_List filtered,packets,objects;
static SMS_ListNode desktop_node,menu_node,other_node;
static GUIObject desktop,other;
GUIObject *g_pFileMenu=(GUIObject *)&menu;
static SMS_List *s_pObjectList=&objects;
static unsigned drains,generic_cleanups,unloads,redraws;
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
'''
reset_adapter = r'''
static void _redraw(GUIFileMenu *p,int all) {
 (void)all;++redraws;
 assert(!p->m_pFiltList->m_Size && !p->m_pFiltList->m_pHead && !p->m_pFiltList->m_pTail);
 _artwork_position(p);
 assert(!p->m_pCurrent && !p->m_pFirst && !p->m_pLast && !p->m_YOffset);
 assert(!p->m_Artwork.m_Count);
 GUIFileMenu_CleanupPackets((GUIObject *)p);
}
'''
post = r'''
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
 } else {
  assert(!strcmp(argv[1],"reset"));
  _reset(&menu,0);
  assert(redraws==1 && drains>=2 && !filtered.m_Size && !menu.m_Artwork.m_Count);
  menu.Cleanup((GUIObject *)&menu);
 }
 return 0;
}
'''

source = prefix + types + adapters + function(gui, "GUIObject_Cleanup")
source += function(browser, "GUIFileMenu_CleanupPackets") + function(browser, "GUIFileMenu_Cleanup")
source += function(browser, "_artwork_position") + reset_adapter
source += function(browser, "_reset") + function(gui, "_cleanup") + post
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
    for case in ("redraw", "reset"):
        if subprocess.run([str(exe), case], timeout=15).returncode:
            failures.append(case)
    if failures:
        raise SystemExit("Artwork lifecycle controls failed: " + ", ".join(failures))
print("Artwork lifecycle: ordinary/full GUI redraw reuse, destruction, reset and empty-list selection passed")
