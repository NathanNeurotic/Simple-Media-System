"""Actual SMS_CopyFile and STIO code against a model of ps2sdk fio.

ps2sdk fio has one block mode and one result word for every call. fioSync
stores a result only in FIO_NOWAIT mode; in FIO_WAIT mode it returns an error
and leaves its argument alone. STIO selects FIO_NOWAIT only while a legacy-fio
source larger than one stream buffer reads ahead, so the destination write then
replaces the read-ahead's byte count. The model follows the pinned SDK's
fioSync and fioWrite; it is NOT a console result.

Each copy must return 0 and leave a destination identical to its source.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/SMS_FileContext.c").read_text()
copy_source = (root / "src/SMS_CopyTree.c").read_text()
header = (root / "include/SMS_FileContext.h").read_text()
shim = (root / "tools/ps2_kernel_shim.h").read_text()

context = header[header.index("typedef struct FileContext {"):header.index("} FileContext;") + len("} FileContext;")]
constants = "\n".join(re.findall(r"^#define (?:MX4SIO_RD_CHUNK|UDPFS_RD_CHUNK|USB_RD_CHUNK|BDM_IOCTL_GET_DRIVERNAME|BDM_DRIVER_USB)\b[^\n]*", source, re.M))
buf_size = re.search(r"^#define BUF_SIZE[^\n]*", copy_source, re.M).group(0)
start = source.index("static int File_AllocStreamBuffers (")
alloc = source[start:source.index("\nstatic int CDDA_Stream", start)]
start = source.index("#ifdef _WIN32\ntypedef struct STIOFilePrivate {")
end = source.index("}  /* end STIO_InitFileContext */") + len("}  /* end STIO_InitFileContext */")
stio = source[start:end]
start = copy_source.index("int SMS_CopyFile (\n")
end = copy_source.index("}  /* end SMS_CopyFile */", start) + len("}  /* end SMS_CopyFile */")
copy = copy_source[start:end]

prefix = shim + r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <unistd.h>
#define FIO_WAIT 0
#define FIO_NOWAIT 1
#define FIO_COMPLETE 1
typedef uint64_t u64;
#define STIO_PF_LOAD( x )     __atomic_load_n ( &( x ), __ATOMIC_SEQ_CST )
#define STIO_PF_STORE( x, v ) __atomic_store_n ( &( x ), ( v ), __ATOMIC_SEQ_CST )
static volatile unsigned g_Timer = 1;
''' + context + "\n" + constants + "\n" + buf_size + r'''
static unsigned char pattern ( unsigned o ) { return ( unsigned char )( ( o ^ ( o >> 8 ) ^ ( o >> 17 ) ) * 37u + 11u ); }
/* One source file, opened as cdfs: ( fio ) or mass: ( fileXio ). */
static unsigned src_size, src_pos, src_usb;
static pthread_mutex_t src_lock = PTHREAD_MUTEX_INITIALIZER;
static int src_read ( void* p, unsigned n ) {
 unsigned i;
 pthread_mutex_lock ( &src_lock );
 if ( n > src_size - src_pos ) n = src_size - src_pos;
 for ( i = 0; i < n; ++i ) ( ( unsigned char* )p )[ i ] = pattern ( src_pos + i );
 src_pos += n;
 pthread_mutex_unlock ( &src_lock );
 return ( int )n;
}
static int src_seek ( int off, int whence ) {
 pthread_mutex_lock ( &src_lock );
 src_pos = whence == SEEK_END ? src_size : ( unsigned )off > src_size ? src_size : ( unsigned )off;
 off = ( int )src_pos;
 pthread_mutex_unlock ( &src_lock );
 return off;
}
static int fileXioOpen ( const char* n, int f ) { ( void )n; ( void )f; src_pos = 0; return 40; }
static int fileXioClose ( int fd ) { assert ( fd == 40 ); return 0; }
static int fileXioIoctl2 ( int fd, int c, void* a, unsigned b, void* d, unsigned e ) {
 ( void )c; ( void )a; ( void )b; ( void )d; ( void )e; assert ( fd == 40 ); return src_usb ? BDM_DRIVER_USB : 0x0063786d;
}
static int fileXioLseek ( int fd, int off, int whence ) { assert ( fd == 40 ); return src_seek ( off, whence ); }
static long long fileXioLseek64 ( int fd, long long off, int whence ) { return fileXioLseek ( fd, ( int )off, whence ); }
static int fileXioRead ( int fd, void* p, unsigned n ) { assert ( fd == 40 ); return src_read ( p, n ); }
/* fio: a call queued in FIO_NOWAIT mode returns 0 and leaves its result in fio_recv. */
static int fio_mode, fio_recv;
static unsigned char* dst;
static unsigned dst_len, dst_cap;
static int fioOpen ( const char* n, int f ) {
 assert ( fio_mode == FIO_WAIT );
 if ( !strncmp ( n, "pfs", 3 ) ) { assert ( f & O_TRUNC ); dst_len = 0; return fio_recv = 20; }
 src_pos = 0; return fio_recv = 10;
}
static int fioClose ( int fd ) { assert ( fd == 10 || fd == 20 ); return fio_recv = 0; }
static int fioLseek ( int fd, int off, int whence ) { assert ( fd == 10 ); return fio_recv = src_seek ( off, whence ); }
static int fioRead ( int fd, void* p, unsigned n ) {
 assert ( fd == 10 ); fio_recv = src_read ( p, n ); return fio_mode == FIO_NOWAIT ? 0 : fio_recv;
}
static int fioWrite ( int fd, const void* p, int n ) {
 assert ( fd == 20 && n >= 0 );
 if ( dst_len + n > dst_cap ) { dst_cap = ( dst_len + n ) * 2; dst = realloc ( dst, dst_cap ); assert ( dst ); }
 memcpy ( dst + dst_len, p, n ); dst_len += n; ++g_Timer;
 fio_recv = n; return fio_mode == FIO_NOWAIT ? 0 : n;
}
static int fioSync ( int m, int* r ) {
 assert ( m == FIO_WAIT );
 if ( fio_mode != FIO_NOWAIT ) return -1;
 if ( r ) *r = fio_recv;
 return FIO_COMPLETE;
}
static void fioSetBlockMode ( int m ) { fio_mode = m; }
typedef struct { char* m_pStr; } TStr;
static TStr STR_COPYING = { "copying" }, STR_KBS = { "KB/s" }, STR_STOPPING = { "stop" };
#define SMS_PAD_TRIANGLE 0x1000
#define RC_STOP 0x2000
static void GUI_Progress ( const char* s, int p, int f ) { ( void )s; ( void )p; ( void )f; }
static void GUI_Status ( const char* s ) { ( void )s; }
static int  GUI_ReadButtons ( void ) { return 0; }
''' + alloc + "\n" + stio + "\n" + copy + r'''
int main ( void ) {
 static const unsigned sizes[] = { 1000, 393216, 393217, 500000, 786432, 1000000, 3000001 };
 unsigned d, k, i;
 for ( d = 0; d < 3; ++d ) for ( k = 0; k < sizeof sizes / sizeof *sizes; ++k ) {
  FileContext* f;
  src_size = sizes[ k ]; src_usb = d == 1;   /* 0: cdfs, 1: USB read-ahead, 2: other mass driver */
  f = STIO_InitFileContext ( d ? "mass:/x.avi" : "cdfs:/x.avi", NULL );
  assert ( f && f -> m_Size == src_size );
  i = SMS_CopyFile ( "pfs0:/x.avi", f, 0, NULL, NULL );
  if ( i != 0 || dst_len != src_size ) {
   printf ( "device %u, %u bytes: returned %u, destination holds %u\n", d, src_size, i, dst_len );
   return 1;
  }
  for ( i = 0; i < src_size; ++i ) assert ( dst[ i ] == pattern ( i ) );
  assert ( fio_mode == FIO_WAIT );
 }
 free ( dst );
 return 0;
}
'''

with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / "copy.c", Path(tmp) / "copy"
    src.write_text(prefix)
    # Pattern-initialised locals make a result word that is never stored show up as a mismatch.
    subprocess.run([os.environ.get("CC", "cc"), "-std=gnu99", "-g", "-O1", "-Wall", "-Wno-sign-compare",
                    "-Wno-unused-function", "-Wno-unused-variable", "-Wno-unused-but-set-variable",
                    "-Wno-misleading-indentation", "-Wno-pointer-to-int-cast", "-no-pie", "-pthread", "-DBDM=1",
                    "-ftrivial-auto-var-init=pattern", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=60)
print("Copy to HDD: fio and fileXio sources of every size copy byte-exact with ps2sdk fioSync semantics")
