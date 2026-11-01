"""Actual STIO open/stream/read/seek/destroy code with confirmed-USB read-ahead (#24).

The production read-ahead block runs on a real host thread (pthreads and POSIX
semaphores via tools/ps2_kernel_shim.h); fileXio is a controlled fake that
sleeps per read, records which bytes are being written, and asserts that no read
is in flight when the descriptor is repositioned or closed. Builds run under
AddressSanitizer/UBSan and, separately, ThreadSanitizer.

What this establishes: byte continuity, EOF and error handling, seek/restart,
teardown with reads in flight or slots full, fallback when setup fails, and that
the next read is issued while the reader still holds the previous slot.
What it does not: EE priority scheduling, real USB/IOP timing, or console
playback. NOT a hardware result.
"""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/SMS_FileContext.c").read_text()
header = (root / "include/SMS_FileContext.h").read_text()
shim = (root / "tools/ps2_kernel_shim.h").read_text()

context = header[header.index("typedef struct FileContext {"):header.index("} FileContext;") + len("} FileContext;")]
constants = "\n".join(re.findall(r"^#define (?:MX4SIO_RD_CHUNK|UDPFS_RD_CHUNK|USB_RD_CHUNK|BDM_IOCTL_GET_DRIVERNAME|BDM_DRIVER_USB)\b[^\n]*", source, re.M))
start = source.index("static int File_AllocStreamBuffers (")
alloc = source[start:source.index("\nstatic int CDDA_Stream", start)]
start = source.index("#ifdef _WIN32\ntypedef struct STIOFilePrivate {")
end = source.index("}  /* end STIO_InitFileContext */") + len("}  /* end STIO_InitFileContext */")
stio = source[start:end]

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
static volatile u64 g_Timer;
''' + context + "\n" + constants + r'''
/* Two descriptors with independent cursors. fxio_lock stands in for the fileXio
 * client lock, which serializes every call; dev_lock guards the bookkeeping. */
