"""Exercise the actual Play-all menu and object destruction under sanitizers."""
from pathlib import Path
import os
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'src/SMS_GUICmdProc.c').read_text();start=s.index('static int _playlist_menu ( SMS_List* apList')
end=s.index('}  /* end _playlist_menu */',start)+len('}  /* end _playlist_menu */')
menu=s[start:end]
s=(root/'src/SMS_GUI.c').read_text();start=s.index('void GUI_DeleteObject (')
end=s.index('}  /* end GUI_DeleteObject */',start)+len('}  /* end GUI_DeleteObject */')
delete=s[start:end].replace('( unsigned int )','( uintptr_t )')
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef uint64_t u64;
#define _STR(n) ((n)->name)
#define GUIcon_Browser 1
struct SMS_ListNode { uintptr_t m_Param;struct SMS_ListNode *m_pNext;char name[32]; };
typedef struct SMS_ListNode SMS_ListNode;
typedef struct { SMS_ListNode *m_pHead,*m_pTail; } SMS_List;
typedef struct { int m_Len;char *m_pStr; } SMString;
typedef struct GUIMenu GUIMenu;
typedef GUIMenu GUIObject;
typedef struct { SMString *m_pOptionName;int m_IconLeft;void (*Handler)(GUIMenu *,int); } GUIMenuItem;
typedef struct { SMString *m_pTitle;GUIMenuItem *m_pItems,*m_pFirst,*m_pCurr,*m_pLast; } GUIMenuState;
struct GUIMenu { int (*HandleEvent)(GUIObject *,u64);void (*Cleanup)(GUIObject *);unsigned m_Color;
 int m_X,m_Y,m_Width,m_Height,m_IGroup;SMS_ListNode *m_pActiveObj;SMS_List *m_pState;
 void (*Redraw)(GUIMenu *);void *m_pUserData;GUIMenuState *state; };
static struct { int m_Width,m_Height; } g_GSCtx={640,480};
static SMS_ListNode *g_pActiveNode;
static SMS_List objects,*s_pObjectList=&objects;
static GUIMenu *s_pMenu;
static SMString STR_SELECT_FILE={6,"Select"};
#define GUICON_AVI 1
static int (*CtxMenu_HandleEventBase)(GUIObject *,u64);
static int CtxMenu_HandleEvent(GUIObject *obj,u64 event) { (void)obj;(void)event;return 0; }
static void _file_handler(GUIMenu *menu,int direction) { (void)menu;(void)direction; }
static void _playlist_menu_redraw(GUIMenu *menu) { (void)menu; }
static SMS_List *SMS_ListInit(void) { return calloc(1,sizeof(SMS_List)); }
static SMS_ListNode *SMS_ListFind(SMS_List *list,const char *name) {
 for(SMS_ListNode *n=list->m_pHead;n;n=n->m_pNext)if(!strcmp(n->name,name))return n;return NULL;
}
static void SMS_ListRemove(SMS_List *list,SMS_ListNode *node) { assert(list->m_pHead==node);list->m_pHead=list->m_pTail=NULL;free(node); }
static void cleanup(GUIObject *obj) { free(obj->m_pState);free(obj->state); }
static GUIObject *GUI_CreateMenu(void) { GUIMenu *m=calloc(1,sizeof(*m));m->Cleanup=cleanup;return m; }
static GUIMenuState *GUI_MenuPushState(GUIMenu *m) { return m->state=calloc(1,sizeof(GUIMenuState)); }
static void GUIMenu_SelectItemByName(GUIMenu *m,const char *name) {
 for(GUIMenuItem *p=m->state->m_pFirst;p<=m->state->m_pLast;++p)
  if(!strcmp(p->m_pOptionName->m_pStr,name)) { m->state->m_pCurr=p;return; }
 abort();
}
static void GUI_AddObject(const char *name,GUIObject *obj) {
 SMS_ListNode *n=calloc(1,sizeof(*n));strcpy(n->name,name);n->m_Param=(uintptr_t)obj;objects.m_pHead=objects.m_pTail=n;
}
static int menu_result, choose_last, expected_selection;
static void GUI_Run(void) {
 assert(s_pMenu->state->m_pCurr == s_pMenu->state->m_pFirst + expected_selection);
 if(choose_last) s_pMenu->state->m_pCurr=s_pMenu->state->m_pLast;
 s_pMenu->m_pUserData=(void *)(uintptr_t)menu_result;
}
'''
post=r'''
int main(void) {
 SMS_ListNode nodes[2]={{0}};
 strcpy(nodes[0].name,"first.avi");strcpy(nodes[1].name,"last.avi");
 nodes[0].m_pNext=&nodes[1];SMS_List list={nodes,nodes+1};
 SMS_ListNode *selected=&nodes[0];
 /* Initial confirmation can choose a different file. */
 menu_result=0;choose_last=1;expected_selection=0;
 assert(_playlist_menu(&list,&selected)==0 && selected==nodes+1);
 assert(!objects.m_pHead && !objects.m_pTail);
 /* After Stop, reopening keeps the last selected file; cancelling returns 1. */
 menu_result=1;choose_last=0;expected_selection=1;
 assert(_playlist_menu(&list,&selected)==1 && selected==nodes+1);
 assert(!objects.m_pHead && !objects.m_pTail);
 /* Confirm again after cancellation to exercise repeated object teardown. */
 menu_result=0;
 assert(_playlist_menu(&list,&selected)==0 && selected==nodes+1);
 assert(!objects.m_pHead && !objects.m_pTail);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
 src,exe=Path(tmp)/"menu.c",Path(tmp)/"menu"
 src.write_text(prefix+delete+menu+post)
 subprocess.run([os.environ.get("CC","cc"),"-g","-no-pie","-Wno-pointer-to-int-cast",
                 "-fsanitize=address,undefined","-fno-sanitize-recover=all",str(src),"-o",str(exe)],check=True)
 subprocess.run([str(exe)],check=True,timeout=15)
print("Playlist menu: confirmation, cancellation, selection after Stop and repeated teardown passed")
