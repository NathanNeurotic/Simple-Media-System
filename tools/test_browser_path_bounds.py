"""Exercise the production browser path builder at its capacity boundaries."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/SMS_GUIFileMenu.c').read_text()
start = source.index('static int _make_path (')
end = source.index('}  /* end _make_path */', start) + len('}  /* end _make_path */')
fixture = r'''
#include <assert.h>
#include <string.h>
#include <stddef.h>
static char g_CWD[1024];
''' + source[start:end] + r'''
int main(void) {
 char out[1024], name[1024], *end;
 memset(name,'n',sizeof(name)); name[1023]=0;
 for(unsigned prefix=0;prefix<1024;++prefix) {
  memset(g_CWD,'d',prefix);g_CWD[prefix]=0;
  for(unsigned slash=0;slash<2;++slash) {
   if(prefix)g_CWD[prefix-1]=slash?'/':'d';
   for(unsigned length=0;length<1024;++length) {
    name[length]=0;
    memset(out,0x5a,sizeof(out));end=(char *)1;
    unsigned separator=prefix && !slash;
    int fits=prefix+separator+length<sizeof(out);
    assert(_make_path(out,sizeof(out),&end,name)==fits);
    if(fits) {
     assert(end==out+prefix+separator);
     assert(strlen(out)==prefix+separator+length);
     assert(!strcmp(end,name));
    } else { assert(!end);assert((unsigned char)out[0]==0x5a); }
    name[length]='n';
   }
  }
 }
 strcpy(g_CWD,"mass0:");
 assert(!_make_path(out,0,&end,"x") && !end);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as td:
    path = Path(td)
    (path / 'test.c').write_text(fixture)
    exe = path / 'test'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-O1', '-g', '-no-pie',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    str(path / 'test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=60)
print('Browser path bounds: 2097152 capacity cases and zero capacity passed')
