"""Actual STIO stream/read code: failed I/O, partial reads and prefetch lifetime.

Set SMS_FILE_CONTEXT_SOURCE to a saved SMS_FileContext.c to check a baseline.
The file API stubs model PS2SDK fioRead's NOWAIT dispatch and fioSync's result
separately; no physical device throughput or console playback is measured.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = Path(os.environ.get("SMS_FILE_CONTEXT_SOURCE", root / "src/SMS_FileContext.c")).read_text()


def extract(name):
    start = source.index("static int " + name + " (")
    if name == "File_AllocStreamBuffers":
        end = source.index("\nstatic int CDDA_Stream", start)
    elif name == "STIO_SeekStm":
        end = source.index("\nstatic int STIO_Stream", start)
    else:
        marker = "}  /* end " + name + " */"
        end = source.index(marker, start) + len(marker)
    return source[start:end]


harness = r"""
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define FIO_WAIT 0
#define FIO_NOWAIT 1
#define FIO_COMPLETE 1
#define MX4SIO_RD_CHUNK 4096

#ifdef _WIN32
typedef unsigned int DWORD;
typedef struct { unsigned Offset,OffsetHigh; } OVERLAPPED;
typedef struct { int m_hFile; OVERLAPPED m_Ovlp; } STIOFilePrivate;
#define FILE_BEGIN 0
#define TRUE 1
#define ERROR_IO_PENDING 997
#else
typedef struct { int m_FD,m_fXio,m_RdChunk; } STIOFilePrivate;
#endif
typedef struct FileContext FileContext;
struct FileContext {
 unsigned char *m_pBuff[2],*m_pPos,*m_pEnd;
 unsigned m_CurBuf,m_Size,m_Pos,m_CurPos,m_BufSize,m_CurBufSize;
 void *m_pData;
 int (*Fill)(FileContext *);
 int (*Seek)(FileContext *,unsigned);
 int (*Stream)(FileContext *,unsigned,unsigned);
};
static int block_mode,seek_error,read_error,dispatch_error,completion_error,sync_error;
static unsigned actual_size,native_pos,read_calls,sync_calls;
static int completed_length;
static void *test_alloc(unsigned align,unsigned n) { (void)align;return malloc(n); }
#define memalign test_alloc
static void fioSetBlockMode(int mode) { block_mode=mode; }
static int fioSync(int mode,int *n) {
 (void)mode;++sync_calls;
 if(block_mode!=FIO_NOWAIT || sync_error) return -10;
 *n=completed_length;return FIO_COMPLETE;
}
static int fioLseek(int fd,unsigned n,int whence) {
 (void)fd;(void)whence;
 if(seek_error) return -1;
 native_pos=n;return n;
}
static int native_read(void *p,unsigned n) {
 unsigned i;
 ++read_calls;
 if(read_error) return -7;
 if(n>actual_size-native_pos) n=actual_size-native_pos;
 for(i=0;i<n;++i) ((unsigned char *)p)[i]=(unsigned char)((native_pos+i)*17+3);
 native_pos+=n;return n;
}
static int fioRead(int fd,void *p,unsigned n) {
 (void)fd;
 if(block_mode==FIO_NOWAIT) {
  if(dispatch_error) return -8;
  completed_length=completion_error?-9:native_read(p,n);
  return 0;
 }
 completed_length=native_read(p,n);return completed_length;
}
#ifdef BDM
static int fileXioLseek(int fd,unsigned n,int whence) { return fioLseek(fd,n,whence); }
static int fileXioRead(int fd,void *p,unsigned n) { (void)fd;return native_read(p,n); }
#endif
#ifdef _WIN32
static unsigned SetFilePointer(int h,unsigned n,void *hi,int from) {
 (void)h;(void)hi;(void)from;
 if(seek_error) return ~0U;
 native_pos=n;return n;
}
static int GetLastError(void) { return 1; }
static int ReadFile(int h,void *p,unsigned n,void *read,OVERLAPPED *o) {
 int result;(void)h;
 if(dispatch_error && read_calls) return 0;
 native_pos=o->Offset;result=native_read(p,n);
 if(result<0) return 0;
 *(unsigned *)read=(unsigned)result;completed_length=result;return 1;
}
static int GetOverlappedResult(int h,OVERLAPPED *o,void *n,int wait) {
 (void)h;(void)o;(void)wait;++sync_calls;
 if(sync_error || completion_error) return 0;
 *(unsigned *)n=(unsigned)completed_length;return 1;
}
#endif
static int STIO_Stream(FileContext *,unsigned,unsigned);
""" + "\n".join(extract(n) for n in (
    "File_AllocStreamBuffers", "STIO_Read", "STIO_Seek", "STIO_Fill",
    "STIO_FillStm", "STIO_SeekStm", "STIO_Stream")) + r"""
