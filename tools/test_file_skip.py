"""Compile actual STIO_Seek/File_Skip; inject seek failures and short reads."""
import os
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "src/SMS_FileContext.c").read_text()

def function(name, signature):
    start = source.index(signature)
    end = source.index("}  /* end " + name + " */", start) + len("}  /* end " + name + " */")
    return source[start:end]

harness = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define BDM 1
typedef struct { int m_fXio,m_FD; } STIOFilePrivate;
typedef struct FileContext FileContext;
struct FileContext {
 unsigned char *m_pBuff[2],*m_pPos,*m_pEnd;
 unsigned m_CurBuf,m_CurPos,m_Pos,m_Size;
 void *m_pData;
 int (*Seek)(FileContext *,unsigned);
 int (*Read)(FileContext *,void *,unsigned);
};
static int seek_fail, short_read;
static unsigned seeks, reads;
static unsigned char g_SMSounds[512];
static unsigned dev_size=1000000;
/* bdmfs_fatfs sign-extends the 32-bit offset; FatFs clamps a read-only seek to the file size. */
static int fileXioLseek(int fd,int n,int whence) {
 (void)fd;(void)whence;++seeks;return seek_fail?-1:n<0||(unsigned)n>dev_size?(int)dev_size:n;
}
static long long fileXioLseek64(int fd,long long n,int whence) {
 (void)fd;(void)whence;++seeks;return seek_fail?-1:n>dev_size?dev_size:n;
}
static int fioLseek(int fd,unsigned n,int whence) { (void)fd;(void)whence;++seeks;return seek_fail?-1:(int)n; }
static int read_file(FileContext *c,void *p,unsigned n) {
 (void)p;++reads;
 if(short_read) return 0;
 if(n>c->m_Size-c->m_CurPos) n=c->m_Size-c->m_CurPos;
 c->m_CurPos+=n;return n;
}
""" + function("STIO_Seek", "static int STIO_Seek (") + function("File_Skip", "void File_Skip (") + r"""
static FileContext file;
int main(void) {
 unsigned char buffer[1024];STIOFilePrivate priv={1,2};FileContext before;
 for(int xio=0;xio<=1;++xio) {
  memset(&file,0,sizeof(file));priv.m_fXio=xio;
  file.m_pBuff[0]=buffer;file.m_pPos=buffer+10;file.m_pEnd=buffer+100;
  file.m_CurPos=10;file.m_Pos=100;file.m_Size=1000000;
  file.m_pData=&priv;file.Seek=STIO_Seek;file.Read=read_file;
  seeks=reads=0;seek_fail=1;before=file;
  assert(STIO_Seek(&file,90000)==-1 && memcmp(&file,&before,sizeof(file))==0);
  File_Skip(&file,65536);
  assert(file.m_CurPos==65546 && reads==128);
  seek_fail=0;seeks=reads=0;
  File_Skip(&file,65536);
  assert(file.m_CurPos==131082 && seeks==1 && reads==0);
  File_Skip(&file,~0U);
  assert(file.m_CurPos==file.m_Size);
  file.m_CurPos=10;seek_fail=short_read=1;seeks=reads=0;
  File_Skip(&file,65536);
  assert(file.m_CurPos==10 && reads==1);
  short_read=0;seek_fail=0;seeks=reads=0;
  File_Skip(&file,513);
  assert(file.m_CurPos==523 && seeks==0 && reads==2);
  /* Above 2 GiB: the MPEG-PS probe seeks to m_Size-0x1FFFF for its tail scan. */
  dev_size=file.m_Size=0xC0000000U;
  assert(STIO_Seek(&file,0xBFFE0001U)!=-1 && file.m_CurPos==0xBFFE0001U);
  assert(STIO_Seek(&file,0x7FFFFFFFU)!=-1 && file.m_CurPos==0x7FFFFFFFU);
  dev_size=1000000;
 }
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / "skip.c", Path(tmp) / "skip"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
print("File skip: failed fio/fileXio seek preserves buffers, read fallback, overflow clamp, short reads and seeks above 2 GiB passed")
