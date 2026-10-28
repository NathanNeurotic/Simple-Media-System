"""Actual STIO open/refill/destruction with controlled fileXio/fio drivers.

Checks driver-confirmed USB sizing, conservative fallback and byte continuity.
RPC/driver timing is simulated; this is not a console throughput measurement.
"""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--case", choices=("all", "usb", "fallback", "allocation"), default="all")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
source = (root / "src/SMS_FileContext.c").read_text()
header = (root / "include/SMS_FileContext.h").read_text()


def function(name):
    marker = "}  /* end " + name + " */"
    end = source.index(marker) + len(marker)
    start = source.rfind("\nstatic ", 0, source.index(marker)) + 1
    if name == "STIO_InitFileContext":
        start = source.index("FileContext* STIO_InitFileContext (")
    return source[start:end]


context = header[header.index("typedef struct FileContext {"):header.index("} FileContext;") + len("} FileContext;")]
start = source.index("typedef struct STIOFilePrivate {", source.index("} STIOFilePrivate;") + 1)
private = source[start:source.index("#endif  /* _WIN32 */", start)]
constants = "\n".join(re.findall(r"^#define (?:MX4SIO_RD_CHUNK|UDPFS_RD_CHUNK|USB_RD_CHUNK|BDM_IOCTL_GET_DRIVERNAME|BDM_DRIVER_USB)\b[^\n]*", source, re.M))
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <stdio.h>
#define FIO_WAIT 0
''' + context + private + constants + r'''
static unsigned allocations,fail_at,opens,closes,queries,reads,total,remaining,limit;
static int driver,open_failure;
static const char *path;
static unsigned int g_Mx4sioMask;
static void *allocate(size_t n) { return ++allocations==fail_at?NULL:malloc(n); }
static void *aligned(size_t a,size_t n) { return ++allocations==fail_at?NULL:memalign(a,n); }
#define malloc allocate
#define memalign aligned
static int fileXioOpen(const char *name,int flags) {
 assert(!strcmp(name,path) && flags==O_RDONLY);++opens;return open_failure?-1:42;
}
static int fioOpen(const char *name,int flags) {
 assert(!strcmp(name,path) && flags==O_RDONLY);++opens;return open_failure?-1:86;
}
static int fileXioIoctl2(int fd,int cmd,void *in,unsigned n,void *out,unsigned m) {
 assert(fd==42 && cmd==3 && !in && !n && !out && !m);++queries;return driver;
}
static int fileXioLseek(int fd,int offset,int whence) { assert(fd==42 && !offset);return whence==SEEK_END?196608:0; }
static int fioLseek(int fd,int offset,int whence) { assert(fd==86 && !offset);return whence==SEEK_END?196608:0; }
static int fileXioClose(int fd) { assert(fd==42);++closes;return 0; }
static int fioClose(int fd) { assert(fd==86);++closes;return 0; }
static void fioSetBlockMode(int mode) { assert(mode==FIO_WAIT); }
static int device_read(int fd,void *buffer,unsigned n) {
 assert((fd==42 || fd==86) && n<=limit);++reads;
 if(n>remaining)n=remaining;
 for(unsigned i=0;i<n;++i)((unsigned char*)buffer)[i]=(unsigned char)(total+i);
 total+=n;remaining-=n;return n;
}
static int fileXioRead(int fd,void *p,unsigned n) { assert(fd==42);return device_read(fd,p,n); }
static int fioRead(int fd,void *p,unsigned n) { assert(fd==86);return device_read(fd,p,n); }
static int STIO_Read(FileContext *f,void *p,unsigned n) { (void)f;(void)p;(void)n;return 0; }
static int STIO_Seek(FileContext *f,unsigned p) { (void)f;(void)p;return 0; }
static int STIO_Stream(FileContext *f,unsigned p,unsigned n) { (void)f;(void)p;(void)n;return 0; }
'''
main = r'''
static FileContext *open_file(const char *name,int kind) {
 path=name;driver=kind;allocations=fail_at=opens=closes=queries=reads=total=0;open_failure=0;
 return STIO_InitFileContext(name,NULL);
}
static void check(const char *name,int kind,unsigned expected,unsigned query_count,unsigned refill_limit) {
 FileContext *f=open_file(name,kind);assert(f && opens==1);
#ifdef BDM
 STIOFilePrivate *p=f->m_pData;assert(p->m_RdChunk==expected && p->m_FillLimit==(int)refill_limit);
#endif
 assert(queries==query_count);
 /* A full player-sized buffer takes three 64 KiB USB RPCs, or six
  * bounded refills of eight 4 KiB RPCs on conservative devices. */
 free(f->m_pBuff[0]);f->m_BufSize=f->m_CurBufSize=196608;
 f->m_pBuff[0]=aligned(64,f->m_BufSize);assert(f->m_pBuff[0]);
 remaining=196608;limit=expected;unsigned fills=0;
 while(remaining) {
  unsigned before=total;int n=f->Fill(f);assert(n>0);++fills;
  if(expected==4096)assert(n<=32768);
  for(unsigned i=0;i<(unsigned)n;++i)assert(f->m_pBuff[0][i]==(unsigned char)(before+i));
 }
 assert(total==196608 && f->Fill(f)==0);
 assert(queries==query_count); /* Never query the driver during a refill. */
 if(expected==65536)assert(fills==(refill_limit?3:1) && reads==4);
 if(expected==4096)assert(fills==6 && reads==49);
 f->Destroy(f);assert(closes==1);
}
int main(int argc,char **argv) {
 assert(argc==2);
#ifdef BDM
 if(!strcmp(argv[1],"all") || !strcmp(argv[1],"usb")) {
  g_Mx4sioMask=~0U;check("mass:/movie.mpg",0x00627375,65536,1,65536);
  g_Mx4sioMask=0;check("mass3:/movie.mpg",0x00627375,65536,1,65536);
 }
 if(!strcmp(argv[1],"all") || !strcmp(argv[1],"fallback")) {
  const int kinds[]={0x0063786d,0x00617461,0x006e6c69,0,1,-5};
  for(unsigned i=0;i<sizeof(kinds)/sizeof(kinds[0]);++i)check("mass1:/movie.mpg",kinds[i],4096,1,32768);
  check("mmce0:/movie.mpg",0x00627375,4096,0,32768);
  check("smb:/movie.mpg",0x00627375,4096,0,32768);
  check("udpfs:/movie.mpg",-5,65536,0,0);
 }
#else
 check("host:movie.mpg",0,196608,0,0);
#endif
 if(!strcmp(argv[1],"all") || !strcmp(argv[1],"allocation")) {
  for(unsigned fail=1;fail<=4;++fail) {
   path="mass0:/movie.mpg";driver=0x00627375;opens=closes=queries=allocations=0;fail_at=fail;
   FileContext *f=STIO_InitFileContext(path,NULL);assert(!f && opens==1 && closes==1);
  }
  path="mass0:/movie.mpg";opens=closes=queries=0;open_failure=1;fail_at=allocations=0;
  assert(!STIO_InitFileContext(path,NULL) && opens==1 && !closes && !queries && !allocations);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    fixture = tmp / "stio_devices.c"
    fixture.write_text(prefix + function("STIO_Fill") + function("STIO_DestroyFileContext") + function("STIO_InitFileContext") + main)
    for bdm in (1, 0):
        exe = tmp / f"stio-devices-{bdm}"
        cmd = [os.environ.get("CC", "cc"), "-std=c99", "-g", "-no-pie",
               "-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
        if bdm:
            cmd += ["-DBDM=1"]
        subprocess.run(cmd + [str(fixture), "-o", str(exe)], check=True)
        subprocess.run([str(exe), args.case], check=True, timeout=15)
print("STIO devices: driver-confirmed USB, conservative fallback, legacy route, continuity and open allocation failures passed")
