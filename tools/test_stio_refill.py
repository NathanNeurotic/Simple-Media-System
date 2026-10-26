"""Compile the actual synchronous refill and start-code scanner with host stubs."""
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
s = (root / "src/SMS_FileContext.c").read_text()
a = s.index("static int STIO_Fill (")
b = s.index("}  /* end STIO_Fill */", a) + len("}  /* end STIO_Fill */")
fill = s[a:b]
s = (root / "src/SMS_ContainerMPEG_PS.c").read_text()
a = s.index("static int _next_start_code (")
b = s.index("}  /* end _next_start_code */", a) + len("}  /* end _next_start_code */")
scan = s[a:b]
harness = r"""
#include <assert.h>
#include <string.h>
#define BDM 1
#define MX4SIO_RD_CHUNK 4096
#define MAX_SYNC_SIZE 100000
#define FILE_EOF(f) ((f)->m_CurPos >= (f)->m_Size)
typedef struct { int m_fXio, m_FD, m_RdChunk; } STIOFilePrivate;
typedef struct {
 void *m_pData;
 unsigned m_BufSize, m_CurBuf, m_CurPos, m_Size;
 unsigned char *m_pBuff[2], *m_pPos, *m_pEnd;
} FileContext;
static unsigned calls, total, remaining, max_request, byte_calls;
static int fileXioRead(int fd, char *buffer, unsigned n) {
 unsigned i;
 (void)fd;
 ++calls;
 assert(n <= max_request);
 if (n > remaining) n = remaining;
 for (i = 0; i < n; ++i) buffer[i] = (unsigned char)(total + i);
 remaining -= n; total += n;
 return n;
}
static int fioRead(int fd, void *buffer, unsigned n) {
 return fileXioRead(fd, buffer, n);
}
static unsigned File_GetByte(FileContext *ctx) {
 ++byte_calls;
 if (ctx->m_pPos == ctx->m_pEnd) return 0;
 ++ctx->m_CurPos;
 return *ctx->m_pPos++;
}
""" + fill + scan + r"""
int main(void) {
 unsigned char buffer[393216], data[] = {0,0,1,0xBA};
 STIOFilePrivate priv = {1, 0, 4096};
 FileContext ctx;
 unsigned i;
 memset(&ctx, 0, sizeof(ctx));
 ctx.m_pData = &priv; ctx.m_pBuff[0] = buffer;
 ctx.m_BufSize = sizeof(buffer); ctx.m_Size = 100000;
 remaining = ctx.m_Size; max_request = 4096;
 while (remaining) {
  unsigned before = total;
  int n = STIO_Fill(&ctx);
  assert(n > 0 && n <= 32768);
  for (i = 0; i < (unsigned)n; ++i) assert(buffer[i] == (unsigned char)(before+i));
 }
 assert(total == 100000);
 assert(STIO_Fill(&ctx) == 0);
 priv.m_RdChunk = 65536; max_request = 65536;
 remaining = sizeof(buffer); calls = 0;
 assert(STIO_Fill(&ctx) == sizeof(buffer));
 assert(calls == 6);
 /* A failed byte read before advertised EOF must terminate immediately. */
 ctx.m_pPos = ctx.m_pEnd = buffer; byte_calls = 0;
 assert(_next_start_code(&ctx) == -1 && byte_calls == 1);
 ctx.m_pPos = data; ctx.m_pEnd = data + sizeof(data);
 assert(_next_start_code(&ctx) == 0x1BA);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "test.c"
    exe = Path(tmp) / "test"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-Wall", "-Wextra", "-Werror", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("STIO bounded refill, byte continuity, short EOF, UDPFS sizing and stalled probe passed")
