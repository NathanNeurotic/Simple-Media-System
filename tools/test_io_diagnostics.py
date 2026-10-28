"""Check actual diagnostic refill/queue accounting without timing or logging I/O."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/SMS_FileContext.c').read_text()
a = source.index('typedef struct STIOFilePrivate {', source.index('} STIOFilePrivate;') + 1)
b = source.index('#endif  /* _WIN32 */', a)
private = source[a:b]
a = source.index('static unsigned int STIO_DiagNow (')
b = source.index('\n#endif\nstatic void STIO_DestroyFileContext', a)
helpers = source[a:b]
a = source.index('static int STIO_Fill (')
b = source.index('}  /* end STIO_Fill */', a) + len('}  /* end STIO_Fill */')
fill = source[a:b]
header = (root / 'include/SMS_RingBuffer.h').read_text()
a = header.index('typedef struct SMS_RingBuffer {')
b = header.index('} SMS_RingBuffer;', a) + len('} SMS_RingBuffer;')
ring_type = header[a:b]
source = (root / 'src/SMS_RingBuffer.c').read_text()
a = source.index('void* SMS_RingBufferWait (')
b = source.index('}  /* end SMS_RingBufferWait */', a) + len('}  /* end SMS_RingBufferWait */')
wait = source[a:b]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define SMS_DIAG 1
#define BDM 1
#define MX4SIO_RD_CHUNK 4096
typedef uint64_t u64;
static u64 g_Timer;
typedef struct {
 void *m_pData;
 unsigned m_BufSize,m_CurBuf;
 unsigned char *m_pBuff[2],*m_pPos,*m_pEnd;
} FileContext;
static int result=4096;
static unsigned request_limit=4096;
static int fileXioRead(int fd,void *buf,unsigned n) {
 (void)fd;assert(n<=request_limit);g_Timer+=8;
 if(result<0)return result;
 if(n>(unsigned)result)n=result;
 memset(buf,0x5A,n);return n;
}
static int fioRead(int fd,void *buf,unsigned n) { return fileXioRead(fd,buf,n); }
static unsigned wait_delay;
static void WaitSema(int id) { if(id==1)g_Timer+=wait_delay; }
static void SignalSema(int id) { assert(id==2); }
'''
main = r'''
int main(void) {
 unsigned char data[65536];STIOFilePrivate priv={0};FileContext ctx={0};
 ctx.m_pData=&priv;ctx.m_pBuff[0]=data;ctx.m_BufSize=sizeof(data);
 priv.m_fXio=1;priv.m_RdChunk=4096;
 assert(STIO_Fill(&ctx)==32768);
 assert(priv.m_ReadCalls==8 && priv.m_ReadBytes==32768);
 assert(priv.m_ReadMs==64 && priv.m_MaxReadMs==8 && !priv.m_ReadErrors);
 assert(priv.m_Refills==1 && priv.m_RefillMs==64 && priv.m_MaxRefillMs==64);
 assert(ctx.m_pPos==data && ctx.m_pEnd==data+32768);
 result=-7;assert(STIO_Fill(&ctx)==0);
 assert(priv.m_ReadCalls==9 && priv.m_ReadErrors==1 && priv.m_Refills==2);
 assert(priv.m_ReadBytes==32768 && ctx.m_pEnd==data);
 /* Zero bytes are EOF, not a device error. */
 result=0;assert(STIO_Fill(&ctx)==0 && priv.m_ReadErrors==1);
 /* Both a non-BDM fio refill and the low 32-bit timer wrap are accounted. */
 memset(&priv,0,sizeof(priv));ctx.m_BufSize=4096;result=4096;g_Timer=0xFFFFFFF8ULL;
 assert(STIO_Fill(&ctx)==4096);
 assert(priv.m_ReadCalls==1 && priv.m_ReadMs==8 && priv.m_RefillMs==8);
 SMS_RingBuffer ring={0};ring.m_Sema[1]=1;ring.m_Sema[2]=2;
 ring.m_pBeg=data;ring.m_pOut=data+8;ring.m_pJmp=data+24;ring.m_pEnd=data+32;
 wait_delay=24;assert(SMS_RingBufferWait(&ring)==data+8);
 assert(ring.m_WaitEvents==1 && ring.m_WaitMs==24 && ring.m_MaxWaitMs==24);
 wait_delay=8;assert(SMS_RingBufferWait(&ring)==data+8);
 assert(ring.m_WaitEvents==2 && ring.m_WaitMs==32 && ring.m_MaxWaitMs==24);
 wait_delay=0;assert(SMS_RingBufferWait(&ring)==data+8 && ring.m_WaitEvents==2);
 /* Instrumentation must preserve queue wrap and occupancy updates. */
 ring.m_pOut=ring.m_pJmp;assert(SMS_RingBufferWait(&ring)==data);
 assert(ring.m_Size==8 && ring.m_pJmp==ring.m_pEnd);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / 'diag.c', Path(tmp) / 'diag'
    src.write_text(prefix + private + helpers + fill + ring_type + wait + main)
    subprocess.run([os.environ.get('CC', 'cc'), '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-g', '-no-pie', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
print('I/O diagnostics: read/refill time, errors, EOF, timer wrap and queue-wait accounting passed')
