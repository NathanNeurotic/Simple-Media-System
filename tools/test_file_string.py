"""Actual File_GetString/File_GetByte under short reads and failed refills.

Set SMS_FILE_STRING_SOURCE to a saved SMS_FileContext.c for baseline checks;
SMS_FILE_STRING_CASE selects one scenario (stalled baselines time out).
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = Path(os.environ.get("SMS_FILE_STRING_SOURCE", root / "src/SMS_FileContext.c")).read_text()
header = (root / "include/SMS_FileContext.h").read_text()
start = source.index("void File_GetString (")
end = source.index("}  /* end File_GetString */", start) + len("}  /* end File_GetString */")
function = source[start:end]
start = header.index("static INLINE int File_GetByte (")
end = header.index("}  /* end File_GetByte */", start) + len("}  /* end File_GetByte */")
get_byte = header[start:end]
harness = r"""
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define INLINE inline
#define FILE_EOF(f) ((f)->m_CurPos >= (f)->m_Size)
typedef struct FileContext FileContext;
struct FileContext {
 unsigned m_CurPos,m_Size;
 unsigned char *m_pPos,*m_pEnd;
 int (*Fill)(FileContext *);
};
static unsigned char input[128],buffer[8];
static unsigned actual_size,native_pos,fill_calls,chunk;
static int failure;
static int fill(FileContext *file) {
 unsigned count;
 ++fill_calls;
 if(native_pos==actual_size) { file->m_pPos=file->m_pEnd=buffer;return failure; }
 count=actual_size-native_pos;if(count>chunk) count=chunk;
 memcpy(buffer,input+native_pos,count);native_pos+=count;
 file->m_pPos=buffer;file->m_pEnd=buffer+count;return count;
}
static FileContext file;
static void init(const char *text,unsigned advertised,int error,unsigned refill) {
 actual_size=strlen(text);assert(actual_size<sizeof(input));memcpy(input,text,actual_size);
 native_pos=fill_calls=0;chunk=refill;failure=error;
 file=(FileContext){0,advertised,buffer,buffer,fill};
}
""" + get_byte + function + r"""
int main(int argc,char **argv) {
 char line[32];assert(argc==2);
 if(!strcmp(argv[1],"normal")) {
  const char *text="alpha\r\n\r\nomega";
  init(text,strlen(text),0,3);
  File_GetString(&file,line,sizeof(line));assert(!strcmp(line,"alpha"));
  File_GetString(&file,line,sizeof(line));assert(!strcmp(line,""));
  File_GetString(&file,line,sizeof(line));assert(!strcmp(line,"omega") && FILE_EOF(&file));
  File_GetString(&file,line,sizeof(line));assert(!strcmp(line,""));
 } else if(!strcmp(argv[1],"crlf-boundary")) {
  const char *text="abc\r\nnext";init(text,strlen(text),0,2);
  File_GetString(&file,line,5);assert(!strcmp(line,"abc"));
  File_GetString(&file,line,sizeof(line));assert(!strcmp(line,"next"));
 } else if(!strcmp(argv[1],"overlong")) {
  const char *text="abcdefgh\nnext\n";init(text,strlen(text),0,3);
  File_GetString(&file,line,5);assert(!strcmp(line,"abcd") && file.m_CurPos==9);
  File_GetString(&file,line,5);assert(!strcmp(line,"next") && FILE_EOF(&file));
 } else if(!strcmp(argv[1],"read-failure") || !strcmp(argv[1],"read-error")) {
  int error=!strcmp(argv[1],"read-error")?-5:0;init("abc",100,error,2);
  File_GetString(&file,line,sizeof(line));assert(!strcmp(line,"abc") && FILE_EOF(&file));
  assert(fill_calls==3);
 } else if(!strcmp(argv[1],"drain-failure") || !strcmp(argv[1],"drain-error")) {
  int error=!strcmp(argv[1],"drain-error")?-5:0;init("abcdefghijkl",100,error,3);
  File_GetString(&file,line,4);assert(!strcmp(line,"abc") && FILE_EOF(&file));
  assert(fill_calls==5);
 } else if(!strcmp(argv[1],"zero-capacity")) {
  char guard='Z';init("abc\n",4,0,3);
  File_GetString(&file,&guard,0);assert(guard=='Z' && file.m_CurPos==0 && fill_calls==0);
  File_GetString(&file,NULL,0);assert(file.m_CurPos==0 && fill_calls==0);
 } else if(!strcmp(argv[1],"one-capacity")) {
  char guarded[2]={'Z','Y'};init("abc\nnext",8,0,3);
  File_GetString(&file,guarded,1);
  assert(guarded[0]==0 && guarded[1]=='Y' && file.m_CurPos==4);
  File_GetString(&file,line,sizeof(line));assert(!strcmp(line,"next"));
 } else if(!strcmp(argv[1],"truncated-cr")) {
  const char *text="ab\rjunk\nnext";init(text,strlen(text),0,3);
  File_GetString(&file,line,4);assert(!strcmp(line,"ab\r"));
  File_GetString(&file,line,sizeof(line));assert(!strcmp(line,"next"));
 } else if(!strcmp(argv[1],"empty-failure")) {
  init("",100,0,3);File_GetString(&file,line,sizeof(line));
  assert(!line[0] && FILE_EOF(&file) && fill_calls==1);
 } else if(!strcmp(argv[1],"overlong-eof")) {
  init("abcdef",6,0,3);File_GetString(&file,line,3);
  assert(!strcmp(line,"ab") && FILE_EOF(&file) && fill_calls==2);
 } else assert(0);
 return 0;
}
"""
cases = ("normal", "crlf-boundary", "overlong", "read-failure", "read-error",
         "drain-failure", "drain-error", "zero-capacity", "one-capacity",
         "truncated-cr", "empty-failure", "overlong-eof")
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / "string.c", Path(tmp) / "string"
    src.write_text(harness)
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-Wall", "-Wextra", "-Werror", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    str(src), "-o", str(exe)], check=True)
    selected = os.environ.get("SMS_FILE_STRING_CASE")
    for case in ([selected] if selected else cases):
        subprocess.run([str(exe), case], check=True, timeout=3)
print("File strings: CRLF, truncation/skip, EOF, zero/one-byte capacity and failed refill progress passed")