static pthread_mutex_t fxio_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t dev_lock = PTHREAD_MUTEX_INITIALIZER;
static unsigned dev_size, dev_pos[ 2 ], dev_reads, dev_delay_us, dev_inflight[ 2 ], dev_closes, dev_opens;
static unsigned dev_fail_at = ~0U;         /* first offset whose read fails */
static unsigned char *dev_dst_lo, *dev_dst_hi;
static int dev_driver = 0x00627375;        /* "usb" */
static unsigned char pattern ( unsigned o ) { return ( unsigned char )( ( o ^ ( o >> 8 ) ^ ( o >> 17 ) ) * 37u + 11u ); }
static int slot ( int fd ) { assert ( fd == 42 || fd == 43 ); return fd - 42; }
static int fileXioOpen ( const char *name, int flags ) {
 int fd = 42 + ( int )( dev_opens++ & 1 );
 ( void )name; assert ( flags == O_RDONLY ); dev_pos[ slot ( fd ) ] = 0; return fd;
}
static int fioOpen ( const char *name, int flags ) { ( void )name; ( void )flags; return -1; }
static int fileXioIoctl2 ( int fd, int cmd, void *a, unsigned b, void *c, unsigned d ) {
 ( void )a; ( void )b; ( void )c; ( void )d; slot ( fd ); assert ( cmd == BDM_IOCTL_GET_DRIVERNAME ); return dev_driver;
}
static int fileXioLseek ( int fd, int off, int whence ) {
 int r, k = slot ( fd );
 pthread_mutex_lock ( &dev_lock );       /* checked before waiting for the client lock */
 assert ( !dev_inflight[ k ] );          /* never reposition under a read */
 pthread_mutex_unlock ( &dev_lock );
 pthread_mutex_lock ( &fxio_lock ); pthread_mutex_lock ( &dev_lock );
 if ( whence == SEEK_END ) r = ( int )dev_size;
 else { dev_pos[ k ] = ( unsigned )off; r = off; }
 pthread_mutex_unlock ( &dev_lock ); pthread_mutex_unlock ( &fxio_lock );
 return r;
}
static int fileXioClose ( int fd ) {
 int k = slot ( fd );
 pthread_mutex_lock ( &dev_lock ); assert ( !dev_inflight[ k ] ); ++dev_closes; pthread_mutex_unlock ( &dev_lock );
 return 0;
}
static int fileXioRead ( int fd, void *p, unsigned n ) {
 unsigned i, pos;
 int k = slot ( fd );
 assert ( n <= USB_RD_CHUNK );
 pthread_mutex_lock ( &fxio_lock );
 pthread_mutex_lock ( &dev_lock );
 assert ( !dev_inflight[ k ] ); ++dev_inflight[ k ]; ++dev_reads;
 pos = dev_pos[ k ];
 if ( !k ) { dev_dst_lo = ( unsigned char* )p; dev_dst_hi = ( unsigned char* )p + n; }
 pthread_mutex_unlock ( &dev_lock );
 { unsigned d = __atomic_load_n ( &dev_delay_us, __ATOMIC_SEQ_CST ); if ( d ) usleep ( d ); }
 if ( pos >= dev_fail_at ) n = 0;
 else {
  if ( n > dev_size - pos ) n = dev_size - pos;
  if ( pos + n > dev_fail_at ) n = dev_fail_at - pos;
  for ( i = 0; i < n; ++i ) ( ( unsigned char* )p )[ i ] = pattern ( pos + i );
 }
 pthread_mutex_lock ( &dev_lock );
 dev_pos[ k ] = pos + n; --dev_inflight[ k ];
 if ( !k ) dev_dst_lo = dev_dst_hi = NULL;
 pthread_mutex_unlock ( &dev_lock );
 pthread_mutex_unlock ( &fxio_lock );
 return pos >= dev_fail_at ? -5 : ( int )n;
}
/* The legacy fio path is compiled but not taken by mass: files. */
static int fioRead ( int fd, void *p, unsigned n ) { ( void )fd; ( void )p; ( void )n; assert ( 0 ); return -1; }
static int fioLseek ( int fd, int o, int w ) { ( void )fd; ( void )o; ( void )w; assert ( 0 ); return -1; }
static int fioClose ( int fd ) { ( void )fd; assert ( 0 ); return -1; }
static int fioSync ( int m, int *n ) { ( void )m; ( void )n; assert ( 0 ); return -1; }
static void fioSetBlockMode ( int m ) { ( void )m; }
''' + alloc + "\n" + stio + r'''
static FileContext *open_file ( unsigned size, int driver ) {
 FileContext *f;
 dev_size = size; dev_driver = driver; dev_fail_at = ~0U; dev_reads = 0; dev_opens = 0;
 f = STIO_InitFileContext ( "mass0:/movie.mpg", NULL );
 assert ( f );
 return f;
}
static void expect_bytes ( FileContext *f, unsigned from, unsigned count, unsigned chunk ) {
 static unsigned char buf[ 300000 ];
 unsigned done = 0, i;
 assert ( chunk <= sizeof ( buf ) );
 while ( done < count ) {
  unsigned want = count - done < chunk ? count - done : chunk;
  unsigned got = ( unsigned )f -> Read ( f, buf, want );
  assert ( got == want );
  for ( i = 0; i < got; ++i ) assert ( buf[ i ] == pattern ( from + done + i ) );
  done += got;
  /* The slot being written must never be the bytes the reader holds. */
  pthread_mutex_lock ( &dev_lock );
  assert ( !dev_dst_lo || dev_dst_hi <= f -> m_pPos || dev_dst_lo >= f -> m_pEnd );
  pthread_mutex_unlock ( &dev_lock );
  chunk = chunk * 7 % 65521 + 1;   /* vary the request size, including odd sizes */
 }
}
static void expect_eof ( FileContext *f ) {
 unsigned char b;
 assert ( f -> Read ( f, &b, 1 ) == 0 );
 assert ( f -> Fill ( f ) == 0 );
}
static unsigned reads_now ( void ) {
 unsigned n;
 pthread_mutex_lock ( &dev_lock ); n = dev_reads; pthread_mutex_unlock ( &dev_lock );
 return n;
}
static unsigned inflight_now ( void ) {
 unsigned n;
 pthread_mutex_lock ( &dev_lock ); n = dev_inflight[ 0 ]; pthread_mutex_unlock ( &dev_lock );
 return n;
}
static int prefetching ( FileContext *f ) { return f -> Fill == STIO_FillPf; }

