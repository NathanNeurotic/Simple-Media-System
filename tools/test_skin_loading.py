"""Fault injection for actual skin loading and its IPU DMA-packet constructor.

Hardware decode/GS output is stubbed: this verifies bounds, allocation failures,
CWD/card routing and the size of DMA input storage, not console rendering.
Set SMS_SKIN_SOURCE_REF to an older Git ref to check an independent baseline.
"""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def source(path):
    ref = os.environ.get("SMS_SKIN_SOURCE_REF")
    if ref:
        return subprocess.check_output(["git", "show", f"{ref}:{path}"], cwd=ROOT, text=True)
    return (ROOT / path).read_text()


def function(text, name):
    match = re.search(r"(?:static )?(?:void|int|unsigned short) " + name + r" \(", text)
    assert match, name
    end = text.index("}  /* end " + name + " */", match.start())
    return text[match.start():end + len("}  /* end " + name + " */")]


desktop = source("src/SMS_GUIDesktop.c")
ipu = source("src/SMS_IPU_1.c")
harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
typedef uint64_t u64;
typedef int64_t s64;
typedef uint32_t uint32_t;
typedef struct { unsigned EOP; } GIFTag;
typedef struct { unsigned ID; } DMATag;
typedef void GSTexSpritePacket;
typedef struct IPULoadImage {
 u64 *m_pDMA; unsigned char *m_pData;
 unsigned short m_Width, m_Height, m_QWC, m_fPal;
 unsigned m_Pal[16]; void (*Destroy)(struct IPULoadImage *);
} IPULoadImage;
static struct { unsigned m_Width,m_Height,m_TBW,m_VRAMTexPtr,m_TW,m_TH; } g_GSCtx;
static struct { char m_SkinName[64]; } g_Config;
static char g_pBootDir[128], *g_SlashStr="/", *g_pSMI=".smi", *g_pSMSSkn="mc0:/SMS/Skins";
static int g_MCSlot;
static int allocations, fail_at, live, reads, closes, opens, loads, renders;
static size_t last_alloc_size;
static s64 file_size;
static int read_failure, rewind_failure, missing_cwd;
static unsigned short width=800,height=600;
static void *test_malloc(size_t size) {
 ++allocations;
 if (fail_at<0 || allocations==fail_at) return NULL;
 void *p=malloc(size); assert(p); ++live;
 last_alloc_size=size;
 return p;
}
static void test_free(void *p) {
 if (p) { --live; free(p); }
}
#define malloc test_malloc
#define free test_free
#define DMA_TAG(...) 0ULL
#define VIF_DIRECT(...) 0ULL
#define GIF_TAG(...) 0ULL
#define GIFTAG_REGS_AD 0ULL
#define GS_SET_BITBLTBUF(...) 0ULL
#define GS_BITBLTBUF 0ULL
#define GS_SET_TRXREG(...) 0ULL
#define GS_TRXREG 0ULL
#define GS_TRXPOS 0ULL
#define GS_SET_TRXDIR(...) 0ULL
#define GS_TRXDIR 0ULL
#define DMATAG_ID_REFE 0
static int check_cache;
static void test_cache_flush(const void *start,const void *end) {
 if (check_cache) assert((const unsigned char *)end-(const unsigned char *)start==(int)last_alloc_size);
}
#define SyncDCache test_cache_flush
static unsigned SMS_unaligned32(const void *p) { unsigned n; memcpy(&n,p,4); return n; }
static unsigned SMS_bswap32(unsigned n) { return __builtin_bswap32(n); }
static int fioOpen(const char *path,int flags) {
 (void)flags; ++opens;
 return !strncmp(path,"mass",4) ? missing_cwd ? -1 : 1 : 2;
}
static s64 fioLseek(int fd,s64 pos,int mode) {
 (void)fd;(void)pos; return mode==SEEK_END ? file_size : rewind_failure ? -1 : 0;
}
static int fioRead(int fd,void *data,int size) {
 unsigned char header[8]={0,0,1,0xb2,(unsigned char)(width>>8),(unsigned char)width,
                        (unsigned char)(height>>8),(unsigned char)height};
 ++reads;
 if (read_failure==1 || (read_failure==2 && fd==1)) return size-1;
 memset(data,0x55,size);
 memcpy(data,header,size<8 ? size : 8);
 return size;
}
static int fioClose(int fd) { (void)fd; ++closes; return 0; }
static int MC_OpenS(int slot,int port,const char *path,int flags) { (void)slot;(void)port; return fioOpen(path,flags); }
#define MC_SeekS fioLseek
#define MC_ReadS fioRead
#define MC_CloseS fioClose
static void PowerOf2(int w,int h,int *tw,int *th) { (void)w;(void)h; *tw=10;*th=10; }
static void IPU_LoadImage(IPULoadImage *ctx,void *data,int size,int a,int b,int c,int d,int e) {
 (void)ctx;(void)a;(void)b;(void)c;(void)d;(void)e;
 ++loads;
 /* DMA_Send consumes whole quadwords, including bytes after the file's last byte. */
 volatile unsigned char last=((unsigned char *)data)[((size+15)&~15)-1];
 if (size & 15) assert(last==0);
 assert(!memcmp(data,"\x00\x00\x01\xb2",4));
 assert(ctx->m_Width==width && ctx->m_Height==height);
}
static void SMS_SetPalette(void *p) { (void)p; }
static u64 packet[64];
#define GS_TSP_PACKET_SIZE() 16
#define GS_VGR_PACKET_SIZE() 16
#define GSPaintMethod_Init 0
#define GSPaintMethod_Continue 0
#define GSVideoMode_DTV_1920x1080I 1
#define GS_SET_RGBAQ(...) 0ULL
#define GSFlushMethod_DeleteLists 0
static u64 *GSContext_NewPacket(int a,int b,int c) { (void)a;(void)b;(void)c; return packet+2; }
static void GSContext_RenderTexSprite(GSTexSpritePacket *p,int a,int b,int c,int d,int e,int f,int g,int h) {
 (void)p;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h; ++renders;
}
static void GSContext_RenderVGRect(u64 *p,int a,int b,int c,int d,u64 e,u64 f) {
 (void)p;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;
}
static struct { int m_GSCRTMode; } gs_params;
#define GS_Params() (&gs_params)
static void GSContext_Flush(int a,int b) { (void)a;(void)b; }
static void GUI_SetColors(void) {}
static void GSFont_Init(void) {}
"""
harness += "\n\n".join(function(ipu, name) for name in ("_destroy", "IPU_InitLoadImage", "IPU_ImageInfo"))
harness += function(desktop, "_bgTexUnsafe") + "\n" + function(desktop, "DrawSkin")
harness += r"""
int main(int argc,char **argv) {
 assert(argc==2);
 int numeric_short=!strncmp(argv[1],"short-",6) && argv[1][6]>='0' && argv[1][6]<='9';
 g_GSCtx.m_Width=640;g_GSCtx.m_Height=512;
 strcpy(g_pBootDir,"mass0:/SMS/");strcpy(g_Config.m_SkinName,"sample");
 file_size=32;
 if(!strcmp(argv[1],"short-header")) file_size=4;
 else if(numeric_short) file_size=atoi(argv[1]+6);
 else if(!strcmp(argv[1],"negative-size")) file_size=-1;
 else if(!strcmp(argv[1],"cache-range")) check_cache=1;
 else if(!strcmp(argv[1],"dma-tail")) file_size=17;
 else if(!strcmp(argv[1],"width-17")) width=17;
 else if(!strcmp(argv[1],"width-1")) width=1;
 else if(!strcmp(argv[1],"width-0")) width=0;
 else if(!strcmp(argv[1],"height-0")) height=0;
 else if(!strcmp(argv[1],"large-width")) { width=2048; height=16; }
 else if(!strcmp(argv[1],"large-height")) { width=16; height=2048; }
 else if(!strcmp(argv[1],"overflow-dimensions")) width=height=65535;
 else if(!strcmp(argv[1],"data-allocation")) fail_at=-1;
 else if(!strcmp(argv[1],"packet-allocation")) fail_at=2;
 else if(!strcmp(argv[1],"short-read")) read_failure=1;
 else if(!strcmp(argv[1],"cwd-short-read")) read_failure=2;
 else if(!strcmp(argv[1],"card-short-read")) { read_failure=1; missing_cwd=1; }
 else if(!strcmp(argv[1],"failed-rewind")) rewind_failure=1;
 else if(!strcmp(argv[1],"max-dma-size")) file_size=0xFFFF0;
 else if(!strcmp(argv[1],"too-large")) file_size=0x100000;
 else if(!strcmp(argv[1],"card-fallback")) missing_cwd=1;
 else assert(!strcmp(argv[1],"valid"));
 int expected=(!strcmp(argv[1],"valid")||!strcmp(argv[1],"cache-range")||!strcmp(argv[1],"dma-tail")||!strcmp(argv[1],"card-fallback")||!strcmp(argv[1],"max-dma-size")||!strcmp(argv[1],"cwd-short-read"));
 unsigned char old_gs[sizeof(g_GSCtx)];
 memcpy(old_gs,&g_GSCtx,sizeof(g_GSCtx));
 assert(DrawSkin()==expected);
 if(!expected)
  assert(!memcmp(old_gs,&g_GSCtx,sizeof(g_GSCtx)));
 assert(live==0 && closes==opens-(missing_cwd?1:0));
 assert(loads==expected && renders==expected);
 if(numeric_short||!strcmp(argv[1],"short-header")||!strcmp(argv[1],"negative-size")||!strcmp(argv[1],"too-large")||!strcmp(argv[1],"failed-rewind"))
  assert(reads==0 && allocations==0);
 if(!strcmp(argv[1],"short-read")||!strcmp(argv[1],"cwd-short-read")) assert(reads==2);
 if(!strcmp(argv[1],"card-short-read")) assert(reads==1);
 return 0;
}
"""

cases = sys.argv[1:] or ["valid", "short-header", "cache-range", "negative-size", "dma-tail", "width-1", "width-17", "width-0", "height-0",
                         "large-width", "large-height", "overflow-dimensions", "data-allocation", "packet-allocation",
                         "short-read", "cwd-short-read", "card-short-read", "failed-rewind", "max-dma-size", "too-large", "card-fallback"] + [f"short-{size}" for size in range(8)]
with tempfile.TemporaryDirectory() as temp:
    src = Path(temp) / "skin.c"
    src.write_text(harness)
    for bdm in (0, 1):
        exe = Path(temp) / f"skin-{bdm}"
        cmd = [os.environ.get("CC", "cc"), "-g", "-no-pie", "-fsanitize=address,undefined",
               "-fno-sanitize-recover=all", "-Wall", "-Wextra", "-Wno-unused-function", "-Werror", str(src), "-o", str(exe)]
        if bdm:
            cmd.append("-DBDM")
        subprocess.run(cmd, check=True)
        for case in cases:
            subprocess.run([str(exe), case], check=True, timeout=10)
print(f"Skin loading: {len(cases)} cases passed with BDM on/off (CWD/card paths, header/dimensions, DMA padding and allocation/read failures)")
