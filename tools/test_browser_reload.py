"""Observe actual list/grid render submissions after normal browser reload.

Runs production delete/reload/fill/reset/render/cleanup functions and the artwork
cache. GS/DMA/device calls are host adapters; observations are submitted caption
packets, not console pixels. Only EE pointer transport is widened on the host.
"""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
browser = (Path(sys.argv[1]) if len(sys.argv) > 1 else root / "src/SMS_GUIFileMenu.c").read_text()
gui = (root / "src/SMS_GUI.c").read_text()
lists = (root / "src/SMS_List.c").read_text()
context = (root / "src/SMS_GUIFileCtxMenu.c").read_text()


def function(source, name):
    match = re.search(r"^[^\n;{}]*\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", source, re.M)
    if not match:
        raise ValueError("Missing production function: " + name)
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[match.start():end]


types = browser[browser.index("typedef struct _LevelInfo"):browser.index("static u64", browser.index("typedef struct GUIFileMenu"))]
cwd = "\n".join(line for line in (root / "include/SMS_Sounds.h").read_text().splitlines()
                 if line.startswith("#define SMS_CWD_CAPACITY") or line.startswith("#define g_CWD "))
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "SMS_GUI.h"
#include "SMS_List.h"
#include "SMS_Artwork.h"
#include "SMS_GS.h"
#include "SMS_GUIcons.h"
#include "SMS_PAD.h"
#include "SMS_RC.h"
#define UNCACHED_SEG(p) (p)
#define VIF_DIRECT(n) ((u64)(0x50000000U | (n)) << 32)
#define SMS_BF_AVIF 1
'''
adapters = cwd + r'''
static GUIFileMenu menu;
static SMS_List filtered,packets,listing,levels;
static SMS_List *g_pFileList=&listing;
GUIObject *g_pFileMenu=(GUIObject *)&menu;
GSContext g_GSCtx={.m_Width=640,.m_Height=480};
static GSParams params;
u64 g_Palette[16];
static unsigned char g_SMSounds[2048];
static u64 s_BitBltPack[32];
static char g_EmptyStr[]="",g_DotStr[]=".";
static int g_CMedia,g_PD,g_SMBU;
static struct { int m_PlayerVolume;unsigned m_BrowserFlags,m_BrowserSCIdx,m_BrowserABCIdx,m_BrowserIBCIdx;char m_Partition[64]; } g_Config;
static unsigned phase,progress,reads,redraws,observed_count;
static char observed[16][64];
static void _artwork_drain(void) {}
static unsigned char *_artwork_load(const void *key,void *arg) { (void)key;(void)arg;return NULL; }
static void _artwork_packet(u64 *p,unsigned char *pixels,int x,int y,int side) {
 (void)p;(void)pixels;(void)x;(void)y;(void)side;assert(0);
}
GSParams *GS_Params(void) { return &params; }
void *SMS_SyncMalloc(int size) { return calloc(1,size); }
u64 *GSContext_NewList(unsigned size) { return (u64 *)calloc(size+2,sizeof(u64))+2; }
void GSContext_DeleteList(u64 *p) { if(p)free(p-2); }
void GSContext_CallList(int context,u64 *p) {
 (void)context;
 for(SMS_ListNode *n=packets.m_pHead;n;n=n->m_pNext) {
  _FileMenuItem *item=(_FileMenuItem *)(uintptr_t)n->m_Param;
  if(item->m_pTxtPack==p) { assert(observed_count<16);strcpy(observed[observed_count++],_STR(n)); }
 }
}
void GSContext_CallList2(int context,u64 *p) { (void)context;(void)p; }
void GS_RenderRoundRect(GSRoundRectPacket *p,int x,int y,int w,int h,int radius,s64 color) {
 (void)p;(void)x;(void)y;(void)w;(void)h;(void)radius;(void)color;
}
int GSFont_WidthEx(char *text,int length,int width) { (void)text;(void)width;return length*8; }
void GSFont_RenderEx(char *text,int length,int x,int y,u64 *p,int width,int height) {
 (void)text;(void)length;(void)x;(void)y;(void)p;(void)width;(void)height;
}
int TranslateUTF8(int codepage,char *dst,int length,const char *src) {
 (void)codepage;(void)dst;(void)length;(void)src;return 0;
}
void GUI_DrawIcon(unsigned icon,int x,int y,GUIcon type,u64 *p) {
 (void)icon;(void)x;(void)y;(void)type;(void)p;
}
static void SMS_InitBitBlt(void *p,int count,int y,int h) { (void)p;(void)count;(void)y;(void)h; }
static void SMS_PgIndStart(void) { assert(!progress);progress=1; }
static void SMS_PgIndStop(void) { assert(progress);progress=0; }
static int _filter_item(SMS_ListNode *p) { (void)p;return 0; }
static void SMS_FileDirInit(char *path) {
 assert(progress && !strcmp(path,"."));++reads;
 SMS_ListDestroy(&listing,0);
 if(!phase)SMS_ListPushBack(&listing,"removed.avi")->m_Param=2;
 if(phase<2) {
  SMS_ListPushBack(&listing,"keep.avi")->m_Param=2;
  SMS_ListPushBack(&listing,"another.avi")->m_Param=2;
 } else if(phase==3)SMS_ListPushBack(&listing,"new.avi")->m_Param=2;
}
#define SMSound_PAD 0
static void SPU_PlaySound(int sound,int volume) { (void)sound;(void)volume; }
static void CDVD_FlushCache(void) {}
void GUI_PostMessage(u64 event) { (void)event; }
static int _step_up(GUIFileMenu *p,int roll) { (void)p;(void)roll;assert(0);return 0; }
static int _step_down(GUIFileMenu *p,int roll) { (void)p;(void)roll;assert(0);return 0; }
static int _page_scroll(int (*step)(GUIFileMenu *,int),GUIFileMenu *p) { (void)step;(void)p;assert(0);return 0; }
static void _context_action_file(GUIFileMenu *p,int popup) { (void)p;(void)popup;assert(0); }
static void _action_partition(GUIFileMenu *p,int popup) { (void)p;(void)popup;assert(0); }
static void _pop_state(GUIFileMenu *p) { (void)p;assert(0); }
static void (*Action[9])(GUIFileMenu *,int);
static void (*ContextAction[9])(GUIFileMenu *,int);
typedef struct GUIMenu GUIMenu;
static int s_Action=2;
static char *s_pFileName="removed.avi";
static int fioRemove(const char *name) { assert(!strcmp(name,"removed.avi"));phase=1;return 0; }
static void SMS_DeleteTree(const char *path) { (void)path;assert(0); }
'''
redraw_adapter = r'''
static void _redraw(GUIFileMenu *p,int all) {
 (void)all;++redraws;
 GUIFileMenu_CleanupPackets((GUIObject *)p);
 observed_count=0;
 GUIFileMenu_Render((GUIObject *)p,0);
}
'''
post = r'''
static void expect_names(unsigned count,const char *first) {
 assert(observed_count==count && packets.m_Size==count);
 if(count)assert(!strcmp(observed[0],first));
}
int main(int argc,char **argv) {
 assert(argc==3);
 unsigned after_reload=(unsigned)atoi(argv[2]);
 g_Config.m_BrowserSCIdx=g_Config.m_BrowserABCIdx=g_Config.m_BrowserIBCIdx=1;
 menu.m_pColor=&g_Config.m_BrowserABCIdx;
 menu.m_pFiltList=&filtered;menu.m_pFileList=&packets;menu.m_pLevels=&levels;
 menu.m_ArtworkMode=!strcmp(argv[1],"grid");
 menu.HandleEvent=GUIFileMenu_HandleEvent;
 strcpy(g_CWD,"mass0:/");
 _fill(&menu,g_DotStr,0);_redraw(&menu,0);
 expect_names(3,"removed.avi");
 unsigned previous_redraws=redraws;
 /* The production delete callback directly invokes the reload event. */
 _delete_handler(NULL,1);
 assert(reads==2&&filtered.m_Size==2&&menu.m_pCurrent==filtered.m_pHead);
 printf("%s direct delete/reload: filtered=%u submitted=%u redraws=%u\n",
        argv[1],filtered.m_Size,observed_count,redraws-previous_redraws);
 fflush(stdout);
 expect_names(after_reload,"keep.avi");
 /* A plain Render reuses its cached packet; it cannot repair an empty cached
  * reset panel. The surrounding menu's SetFocus redraw CAN mask the defect. */
 observed_count=0;GUIFileMenu_Render((GUIObject *)&menu,0);
 expect_names(after_reload,"keep.avi");
 GUIFileMenu_SetFocus((GUIObject *)&menu,1);expect_names(2,"keep.avi");
 printf("%s focus redraw: submitted=%u (surrounding context-menu repair)\n",argv[1],observed_count);
 phase=2;GUIFileMenu_HandleEvent((GUIObject *)&menu,GUI_MSG_RELOAD_BROWSER);
 expect_names(0,NULL);
 assert(!filtered.m_Size&&!menu.m_pCurrent&&!menu.m_pFirst&&!menu.m_pLast&&!menu.m_Artwork.m_Count);
 phase=3;GUIFileMenu_HandleEvent((GUIObject *)&menu,GUI_MSG_RELOAD_BROWSER);
 assert(filtered.m_Size==1&&!strcmp(_STR(menu.m_pCurrent),"new.avi"));
 expect_names(after_reload?1:0,"new.avi");
 phase=1;GUIFileMenu_HandleEvent((GUIObject *)&menu,GUI_MSG_RELOAD_BROWSER);
 expect_names(after_reload,"keep.avi");
 GUIFileMenu_Cleanup((GUIObject *)&menu);
 GSContext_DeleteList(menu.m_pSelRect);menu.m_pSelRect=NULL;
 SMS_ListDestroy(&filtered,0);SMS_ListDestroy(&listing,0);SMS_ListDestroy(&levels,0);
 assert(!progress);return 0;
}
'''

source = prefix + types + adapters
source += "\n".join(function(lists, name) for name in
                    ("SMS_ListPushBack", "SMS_ListDestroy", "SMS_ListAt", "_find", "SMS_ListFind"))
source += function(gui, "GUIObject_Cleanup")
source += function(browser, "_artwork_position") + function(browser, "_artwork_move")
source += function(browser, "_is_utf8") + function(browser, "GUIFileMenu_RenderItem")
source += function(browser, "_artwork_render") + function(browser, "GUIFileMenu_Render")
source += function(browser, "GUIFileMenu_CleanupPackets") + function(browser, "GUIFileMenu_Cleanup")
source += redraw_adapter + function(browser, "GUIFileMenu_SetFocus")
source += function(browser, "_reset") + function(browser, "_fill")
source += function(browser, "GUIFileMenu_HandleEvent") + function(context, "_delete_handler") + post
# EE addresses travel through m_Param as unsigned int. Widen only this transport
# so production heap objects retain their addresses on a 64-bit host.
source = source.replace("( unsigned int )lpPack", "( uintptr_t )lpPack")
source = source.replace("( _FileMenuItem* )( unsigned int )", "( _FileMenuItem* )( uintptr_t )")
source = source.replace("( _LevelInfo* )( unsigned int )", "( _LevelInfo* )( uintptr_t )")
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    (tmp / "tamtypes.h").write_text("#include <stdint.h>\ntypedef uint64_t u64;typedef int64_t s64;\n")
    fixture = tmp / "browser_reload.c"
    fixture.write_text(source)
    exe = tmp / "browser_reload"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-g", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    "-I", str(tmp), "-I", str(root / "include"), str(fixture),
                    str(root / "src/SMS_Artwork.c"), "-o", str(exe)], check=True)
    expected = "0" if "--observe-prior" in sys.argv else "2"
    for mode in ("list", "grid"):
        subprocess.run([str(exe), mode, expected], check=True, timeout=15)
print("Browser reload: actual list/grid render submissions, deletion, empty/repopulated directory and focus repair checked")
