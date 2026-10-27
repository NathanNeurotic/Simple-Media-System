"""Actual CDDA stream/read/seek fallback with injected read and sync failures."""
import os
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "src/SMS_FileContext.c").read_text()

def extract(name):
    start = source.index("static int " + name + " (")
    if name == "File_AllocStreamBuffers":
        end = source.index("\nstatic int CDDA_Stream", start)
    else:
        marker = "}  /* end " + name + " */"
        end = source.index(marker, start) + len(marker)
    return source[start:end]

harness = r"""
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef struct { int m_Offset; } CDDAContext;
typedef struct { int m_CurrentSector,m_StartSector,m_EndSector,m_nSectors,m_nSectorsRead,m_Offset;
                 CDDAContext *m_pCtx; } CDDAFilePrivate;
typedef struct FileContext FileContext;
struct FileContext {
 unsigned char *m_pBuff[2],*m_pPos,*m_pEnd;
 unsigned m_CurBuf,m_Size,m_Pos,m_CurPos,m_BufSize,m_CurBufSize;
 void *m_pData;
 int (*Fill)(FileContext *);
 int (*Seek)(FileContext *,unsigned);
 int (*Stream)(FileContext *,unsigned,unsigned);
};
static int failure, allocations;
static void *test_alloc(unsigned align,unsigned n) { (void)align;++allocations;return malloc(n); }
#define memalign test_alloc
static int CDDA_ReadSectors(CDDAContext *c,int sector,int n,unsigned char *p) {
 (void)c;(void)sector;
 if(failure==1) return 0;
 memset(p,0x5a,n*2352);return 1;
}
static int CDDA_Sync(CDDAContext *c) { (void)c;return failure!=2; }
static int CDDA_SeekStm(FileContext *c,unsigned n) { (void)c;(void)n;return -1; }
static int CDDA_FillStm(FileContext *c) { (void)c;return 0; }
""" + "\n".join(extract(n) for n in ("File_AllocStreamBuffers", "CDDA_Fill", "CDDA_Seek", "CDDA_Stream", "CDDA_Read")) + r"""
int main(void) {
 for(int reuse=0;reuse<=1;++reuse) for(int fail=1;fail<=2;++fail) {
  FileContext ctx={0};CDDAContext disc={0};
  CDDAFilePrivate priv={0};unsigned char data[16];
  priv.m_pCtx=&disc;priv.m_EndSector=100;
  ctx.m_pData=&priv;ctx.m_Size=200000;ctx.m_Pos=8192;ctx.m_CurPos=100;
  ctx.Fill=CDDA_Fill;ctx.Seek=CDDA_Seek;ctx.Stream=CDDA_Stream;
  assert(File_AllocStreamBuffers(&ctx,reuse?4*2352:4096,2));
  /* Both a replacement and reuse of a live second buffer must recover. */
  ctx.m_CurBuf=1;ctx.m_pPos=ctx.m_pBuff[1]+10;ctx.m_pEnd=ctx.m_pBuff[1]+100;
  failure=fail;allocations=0;
  assert(CDDA_Stream(&ctx,15000,4)==0);
  assert(allocations==(reuse?0:2));
  assert(ctx.m_CurBuf==0 && ctx.m_pPos==ctx.m_pBuff[0] && ctx.m_pEnd==ctx.m_pBuff[0]);
  failure=0;
  assert(CDDA_Stream(&ctx,15000,0)==1 && ctx.m_CurPos==15000);
  assert(ctx.m_pPos>=ctx.m_pBuff[0] && ctx.m_pEnd<=ctx.m_pBuff[0]+ctx.m_BufSize);
  assert(ctx.m_pPos<ctx.m_pEnd);
  assert(CDDA_Read(&ctx,data,sizeof(data))==sizeof(data));
  for(unsigned i=0;i<sizeof(data);++i) assert(data[i]==0x5a);
  assert(ctx.m_CurPos==15016);
  free(ctx.m_pBuff[0]);free(ctx.m_pBuff[1]);
 }
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / "cdda.c", Path(tmp) / "cdda"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
print("CDDA: replacement/reuse read and sync failures recover through actual ordinary seek/read")