static void init(FileContext *c,STIOFilePrivate *p,int xio) {
 memset(c,0,sizeof(*c));memset(p,0,sizeof(*p));
 block_mode=FIO_WAIT;seek_error=read_error=dispatch_error=completion_error=sync_error=0;
 read_calls=sync_calls=native_pos=0;actual_size=16384;
#ifndef _WIN32
 p->m_fXio=xio;p->m_RdChunk=4096;
#endif
 c->m_pData=p;c->m_Size=actual_size;
 c->Fill=STIO_Fill;c->Seek=STIO_Seek;c->Stream=STIO_Stream;
 assert(File_AllocStreamBuffers(c,4096,xio?1:2));
}
static void destroy(FileContext *c) { free(c->m_pBuff[0]);free(c->m_pBuff[1]); }
static void consume(FileContext *c) {
 c->m_CurPos+=(unsigned)(c->m_pEnd-c->m_pPos);c->m_pPos=c->m_pEnd;
 /* File_GetByte, used by parsers, advances m_CurPos but not m_Pos. */
}
int main(int argc,char **argv) {
 FileContext ctx;STIOFilePrivate priv;int which;
 assert(argc==2);which=atoi(argv[1]);
 init(&ctx,&priv,0);
 if(which==1) { /* Initial negative read must never create a negative span. */
  read_error=1;
  assert(STIO_Stream(&ctx,0,1)==0);
  assert(ctx.m_pPos==ctx.m_pEnd && ctx.m_pPos==ctx.m_pBuff[0]);
 } else if(which==2) { /* A failed async read is a failure, not a buffer. */
  completion_error=1;
  assert(STIO_Stream(&ctx,0,1)==4096);consume(&ctx);
  assert(ctx.Fill(&ctx)==0 && ctx.m_pPos==ctx.m_pEnd);
 } else if(which==3) { /* Sync failures must not be treated as completion. */
  assert(STIO_Stream(&ctx,0,1)==4096);consume(&ctx);sync_error=1;
  assert(ctx.Fill(&ctx)==0 && ctx.m_pPos==ctx.m_pEnd);
 } else if(which==4) { /* EOF must not replay the last cached completion. */
  actual_size=ctx.m_Size=8192;
  assert(STIO_Stream(&ctx,0,1)==4096);consume(&ctx);
  assert(ctx.Fill(&ctx)>0);consume(&ctx);
  assert(ctx.m_CurPos==ctx.m_Size);
  assert(ctx.Fill(&ctx)==0 && ctx.m_pPos==ctx.m_pEnd);
 } else if(which==5) { /* An unqueued prefetch must use ordinary refill. */
  unsigned char data[16];
  dispatch_error=1;
  assert(STIO_Stream(&ctx,0,1)==4096);
  assert(ctx.Fill==STIO_Fill && ctx.Seek==STIO_Seek && block_mode==FIO_WAIT);
  consume(&ctx);dispatch_error=0;
  assert(STIO_Read(&ctx,data,sizeof(data))==sizeof(data));
  for(unsigned i=0;i<sizeof(data);++i) assert(data[i]==(unsigned char)((4096+i)*17+3));
 } else if(which==6) { /* An early EOF preserves the count already copied. */
  unsigned char data[8]={0};actual_size=3;
  assert(STIO_Read(&ctx,data,sizeof(data))==3);
  for(unsigned i=0;i<3;++i) assert(data[i]==(unsigned char)(i*17+3));
 } else if(which==7) { /* No prefetch exists after a short initial read. */
  actual_size=1000;
  assert(STIO_Stream(&ctx,0,1)==1000);
  assert(ctx.Fill==STIO_Fill && ctx.Seek==STIO_Seek && block_mode==FIO_WAIT);
  consume(&ctx);assert(ctx.Fill(&ctx)==0);
 } else if(which==8) { /* Failed positioning cannot return unrelated bytes. */
  int xio;
  destroy(&ctx);
  for(xio=0;xio<=1;++xio) {
#ifndef BDM
   if(xio) continue;
#endif
   init(&ctx,&priv,xio);seek_error=1;
   assert(STIO_Stream(&ctx,100,1)==0 && read_calls==0);
   assert(ctx.m_pPos==ctx.m_pEnd);
   destroy(&ctx);
  }
  return 0;
 } else if(which==9) { /* Real async buffers preserve byte continuity. */
  unsigned char data[10240];actual_size=ctx.m_Size=sizeof(data);
  assert(STIO_Stream(&ctx,0,1)==4096);
  assert(STIO_Read(&ctx,data,sizeof(data))==sizeof(data));
  for(unsigned i=0;i<sizeof(data);++i) assert(data[i]==(unsigned char)(i*17+3));
  assert(ctx.Fill(&ctx)==0);
 } else if(which==10) { /* Streaming seek reports a rejected native seek. */
  assert(STIO_Stream(&ctx,0,1)==4096);seek_error=1;
  assert(ctx.Seek(&ctx,100)==-1);
 } else if(which==11) { /* Failed initial read must be safe for large callers. */
  unsigned char data[8192];read_error=1;
  STIO_Stream(&ctx,0,1);
  assert(STIO_Read(&ctx,data,sizeof(data))==0);
 } else if(which==12) { /* A later prefetch dispatch can fail too. */
  unsigned char data[16];
  assert(STIO_Stream(&ctx,0,1)==4096);consume(&ctx);dispatch_error=1;
  assert(ctx.Fill(&ctx)>0 && ctx.Fill==STIO_Fill && ctx.Seek==STIO_Seek);
  consume(&ctx);dispatch_error=0;
  assert(STIO_Read(&ctx,data,sizeof(data))==sizeof(data));
  for(unsigned i=0;i<sizeof(data);++i) assert(data[i]==(unsigned char)((8192+i)*17+3));
 } else if(which==13) { /* Failed reuse of the second buffer can recover. */
  unsigned char data[16];
  ctx.m_CurBuf=1;ctx.m_pPos=ctx.m_pBuff[1]+10;ctx.m_pEnd=ctx.m_pBuff[1]+100;
  read_error=1;assert(STIO_Stream(&ctx,100,1)==0);
  assert(ctx.m_CurBuf==0 && ctx.m_pPos==ctx.m_pBuff[0] && ctx.m_pEnd==ctx.m_pPos);
  read_error=0;assert(STIO_Stream(&ctx,100,0));
  assert(STIO_Read(&ctx,data,sizeof(data))==sizeof(data));
  for(unsigned i=0;i<sizeof(data);++i) assert(data[i]==(unsigned char)((100+i)*17+3));
 } else if(which==14) { /* An async completion error can recover by seeking. */
  unsigned char data[16];
  completion_error=1;assert(STIO_Stream(&ctx,0,1)==4096);consume(&ctx);
  assert(ctx.Fill(&ctx)==0);completion_error=0;
  assert(STIO_SeekStm(&ctx,100)==100);
  assert(STIO_Read(&ctx,data,sizeof(data))==sizeof(data));
  for(unsigned i=0;i<sizeof(data);++i) assert(data[i]==(unsigned char)((100+i)*17+3));
 } else assert(0);
 destroy(&ctx);return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "stio.c"
    src.write_text(harness)
    selected = os.environ.get("SMS_STIO_CASE")
    for variant, flags in (("legacy", []), ("bdm", ["-DBDM"]),
                           ("win32", ["-D_WIN32", "-Wno-unused-function"])):
        exe = Path(tmp) / ("stio-" + variant)
        subprocess.run([os.environ.get("CC", "cc"), "-g", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare", "-no-pie",
                        "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                        *flags, str(src), "-o", str(exe)], check=True)
        for case in ([int(selected)] if selected else range(1, 15)):
            subprocess.run([str(exe), str(case)], check=True, timeout=15)
print("STIO legacy/BDM/Win32: failed seeks/reads/prefetch, completion/EOF bounds, partial reads and byte continuity passed")