int main ( int argc, char **argv ) {
 FileContext *f, *g;
 int which;
 assert ( argc == 2 ); which = atoi ( argv[ 1 ] );
 switch ( which ) {
 case 1: /* Whole-file continuity across many slot wraps, odd file size. */
  f = open_file ( 1000003, 0x00627375 );
  assert ( f -> Stream ( f, 0, f -> m_StreamSize >> 3 ) == USB_RD_CHUNK );
  assert ( prefetching ( f ) && s_PF.m_nSlots == 3 );
  expect_bytes ( f, 0, 1000003, 1 ); expect_eof ( f );
  f -> Destroy ( f ); assert ( dev_closes == 1 && !s_PF.m_pOwner );
  break;
 case 2: /* The next read is issued while the reader still holds a slot. */
  f = open_file ( 4 * USB_RD_CHUNK, 0x00627375 ); dev_delay_us = 2000;
  assert ( f -> Stream ( f, 0, 48 ) == USB_RD_CHUNK );
  { int tries = 0; while ( reads_now () < 3 && tries++ < 5000 ) usleep ( 200 ); }
  assert ( reads_now () >= 3 );   /* slots 1 and 2 were read with no Fill call */
  expect_bytes ( f, 0, 4 * USB_RD_CHUNK, 4096 ); expect_eof ( f );
  f -> Destroy ( f );
  break;
 case 3: /* Seek mid-stream, backwards and forwards, then to EOF. */
  f = open_file ( 700000, 0x00627375 );
  assert ( f -> Stream ( f, 0, 48 ) > 0 );
  expect_bytes ( f, 0, 100000, 5000 );
  assert ( f -> Seek ( f, 12345 ) == 12345 && prefetching ( f ) );
  expect_bytes ( f, 12345, 200000, 777 );
  assert ( f -> Seek ( f, 600001 ) == 600001 );
  expect_bytes ( f, 600001, 99999, 65536 ); expect_eof ( f );
  assert ( f -> Seek ( f, 700000 ) == 700000 ); expect_eof ( f );
  f -> Destroy ( f );
  break;
 case 4: /* Player pattern: stop, a short pause-time stream, restart. */
  f = open_file ( 500000, 0x00627375 );
  assert ( f -> Stream ( f, 0, 48 ) > 0 ); expect_bytes ( f, 0, 70000, 3000 );
  assert ( f -> Stream ( f, f -> m_CurPos, 0 ) && !prefetching ( f ) && !s_PF.m_pOwner );
  assert ( f -> Stream ( f, f -> m_CurPos, 10 ) > 0 && !prefetching ( f ) );   /* 40 KiB: synchronous */
  expect_bytes ( f, 70000, 50000, 999 );
  assert ( f -> Stream ( f, f -> m_CurPos, 0 ) );
  assert ( f -> Stream ( f, f -> m_CurPos, 48 ) > 0 && prefetching ( f ) );
  expect_bytes ( f, 120000, 380000, 4096 ); expect_eof ( f );
  f -> Destroy ( f );
  break;
 case 5: /* A device error mid-file ends the stream; a seek recovers. */
  f = open_file ( 600000, 0x00627375 );
  dev_fail_at = 200000;
  assert ( f -> Stream ( f, 0, 48 ) > 0 );
  expect_bytes ( f, 0, 200000, 4096 ); expect_eof ( f );
  dev_fail_at = ~0U;
  assert ( f -> Seek ( f, 150000 ) == 150000 );
  expect_bytes ( f, 150000, 450000, 8192 ); expect_eof ( f );
  f -> Destroy ( f );
  break;
 case 6: /* Only one owner; a second USB file uses ordinary synchronous refills. */
  f = open_file ( 300000, 0x00627375 );
  assert ( f -> Stream ( f, 0, 48 ) > 0 && prefetching ( f ) );
  expect_bytes ( f, 0, 100000, 6000 );
  g = STIO_InitFileContext ( "mass0:/other.mpg", NULL ); assert ( g );
  assert ( g -> Stream ( g, 0, 48 ) > 0 && !prefetching ( g ) );
  expect_bytes ( g, 0, 300000, 4096 ); expect_eof ( g );
  g -> Destroy ( g );   /* the owner is untouched */
  assert ( s_PF.m_pOwner == f );
  expect_bytes ( f, 100000, 200000, 6000 ); expect_eof ( f );
  f -> Destroy ( f );
  break;
 case 7: /* Non-USB mass devices ( MX4SIO etc. ) never read ahead. */
  f = open_file ( 300000, 0x0063786d );
  assert ( f -> Stream ( f, 0, 48 ) > 0 && !prefetching ( f ) && !shim_threads_started );
  expect_bytes ( f, 0, 300000, 4096 ); expect_eof ( f );
  f -> Destroy ( f );
  break;
 case 8: /* Setup failures fall back to synchronous refills, then later succeed. */
  f = open_file ( 300000, 0x00627375 );
  shim_fail_thread = 1;
  assert ( f -> Stream ( f, 0, 48 ) > 0 && !prefetching ( f ) );
  expect_bytes ( f, 0, 100000, 4096 );
  assert ( f -> Stream ( f, f -> m_CurPos, 0 ) );
  shim_fail_sema = shim_sema_calls + 1;   /* the per-session free-slot semaphore */
  assert ( f -> Stream ( f, f -> m_CurPos, 48 ) > 0 && !prefetching ( f ) );
  assert ( shim_sems_in_use () == 2 );    /* only the persistent pair remains */
  expect_bytes ( f, 100000, 50000, 4096 );
  assert ( f -> Stream ( f, f -> m_CurPos, 0 ) );
  assert ( f -> Stream ( f, f -> m_CurPos, 48 ) > 0 && prefetching ( f ) );
  expect_bytes ( f, 150000, 150000, 4096 ); expect_eof ( f );
  f -> Destroy ( f ); assert ( shim_sems_in_use () == 2 );
  break;
 case 9: /* Teardown with every slot full, and with a read in flight. */
  f = open_file ( 2000000, 0x00627375 );
  assert ( f -> Stream ( f, 0, 48 ) > 0 );
  { int tries = 0; while ( reads_now () < ( unsigned )s_PF.m_nSlots && tries++ < 5000 ) usleep ( 200 ); }
  assert ( reads_now () >= ( unsigned )s_PF.m_nSlots );   /* every slot is full */
  f -> Destroy ( f ); assert ( !s_PF.m_pOwner && dev_closes == 1 );
  f = open_file ( 2000000, 0x00627375 ); dev_delay_us = 20000;
  assert ( f -> Stream ( f, 0, 48 ) > 0 );
  { int tries = 0; while ( !inflight_now () && tries++ < 5000 ) usleep ( 200 ); }
  assert ( inflight_now () );             /* helper is inside a slow read */
  f -> Destroy ( f ); assert ( !s_PF.m_pOwner && dev_closes == 2 );
  assert ( shim_threads_started == 1 && shim_sems_in_use () == 2 );   /* one persistent helper */
  break;
 case 10: /* EOF exactly at a slot boundary, and a file shorter than one slot. */
  f = open_file ( 3 * USB_RD_CHUNK, 0x00627375 );
  assert ( f -> Stream ( f, 0, 48 ) == USB_RD_CHUNK );
  expect_bytes ( f, 0, 3 * USB_RD_CHUNK, 65536 ); expect_eof ( f );
  f -> Destroy ( f );
  f = open_file ( 1000, 0x00627375 );
  assert ( f -> Stream ( f, 0, 48 ) == 1000 );
  expect_bytes ( f, 0, 1000, 1000 ); expect_eof ( f );
  f -> Destroy ( f );
  break;
 case 11: /* Starting at or past EOF reports failure without a pending read. */
  f = open_file ( 100000, 0x00627375 );
  assert ( f -> Stream ( f, 100000, 48 ) == 0 && !prefetching ( f ) );
  assert ( f -> Stream ( f, 0, 0 ) );
  expect_bytes ( f, 0, 100000, 4096 ); expect_eof ( f );
  f -> Destroy ( f );
  break;
 case 12: /* Restarting without a stop: same-size reuse, then a reallocation. */
  f = open_file ( 900000, 0x00627375 ); dev_delay_us = 3000;
  assert ( f -> Stream ( f, 0, 48 ) > 0 ); expect_bytes ( f, 0, 10000, 1000 );
  assert ( f -> Stream ( f, f -> m_CurPos, 48 ) > 0 && prefetching ( f ) );
  expect_bytes ( f, 10000, 10000, 1000 );
  assert ( f -> Stream ( f, f -> m_CurPos, 64 ) > 0 && prefetching ( f ) && s_PF.m_nSlots == 4 );
  __atomic_store_n ( &dev_delay_us, 0, __ATOMIC_SEQ_CST );
  expect_bytes ( f, 20000, 880000, 4096 ); expect_eof ( f );
  f -> Destroy ( f );
  break;
 case 13: /* Stop paths that do not seek resume at the logical cursor. */
  f = open_file ( 900000, 0x00627375 );
  assert ( f -> Stream ( f, 0, 48 ) > 0 ); expect_bytes ( f, 0, 10001, 777 );
  assert ( f -> Seek ( f, 900001 ) == -1 && prefetching ( f ) );   /* rejected before stopping */
  expect_bytes ( f, 10001, 10000, 999 );
  { int tries = 0; while ( reads_now () < 3 && tries++ < 5000 ) usleep ( 200 ); }
  assert ( f -> Stream ( f, 900000, 48 ) == 0 && !prefetching ( f ) );   /* stops, no seek */
  expect_bytes ( f, 20001, 100000, 4096 );                            /* STIO_Fill from m_CurPos */
  expect_bytes ( f, 120001, 779999, 4096 ); expect_eof ( f );
  f -> Destroy ( f );
  break;
 default: assert ( 0 );
 }
 assert ( !s_PF.m_pOwner );
 return 0;
}
'''

cc = os.environ.get("CC", "cc")
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "readahead.c"
    src.write_text(prefix)
    builds = (("asan", ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], []),
              ("asan-diag", ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"], ["-DSMS_DIAG"]),
              ("tsan", ["-fsanitize=thread"], []))
    selected = os.environ.get("SMS_READAHEAD_CASE")
    for name, san, defs in builds:
        exe = Path(tmp) / name
        subprocess.run([cc, "-std=gnu99", "-g", "-O1", "-Wall", "-Wextra", "-Werror", "-Wno-sign-compare",
                        "-Wno-unused-function", "-Wno-unused-parameter", "-Wno-missing-field-initializers", "-no-pie", "-pthread", "-DBDM=1",
                        *defs, *san, str(src), "-o", str(exe)], check=True)
        for case in ([int(selected)] if selected else range(1, 14)):
            env = dict(os.environ, TSAN_OPTIONS="halt_on_error=1 second_deadlock_stack=1")
            r = subprocess.run([str(exe), str(case)], timeout=60, env=env)
            if r.returncode:
                sys.exit(f"{name} case {case} failed ({r.returncode})")
print("STIO USB read-ahead: continuity, overlap, seek/restart, errors, single owner, "
      "non-USB, setup failure, teardown and EOF passed (ASan/UBSan, DIAG, TSan)")
