"""Production poster caption/centering controls; font/device boundaries are adapters."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
browser = (root / 'src/SMS_GUIFileMenu.c').read_text()
icons = '\n'.join(line for line in (root / 'include/SMS_GUIcons.h').read_text().splitlines()
                  if re.match(r'#define GUICON_(?:FOLDER|AVI|AVIS|PICTURE)\s+\d', line))


def function(name):
    """Extract the production caption helper rather than testing a reimplementation."""
    match = re.search(r'^static (?:int|void) ' + name + r'\s*\([^;{}]*\)\s*\{', browser, re.M)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (browser[end] == '{') - (browser[end] == '}')
        end += 1
    return browser[match.start():end]


prefix = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "SMS_List.h"
static const char *DisplayUTF8(const char *src,char *dst,int capacity) {
 (void)dst;(void)capacity;return src;
}
static int GSFont_WidthEx(char *text,int count,int font) {
 (void)text;return count*(32+font)/2;
}
'''
post = r'''
static void caption(const char *name,int type,int width,int font,char lines[2][320]) {
 SMS_ListNode *node=calloc(1,sizeof(*node)+strlen(name)+1);assert(node);
 strcpy(_STR(node),name);node->m_Param=type;
 _poster_names(node,lines,width,font);
 assert(!strcmp(_STR(node),name));
 assert(GSFont_WidthEx(lines[0],strlen(lines[0]),font)<=width);
 assert(GSFont_WidthEx(lines[1],strlen(lines[1]),font)<=width);
 free(node);
}
int main(void) {
 char lines[2][320],long_name[600];
 caption("Shrek (2001).avi",GUICON_AVI,500,-7,lines);
 assert(!strcmp(lines[0],"Shrek (2001)")&&!lines[1][0]);
 caption("Movie.v1.mp4",GUICON_AVIS,500,-7,lines);
 assert(!strcmp(lines[0],"Movie.v1"));
 caption("Folder.avi",GUICON_FOLDER,500,-7,lines);
 assert(!strcmp(lines[0],"Folder.avi"));
 caption("Photo.jpg",GUICON_PICTURE,500,-7,lines);
 assert(!strcmp(lines[0],"Photo.jpg"));
 caption(".avi",GUICON_AVI,500,-7,lines);assert(!strcmp(lines[0],".avi"));
 caption("Film",GUICON_AVI,500,-7,lines);assert(!strcmp(lines[0],"Film"));
 caption("",GUICON_AVI,500,-7,lines);assert(!lines[0][0]&&!lines[1][0]);
 caption("Film.avi",GUICON_AVI,0,-7,lines);assert(!lines[0][0]&&!lines[1][0]);
 caption("The Lord of the Rings - The Fellowship of the Ring (2001) - Part 3.avi",GUICON_AVI,190,-7,lines);
 assert(lines[0][0]&&strstr(lines[1],"Part 3")&&!strstr(lines[1],".avi"));
 caption("\330\360\345\352 (2001).avi",GUICON_AVI,500,-7,lines);
 assert(!strcmp(lines[0],"\330\360\345\352 (2001)"));
 memset(long_name,'x',590);strcpy(long_name+590,".avi");
 caption(long_name,GUICON_AVI,190,-7,lines);
 assert(strlen(lines[0])<320&&strlen(lines[1])<320);
 assert(_poster_text_offset(45,22,0)==11);
 assert(_poster_text_offset(45,22,1)==0);
 assert(_poster_text_offset(51,22,0)==14);
 assert(_poster_text_offset(51,22,1)==3);
 assert(_poster_text_offset(20,22,1)==0);
 for(int row=20;row<=120;++row) {
  int height=row/2;if(height>22)height=22;
  for(int two=0;two<2;++two) {
   int offset=_poster_text_offset(row,height,two);
   assert(offset>=0&&offset+height*(two?2:1)<=row);
  }
 }
 puts("Poster typography: video extension hiding, original names, CP1251, wrapping/tails, bounds and one/two-line centering passed");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    (tmp / 'tamtypes.h').write_text('#include <stdint.h>\ntypedef uint64_t u64;typedef int64_t s64;\n')
    fixture = tmp / 'typography.c'
    fixture.write_text(prefix + icons + '\n' + function('_poster_names')
                       + function('_poster_text_offset') + post)
    exe = tmp / 'typography'
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c99', '-g', '-no-pie',
                    '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                    '-I', str(tmp), '-I', str(root / 'include'), str(fixture), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
