"""Execute actual start/resume Stream call expressions to check block-count units."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
calls = []
pattern = r"(?:s_Player\.m_pFileCtx|lpFileCtx)\s*->\s*Stream\s*\([^;]*?m_StreamSize[^;]*?\)"
for name, count in (("SMS_Player.c", 1), ("SMS_PlayerControl.c", 2)):
    matches = re.findall(pattern, (root / "src" / name).read_text())
    assert len(matches) == count, (name, matches)
    calls.extend(matches)
harness = r"""
#include <assert.h>
typedef struct FileContext FileContext;
struct FileContext {
 unsigned m_CurPos,m_StreamSize;
 int (*Stream)(FileContext *,unsigned,unsigned);
};
static struct { FileContext *m_pFileCtx;unsigned m_StartPos; } s_Player;
static unsigned expected,invocations;
static int stream(FileContext *c,unsigned pos,unsigned n) {
 assert(pos==c->m_CurPos && n==expected);++invocations;return 1;
}
int main(void) {
 static FileContext ctx={123,0,stream};FileContext *lpFileCtx=&ctx;
 unsigned lFilePos=ctx.m_CurPos;
 s_Player.m_pFileCtx=&ctx;
 for(unsigned size=384;size<=432;size+=48) {
  ctx.m_StreamSize=size;expected=size/8;
""" + "\n".join(call + ";" for call in calls) + r"""
 }
 assert(invocations==6);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / "rebuffer.c", Path(tmp) / "rebuffer"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-Wall", "-Wextra", "-Werror",
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
print("Playback start and both rebuffer call sites consistently request 48 STIO blocks / 54 CDDA sectors")
