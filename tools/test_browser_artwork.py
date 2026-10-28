"""Actual artwork cache, lookup/loader, navigation and upload packet controls.

Compiles the production portable helper and selected browser functions under
ASan/UBSan. JPEG, device I/O and GS execution are stubbed: this does not certify
on-console rendering, decoder acceptance or storage timing.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
browser = (root / "src/SMS_GUIFileMenu.c").read_text()
dma = (root / "include/SMS_DMA.h").read_text()


def extract(name):
    start = browser.rfind("\nstatic ", 0, browser.index(name)) + 1
    brace = browser.index("{", browser.index(name))
    depth = 1
    end = brace + 1
    while depth:
        depth += (browser[end] == "{") - (browser[end] == "}")
        end += 1
    return browser[start:end]


prefix = r'''
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <strings.h>
#include "SMS_Artwork.h"
#include "SMS_GS.h"
#include "SMS_List.h"
#define GUICON_AVI 2
#define GUICON_MP3 4
#define GUICON_M3U 6
#define GUICON_AVIS 12
#define DMAC_VIF0 0
#define DMAC_VIF1 1
#define DMAC_GIF 2
#define DMATAG_ID_CNT 1
#define DMATAG_ID_REF 3
#define DMATAG_ID_RET 6
#define VIF_DIRECT(n) ((uint64_t)(0x50000000U | (n)) << 32)
typedef struct { SMS_List *m_pFiltList;SMS_ListNode *m_pCurrent,*m_pFirst,*m_pLast;int m_YOffset; } GUIFileMenu;
static SMS_ListNode* SMS_ListAt_stub(SMS_List *l,int i) {
 SMS_ListNode *n=l->m_pHead;while(n && i-->0)n=n->m_pNext;return n;
}
#define SMS_ListAt SMS_ListAt_stub
static SMS_ListNode* SMS_ListFindI_stub(SMS_List *l,const char *s) {
 for(SMS_ListNode *n=l->m_pHead;n;n=n->m_pNext)if(!strcasecmp(_STR(n),s))return n;return NULL;
}
#define SMS_ListFindI SMS_ListFindI_stub
typedef struct FileContext { unsigned m_Size;int (*Read)(struct FileContext*,void*,unsigned);
 void (*Destroy)(struct FileContext*); } FileContext;
typedef struct { unsigned m_NewWidth,m_NewHeight; } SMS_RescaleContext;
typedef struct { SMS_RescaleContext *m_pRC;unsigned m_nBitmapBytes;unsigned char *m_pBitmap; } SMS_JPEGContext;
static int opens,closes,decodes,suspends,read_short,decode_fail,allocation_fail,open_fail,gs_restored;
static unsigned source_size;
static unsigned char source[64];
static unsigned char uncached_pixels[24];
static void *cached_bitmap;
static void *uncached_bitmap(void *p) { assert(p==cached_bitmap);return uncached_pixels; }
#define UNCACHED_SEG(p) uncached_bitmap(p)
static char expected_path[1024],g_CWD[1024];
static int g_CMedia;
static void *g_pCDDACtx;
static SMS_List *g_pFileList;
static void DMA_Wait(int channel) { (void)channel; }
static void SMS_GUIClockSuspend(void) { assert(!suspends);suspends=1; }
static void SMS_GUIClockResume(void) { assert(suspends&&gs_restored);suspends=0; }
static void _artwork_drain(void) {}
static void _artwork_restore_gs(void) { gs_restored=1; }
static void SyncDCache(void *a,void *b) { (void)a;(void)b; }
static void destroy(FileContext *f) { ++closes;free(f); }
static int read_file(FileContext *f,void *p,unsigned size) {
 (void)f;assert(size<=sizeof(source));memcpy(p,source,size);return size-read_short;
}
static FileContext *STIO_InitFileContext(const char *path,void *arg) {
 (void)arg;assert(!strcmp(path,expected_path));if(open_fail)return NULL;++opens;
 FileContext *f=calloc(1,sizeof(*f));f->m_Size=source_size;f->Read=read_file;f->Destroy=destroy;return f;
}
static FileContext *CDDA_InitFileContext(const char *path,void *arg) { return STIO_InitFileContext(path,arg); }
static SMS_JPEGContext *SMS_JPEGInit(void *a,void *b) {
 (void)a;(void)b;gs_restored=0;return calloc(1,sizeof(SMS_JPEGContext));
}
static int SMS_JPEGLoad(SMS_JPEGContext *j,void *p,unsigned size) {
 (void)p;(void)size;++decodes;assert(suspends);if(decode_fail)return 0;
 j->m_pRC=calloc(1,sizeof(*j->m_pRC));j->m_pRC->m_NewWidth=2;j->m_pRC->m_NewHeight=4;
 j->m_pBitmap=malloc(24);memset(j->m_pBitmap,0xee,24);cached_bitmap=j->m_pBitmap;
 memset(uncached_pixels,0x40,24);j->m_nBitmapBytes=24;return 1;
}
static void SMS_JPEGDestroy(SMS_JPEGContext *j) { if(j){free(j->m_pBitmap);free(j->m_pRC);free(j);} }
static void *test_memalign(size_t align,size_t size) { return allocation_fail && !--allocation_fail ? NULL : memalign(align,size); }
#define memalign test_memalign
u64 GS_XYZ(int x,int y,int z) { return (u64)x|((u64)y<<16)|((u64)z<<32); }
'''
tag_start = dma.index("#define DMA_TAG(")
tag_end = dma.index("\n\n", tag_start)
functions = "\n".join(extract(name) for name in ["_artwork_position", "_artwork_move", "_artwork_load", "_artwork_packet"])

post = r'''
#undef memalign
static SMS_ListNode *node(const char *name,int type) {
 SMS_ListNode *n=calloc(1,sizeof(*n)+strlen(name)+1);strcpy(_STR(n),name);n->m_Param=type;return n;
}
static unsigned char* cache_load(const void *key,void *arg) {
 int *calls=arg;++*calls;return key==(void *)2?NULL:malloc(SMS_ARTWORK_BYTES);
}
static void fixture(void) {
 unsigned char bytes[]={0xff,0xd8,0xff,0xc0,0,17,8,0,4,0,2,3,1,0x11,0,2,0x11,0,3,0x11,0};
 memcpy(source,bytes,sizeof(bytes));source_size=sizeof(bytes);
}
int main(void) {
 char name[32];fixture();
 assert(SMS_ArtworkName(name,sizeof(name),"Movie.part.avi",0)&&!strcmp(name,"Movie.part.jpg"));
 assert(SMS_ArtworkName(name,sizeof(name),"Movie.mp4",1)&&!strcmp(name,"Movie.jpeg"));
 assert(!SMS_ArtworkName(name,5,"Movie.avi",0));assert(!SMS_ArtworkName(name,sizeof(name),"../a.avi",0));
 assert(!SMS_ArtworkName(name,sizeof(name),".avi",0));
 assert(SMS_ArtworkJPEGSize(source,source_size));
 for(unsigned i=0;i<source_size;++i)assert(!SMS_ArtworkJPEGSize(source,i));
 source[9]=2;source[10]=1;assert(!SMS_ArtworkJPEGSize(source,source_size));fixture();
 source[3]=0xc2;assert(!SMS_ArtworkJPEGSize(source,source_size));fixture();
 assert(!SMS_ArtworkJPEGSize(source,SMS_ARTWORK_MAX_FILE+1));
 unsigned char rgb[6]={200,50,10,20,150,90},thumb[SMS_ARTWORK_BYTES];
 assert(SMS_ArtworkThumbnail(thumb,rgb,1,2));
 assert(thumb[0]==0&&thumb[3]==0x80); /* centered portrait, opaque letterbox */
 assert(thumb[(16)*4]==200&&thumb[(63*64+16)*4]==20);
 assert(!SMS_ArtworkThumbnail(thumb,rgb,0,2));
 assert(!SMS_ArtworkThumbnail(thumb,rgb,513,2));
 int calls=0;SMS_ArtworkCache cache={0};const void *keys[6]={(void*)1,(void*)2,(void*)3,(void*)4,(void*)5,(void*)6};
 SMS_ArtworkPage(&cache,keys,6);
 for(int repeat=0;repeat<10;++repeat)for(unsigned i=0;i<6;++i)SMS_ArtworkGet(&cache,i,cache_load,&calls);
 assert(calls==6);assert(!SMS_ArtworkGet(&cache,6,cache_load,&calls));
 SMS_ArtworkPage(&cache,keys,6);assert(calls==6&&cache.m_Slots[0].m_pPixels);
 SMS_ArtworkPage(&cache,keys+2,4);assert(cache.m_Count==4&&!cache.m_Slots[0].m_Attempted);
 SMS_ArtworkGet(&cache,0,cache_load,&calls);assert(calls==7);SMS_ArtworkClear(&cache);assert(!cache.m_Count);
 assert(SMS_ArtworkMove(0,0,3)==0);assert(SMS_ArtworkMove(13,0,-3)==0);
 assert(SMS_ArtworkMove(13,5,3)==8);assert(SMS_ArtworkMove(13,10,6)==12);
 assert(SMS_ArtworkMove(13,3,INT_MIN)==0);assert(SMS_ArtworkMove(13,3,INT_MAX)==12);
 SMS_List list={0};SMS_ListNode *items[13];
 for(int i=0;i<13;++i){items[i]=node("movie.avi",GUICON_AVI);if(i){items[i-1]->m_pNext=items[i];items[i]->m_pPrev=items[i-1];}}
 list.m_pHead=items[0];list.m_pTail=items[12];list.m_Size=13;
 GUIFileMenu menu={.m_pFiltList=&list,.m_pCurrent=items[5]};_artwork_position(&menu);
 assert(menu.m_pFirst==items[0]&&menu.m_pLast==items[5]&&menu.m_YOffset==5);
 _artwork_move(&menu,1);assert(menu.m_pCurrent==items[6]&&menu.m_pFirst==items[6]&&menu.m_YOffset==0);
 _artwork_move(&menu,6);assert(menu.m_pCurrent==items[12]&&menu.m_pLast==items[12]);
 _artwork_move(&menu,-6);assert(menu.m_pCurrent==items[6]);
 menu.m_pCurrent=NULL;_artwork_position(&menu);assert(menu.m_pCurrent==items[0]);
 SMS_ListNode *media=node("movie.avi",GUICON_AVI),*jpg=node("Movie.JPG",16),*jpeg=node("movie.jpeg",16);
 jpg->m_pNext=jpeg;SMS_List art={.m_pHead=jpg,.m_pTail=jpeg,.m_Size=2};g_pFileList=&art;
 strcpy(g_CWD,"smb:/Movies");strcpy(expected_path,"smb:/Movies/Movie.JPG");
 unsigned char *image=_artwork_load(media,NULL);assert(image&&opens==1&&closes==1&&decodes==1&&!suspends);
 assert(image[16*4]==0x40); /* sample decoded uncached bytes, not stale cached heap */
 free(image);
 read_short=1;assert(!_artwork_load(media,NULL)&&decodes==1&&opens==closes);read_short=0;
 source_size=SMS_ARTWORK_MAX_FILE+1;assert(!_artwork_load(media,NULL)&&decodes==1&&opens==closes);fixture();
 source[9]=2;source[10]=1;assert(!_artwork_load(media,NULL)&&decodes==1);fixture();
 decode_fail=1;assert(!_artwork_load(media,NULL)&&!suspends&&opens==closes);decode_fail=0;
 allocation_fail=1;assert(!_artwork_load(media,NULL)&&opens==closes);allocation_fail=0;
 allocation_fail=2;assert(!_artwork_load(media,NULL)&&opens==closes&&!suspends);allocation_fail=0;
 open_fail=1;assert(!_artwork_load(media,NULL)&&opens==closes);open_fail=0;
 art.m_pHead=jpeg;strcpy(expected_path,"smb:/Movies/movie.jpeg");image=_artwork_load(media,NULL);assert(image);free(image);
 int before=opens;art.m_pHead=NULL;assert(!_artwork_load(media,NULL)&&opens==before);
 media->m_Param=0;assert(!_artwork_load(media,NULL)&&opens==before);media->m_Param=GUICON_AVI;
 u64 packet[32];_artwork_packet(packet,thumb,20,30,90);
 assert((packet[8]&0xfff)==64&&(packet[8]>>32)==64); /* one 64x64 transfer */
 assert((packet[12]&0x7fff)==1024&&(packet[14]&0xffff)==1024);
 assert(((packet[14]>>28)&7)==DMATAG_ID_REF&&((packet[16]>>28)&7)==DMATAG_ID_RET);
 assert(packet[21]==GS_TEXFLUSH&&packet[23]==GS_TEX0_1&&packet[25]==GS_PRIM);
 assert(packet[29]==GS_XYZ(20,30,0)&&packet[31]==GS_XYZ(110,120,0));
 for(int i=0;i<13;++i)free(items[i]);free(media);free(jpg);free(jpeg);return 0;
}
'''

with tempfile.TemporaryDirectory() as tmp:
    tmp = Path(tmp)
    (tmp / "tamtypes.h").write_text("#include <stdint.h>\ntypedef uint64_t u64;typedef int64_t s64;\n")
    source = tmp / "browser_artwork.c"
    source.write_text(prefix + dma[tag_start:tag_end] + "\n" + functions + post)
    exe = tmp / "browser_artwork"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-g", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    "-Wno-pointer-to-int-cast", "-I", str(tmp), "-I", str(root / "include"),
                    str(source), str(root / "src/SMS_Artwork.c"), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
print("Browser artwork: lookup/read/decode failures, bounded cache, aspect fit, page navigation and upload ordering passed")
