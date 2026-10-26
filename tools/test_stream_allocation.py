"""Host fault injection for the actual shared STIO/CDDA stream allocator.
Run with Python 3 and a host C compiler (CC defaults to cc).
"""
import os
from pathlib import Path
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / "src/SMS_FileContext.c").read_text()
start = source.index("static int File_AllocStreamBuffers (")
end = source.index("\nstatic int CDDA_Stream", start)
block = source[start:end]
harness = r"""
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef struct {
 void *m_pBuff[2], *m_pPos, *m_pEnd;
 unsigned m_BufSize, m_CurBufSize;
} FileContext;
static int allocation, fail_at;
static void *test_alloc(unsigned align, unsigned size) {
 (void)align;
 return ++allocation == fail_at ? NULL : malloc(size);
}
#define memalign test_alloc
""" + block + r"""
int main(void) {
 int fail, count;
 for (count = 1; count <= 2; ++count) for (fail = 0; fail <= count; ++fail) {
  FileContext ctx, before;
  memset(&ctx, 0, sizeof(ctx));
  ctx.m_pBuff[0] = malloc(4096);
  ctx.m_pBuff[1] = malloc(4096);
  ctx.m_pPos = ctx.m_pBuff[0];
  ctx.m_pEnd = (char *)ctx.m_pBuff[0] + 4096;
  ctx.m_BufSize = ctx.m_CurBufSize = 4096;
  before = ctx;
  allocation = 0; fail_at = fail;
  if (fail) {
   assert(File_AllocStreamBuffers(&ctx, 393216, count) == 0);
   assert(memcmp(&ctx, &before, sizeof(ctx)) == 0);
   memset(ctx.m_pBuff[0], 0x55, 4096);
   memset(ctx.m_pBuff[1], 0xAA, 4096);
  } else {
   assert(File_AllocStreamBuffers(&ctx, 393216, count) == 1);
   assert(ctx.m_BufSize == 393216 && ctx.m_CurBufSize == 393216);
   memset(ctx.m_pBuff[0], 0x55, ctx.m_BufSize);
   if (count == 2) memset(ctx.m_pBuff[1], 0xAA, ctx.m_BufSize);
   else assert(ctx.m_pBuff[1] == NULL);
  }
  free(ctx.m_pBuff[0]); free(ctx.m_pBuff[1]);
 }
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "test.c"
    exe = Path(tmp) / "test"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-Wall", "-Wextra", "-Werror", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("STIO/CDDA allocation: one/two-buffer success and each allocation failure passed")
