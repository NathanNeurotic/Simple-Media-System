"""Compile the actual file-menu import copy; the source may already be the install target."""
import os
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "src/SMS_GUIFileCtxMenu.c").read_text()
start = source.index("static int _copy_file_to_mc (")
marker = "}  /* end _copy_file_to_mc */"
function = source[start:source.index(marker, start) + len(marker)]

harness = r"""
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
typedef long long s64;
static int lock_open, write_fail, opened, read_fd = -1;
static char read_path[64];
static int fioOpen(const char *path,int flags) {
 int fd;
 /* Optionally refuse to open for writing a file that is still open for reading. */
 if(lock_open && (flags&O_WRONLY) && read_fd>=0 && !strcmp(path,read_path)) return -EACCES;
 fd=open(path,flags,0644);
 if(fd<0) return -errno;
 if(!(flags&O_WRONLY)) { strcpy(read_path,path);read_fd=fd; }
 ++opened;return fd;
}
static int fioClose(int fd) { if(fd==read_fd) read_fd=-1;--opened;return close(fd); }
static int fioLseek(int fd,int offset,int whence) { return (int)lseek(fd,offset,whence); }
static int fioRead(int fd,void *p,int n) { return (int)read(fd,p,n); }
static int fioWrite(int fd,const void *p,int n) { return write_fail?-5:(int)write(fd,p,n); }
static int fioRemove(const char *path) { return unlink(path); }
static int fioRmdir(const char *path) { return rmdir(path); }
""" + function + r"""
static const char pack[]="0123456789 language pack";
static void put(const char *path,const char *text) {
 FILE *f=fopen(path,"wb");assert(f);fputs(text,f);fclose(f);
}
static int holds(const char *path,const char *text) {
 char buffer[128];size_t n;FILE *f=fopen(path,"rb");
 if(!f) return 0;
 n=fread(buffer,1,sizeof(buffer),f);fclose(f);
 return n==strlen(text) && !memcmp(buffer,text,n);
}
int main(void) {
 for(lock_open=0;lock_open<=1;++lock_open) {
  put("SMS.lng",pack);
  assert(_copy_file_to_mc("SMS.lng","SMS.lng")==1 && holds("SMS.lng",pack));
 }
 lock_open=0;
 assert(_copy_file_to_mc("SMS.lng","missing.lng")==0 && holds("SMS.lng",pack));
 put("empty.lng","");
 assert(_copy_file_to_mc("SMS.lng","empty.lng")==0 && holds("SMS.lng",pack));
 put("new.lng","fresh");
 assert(_copy_file_to_mc("SMS.lng","new.lng")==1 && holds("SMS.lng","fresh"));
 assert(_copy_file_to_mc("SMS.pal","new.lng")==1 && holds("SMS.pal","fresh"));
 write_fail=1;
 assert(_copy_file_to_mc("SMS.pal","new.lng")==0 && access("SMS.pal",F_OK)!=0);
 assert(opened==0);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / "import.c", Path(tmp) / "import"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-no-pie", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15, cwd=tmp)
print("File-menu import: self-import, unreadable or empty source, truncation and failed-write cleanup passed")
