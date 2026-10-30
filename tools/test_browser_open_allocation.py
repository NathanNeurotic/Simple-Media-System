"""Fault-inject the production media-open handoff allocation and cleanup block."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/SMS_GUIFileMenu.c').read_text()
start = source.index('  retVal = malloc (', source.index('void** SMS_OpenMediaFile ('))
end = source.index('  if ( aFlags & SMS_FA_FLAGS_SUB )', start)
fixture = r'''
#include <assert.h>
#include <stddef.h>
typedef struct FileContext FileContext;
struct FileContext { void (*Destroy)(FileContext *); };
static unsigned destroyed,stopped,fail;
static void *storage[3];
static FileContext context;
static void destroy(FileContext *p) { assert(p==&context);++destroyed; }
static void SMS_PgIndStop(void) { ++stopped; }
static void *allocate(size_t n) { assert(n==sizeof(storage));return fail?NULL:storage; }
#define malloc allocate
static void **handoff(void) {
 FileContext *lpFileCtx=&context;
 void **retVal=NULL;
''' + source[start:end] + r'''
 return retVal;
}
int main(void) {
 context.Destroy=destroy;
 fail=1;assert(!handoff());assert(destroyed==1 && stopped==1);
 fail=0;void **result=handoff();
 assert(result==storage && result[0]==&context && !result[1]);
 assert(destroyed==1 && stopped==1);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    tmp = Path(td)
    (tmp / 'test.c').write_text(fixture)
    exe = tmp / 'test'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-g', '-no-pie',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    str(tmp / 'test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
print('Media handoff allocation: failed allocation closes handle and balances progress; success retains ownership')
