"""Run the production directory scanner with host I/O at path boundaries."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
source = (Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'src/SMS_FileDir.c').read_text()
start = source.find('static int _append_directory_path (')
if start < 0:
    start = source.index('void SMS_FileDirInit (')
end = source.index('}  /* end SMS_FileDirInit */', start)
code = source[start:end] + '}\n'
fixture = r'''
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <malloc.h>
#define SMS_CWD_CAPACITY 1024
#define SMS_BF_SORT 1
#define FIO_SO_IFREG 1
#define FIO_SO_IFDIR 2
#define GUICON_FOLDER 3
#define GUICON_PARTITION 4
#define GUICON_SHARE 5
#define GUICON_AVI 6
#define GUICON_AVIS 7
typedef struct Node { uintptr_t m_Param;struct Node *m_pNext; } SMS_ListNode;
typedef struct { SMS_ListNode *m_pHead; } SMS_List;
#define _STR(n) ((char *)((n)+1))
static struct { int m_BrowserFlags;void *m_pSMBList; } g_Config;
static struct { char *m_pStr; } STR_READING_MEDIA={"reading"};
static char g_CWD[1024],g_HDDWD[1024],g_pUSB[8]="mass0";
static char *g_pDevName[9]={"mass0","cdfs","hdd0","cdda","host","cdfs","smb","mmce0","udpfs"};
static char *g_SlashStr="/",*g_BSlashStr="\\",*g_ColonSStr=": ";
static char *g_pSrtStr="srt",*g_pSubStr="sub",*g_pTxtStr="txt";
static char *g_pCDDAFS="cddafs:/",*s_pELL="host:elflist.txt",*s_pHST="host:";
static int g_CMedia,g_CUnit,g_IOPFlags;
#define SMS_IOPF_UMS 1
static SMS_List *g_pFileList;
typedef struct { char m_Share[256]; } SMBLoginInfo;
typedef struct { char ShareName[256],ShareComment[256]; } ShareEntry_t;
typedef struct { void *EE_addr;int maxent; } smbGetShareList_in_t;
static SMBLoginInfo *_lookup_login_info(void) { return NULL; }
static void SyncDCache(void *a,void *b) { (void)a;(void)b; }
typedef struct Dir { char *m_pName;struct Dir *m_pNext; } CDDADirectory;
typedef CDDADirectory CDDAFile;
static void *g_pCDDACtx;
static const CDDADirectory *CDDA_DirectoryList(void *p) { (void)p;abort(); }
static CDDAFile *CDDA_GetFileList(void *p,const CDDADirectory *d) { (void)p;(void)d;abort(); }
static void CDDA_DestroyFileList(CDDAFile *p) { (void)p;abort(); }
typedef struct FC { void (*Destroy)(struct FC *); } FileContext;
static FileContext *STIO_InitFileContext(char *p,void *q) { (void)p;(void)q;return NULL; }
static void File_GetString(FileContext *p,char *s,int n) { (void)p;(void)s;(void)n;abort(); }
#define FILE_EOF(p) 1
typedef struct { struct { int attr,mode; } stat;char name[256]; } fio_dirent_t;
static unsigned opens,reads,stops,statuses,entry_mode,emit_entry,probes;
static const char *entry_name="file.mpg";
static int fioDopen(const char *s) { assert(strlen(s)<1024);++opens;return 1; }
static int fioDread(int fd,fio_dirent_t *e) {
 (void)fd;++reads;if(!emit_entry)return 0;emit_entry=0;
 memset(e,0,sizeof(*e));e->stat.mode=entry_mode;strcpy(e->name,entry_name);return 1;
}
static void fioDclose(int fd) { (void)fd; }
static int fioOpen(const char *s,int flags) { (void)flags;assert(strlen(s)<1024);++probes;return 1; }
static void fioClose(int fd) { (void)fd; }
static void GUI_Status(const char *s) { (void)s;++statuses; }
static void CDVD_Stop(void) { ++stops; }
static int SMS_FileID(const char *s) { (void)s;return 8; }
static SMS_List *SMS_ListInit(void) { return calloc(1,sizeof(SMS_List)); }
static void SMS_ListDestroy(SMS_List *p,int release) {
 SMS_ListNode *n=p->m_pHead;while(n){SMS_ListNode *next=n->m_pNext;free(n);n=next;}
 p->m_pHead=NULL;if(release)free(p);
}
static SMS_ListNode *SMS_ListPushBack(SMS_List *p,const char *s) {
 SMS_ListNode *n=calloc(1,sizeof(*n)+strlen(s)+1);strcpy(_STR(n),s);
 SMS_ListNode **tail=&p->m_pHead;while(*tail)tail=&(*tail)->m_pNext;*tail=n;return n;
}
static void SMS_ListSort(SMS_List *p) { (void)p; }
static SMS_ListNode *SMS_ListFindI(SMS_List *p,char *s) { (void)p;(void)s;return NULL; }
static void SMS_ListAppend(SMS_List *p,SMS_List *q) {
 SMS_ListNode **tail=&p->m_pHead;while(*tail)tail=&(*tail)->m_pNext;*tail=q->m_pHead;q->m_pHead=NULL;
}
''' + code + r'''
static void scan(unsigned length,int media,char last,char *path,int fits) {
 char before[1024];memset(g_CWD,'d',length);g_CWD[length]=0;
 if(length)g_CWD[length-1]=last;strcpy(before,g_CWD);g_CMedia=media;
 opens=reads=statuses=0;SMS_FileDirInit(path);
 assert(statuses==2);assert(opens==(unsigned)fits);
 if(!fits)assert(!strcmp(before,g_CWD));
}
int main(void) {
 char sentinel[]={1,0};
 for(int media=0;media<9;++media) {
  if(media==3)continue; /* CDDA has no fio directory scan. */
  char slash=media==4?'\\':'/';
  for(unsigned length=1;length<1024;++length) {
   scan(length,media,'d',sentinel,length<1023);
   scan(length,media,slash,sentinel,1);
   scan(length,media,'d',".",length<1023);
  }
 }
 scan(0,0,'d',sentinel,0);
 scan(1022,0,'/',"x",1);assert(strlen(g_CWD)==1023);
 scan(1022,0,'d',"x",0);
 scan(1021,4,'d',"x",1);assert(strlen(g_CWD)==1023);
 scan(1022,4,'d',"x",0);
 /* Known entry types remain listable at the maximum directory length. */
 entry_mode=FIO_SO_IFREG;emit_entry=1;
 scan(1023,0,'/',sentinel,1);assert(g_pFileList->m_pHead);
 /* Untyped entries require a probe whose full path must also fit. */
 entry_mode=0;emit_entry=1;probes=0;
 scan(1023,0,'/',sentinel,1);assert(!probes && !g_pFileList->m_pHead);
 emit_entry=1;scan(1015,0,'/',sentinel,1);assert(probes==1);
 emit_entry=1;scan(1016,0,'/',sentinel,1);assert(probes==1);
 SMS_ListDestroy(g_pFileList,1);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    path = Path(td)
    (path / 'test.c').write_text(fixture)
    exe = path / 'test'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-no-pie',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    str(path / 'test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=60)
print('Production directory scanner: sentinel, separator, entry-probe boundaries passed')
