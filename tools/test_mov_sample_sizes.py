"""Exercise the production stsz reader with production buffered reads under ASan/UBSan."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
mov = (args.source or root / "src/SMS_ContainerMOV.c").read_text()
io = (root / "src/SMS_FileContext.c").read_text()
header = (root / "include/SMS_FileContext.h").read_text()


def extract(source, signature, end_name):
    start = source.index(signature)
    end = source.index("}  /* end " + end_name + " */", start)
    return source[start:end + len("}  /* end " + end_name + " */")]


prefix = r"""
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define INLINE inline
typedef struct FileContext FileContext;
struct FileContext {
 unsigned m_Size,m_CurPos,m_Pos;
 unsigned char *m_pPos,*m_pEnd;
 int (*Read)(FileContext *,void *,unsigned);
 int (*Fill)(FileContext *);
};
typedef struct { void *m_pCtx; } SMS_Stream;
typedef struct { SMS_Stream *m_pStm[8]; unsigned m_nStm; } SMS_Container;
static unsigned char data[8192],buffer[257];
static unsigned available,loaded,window,read_calls,allocations,fail_alloc;
static int fill(FileContext *f) {
 unsigned n=available-loaded;
 if(n>window)n=window;
 memcpy(buffer,data+loaded,n);loaded+=n;
 f->m_pPos=buffer;f->m_pEnd=buffer+n;return n;
}
static unsigned SMS_unaligned32(const void *p) { unsigned n;memcpy(&n,p,4);return n; }
static unsigned SMS_bswap32(unsigned n) { return __builtin_bswap32(n); }
static unsigned _read_int_be(FileContext *f);
static void File_Skip(FileContext *f,unsigned n) {
 assert(n==4);(void)_read_int_be(f);
}
static void *allocate(size_t size) { ++allocations;return fail_alloc?NULL:malloc(size); }
#define malloc allocate
"""
types = mov[mov.index("typedef struct MOVIndex"):mov.index("static uint32_t _read_int_be")]
functions = extract(io, "static int STIO_Read (", "STIO_Read")
functions += extract(header, "static INLINE unsigned int File_GetUInt (", "File_GetInt")
functions += r"""
static unsigned _read_int_be(FileContext *f) { return SMS_bswap32(File_GetUInt(f)); }
static int read_payload(FileContext *f,void *p,unsigned n) {
 ++read_calls;return STIO_Read(f,p,n);
}
"""
functions += extract(mov, "static int _tbl_fits (", "_tbl_fits")
functions += extract(mov, "static int _mov_read_stsz (", "mov_read_stsz")
tests = r"""
static void put(unsigned offset,unsigned value) {
 data[offset]=value>>24;data[offset+1]=value>>16;
 data[offset+2]=value>>8;data[offset+3]=value;
}
static void run(unsigned count,unsigned fixed,unsigned bytes,unsigned chunk,unsigned failure) {
 memset(data,0,sizeof(data));put(4,fixed);put(8,count);
 for(unsigned i=0;i<count;++i)put(12+4*i,0x80000000U+i*0x1234567U);
 available=bytes;loaded=read_calls=allocations=0;window=chunk;fail_alloc=failure;
 FileContext f={12+4*count,0,0,buffer,buffer,read_payload,fill};
 MOVStream stream={0};SMS_Stream s={&stream};SMS_Container c={{&s},1};MOVContext ctx={0};
 ctx.m_pBase=&c;MOVAtom atom={0,0,12+4*count};
 assert(_mov_read_stsz(&ctx,&f,&atom)==0);
 if(fixed) {
  assert(stream.m_nSamples==count && stream.m_SampleSize==fixed);
  assert(!stream.m_pSampleSize && !allocations && f.m_CurPos==12);
 } else if(failure || bytes<12+4*count) {
  assert(!stream.m_nSamples);
 } else {
  assert(stream.m_nSamples==count && f.m_CurPos==12+4*count);
  for(unsigned i=0;i<count;++i)assert((unsigned)stream.m_pSampleSize[i]==0x80000000U+i*0x1234567U);
 }
 free(stream.m_pSampleSize);
}
int main(void) {
 /* Every payload truncation, including within a word, with tiny refill windows. */
 for(unsigned chunk=1;chunk<=17;++chunk) {
  for(unsigned n=1;n<=33;++n) {
   run(n,0,12+4*n,chunk,0);
   for(unsigned bytes=12;bytes<12+4*n;++bytes)run(n,0,bytes,chunk,0);
  }
  run(0,0,12,chunk,0);run(100,8,12,chunk,0);
  run(33,0,144,chunk,1);
 }
 run(1024,0,4108,257,0);
 /* Declared atom shorter than its table must not allocate. */
 put(4,0);put(8,5);available=12;loaded=allocations=0;window=4;fail_alloc=0;
 FileContext f={12,0,0,buffer,buffer,read_payload,fill};
 MOVStream stream={0};SMS_Stream s={&stream};SMS_Container c={{&s},1};MOVContext ctx={0};
 ctx.m_pBase=&c;MOVAtom atom={0,0,12};
 assert(!_mov_read_stsz(&ctx,&f,&atom) && !allocations && !stream.m_nSamples);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as temporary:
    src, exe = Path(temporary) / "stsz.c", Path(temporary) / "stsz"
    src.write_text(prefix + types + functions + tests)
    subprocess.run([os.environ.get("CC", "cc"), "-O2", "-g", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=30)
print("MOV sample sizes: byte order, buffered boundaries, truncation, fixed sizes and allocation failure passed")
