"""IPCONFIG.DAT parsing at boot and the IP editor's octet cells, compiled from production code.

Production code: _read_ipconfig (SMS_IOP.c), _set_ips (SMS_GUIMenuSMS.c) and the address
buffers from SMS_Locale.c. fio is a single in-memory file. Network bring-up is not exercised.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(path, name):
    text = (ROOT / path).read_text(encoding="utf-8", errors="replace")
    start = re.search(r"static (?:void|int) " + name + r" \(", text).start()
    tail = "}  /* end " + name + " */"
    return text[start:text.index(tail, start) + len(tail)]


locale = (ROOT / "src/SMS_Locale.c").read_text(encoding="utf-8", errors="replace")
buffers = "\n".join(re.search(r"^char " + name + r" *\[\][^\n]*", locale, re.M).group(0)
                    for name in ("g_pDefIP", "g_pDefMask", "g_pDefGW", "g_pIPConf"))

harness = r"""
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
""" + buffers + r"""
static char g_pBootDir[128];
static const char *content;
static int fioOpen(const char *path,int flags) { (void)flags; return strcmp(path,g_pIPConf) ? -1 : 3; }
static int fioRead(int fd,void *data,int size) {
 int length=(int)strlen(content);
 (void)fd;
 if(length>size) length=size;
 memcpy(data,content,length);
 return length;
}
static int fioClose(int fd) { (void)fd; return 0; }
""" + function("src/SMS_IOP.c", "_read_ipconfig") + function("src/SMS_GUIMenuSMS.c", "_set_ips") + r"""
static void parse(const char *file,const char *ip,const char *mask,const char *gw) {
 strcpy(g_pDefIP,"10.0.0.2"); strcpy(g_pDefMask,"255.0.0.0"); strcpy(g_pDefGW,"10.0.0.1");
 content=file;
 assert(_read_ipconfig()==1);
 if(strcmp(g_pDefIP,ip)||strcmp(g_pDefMask,mask)||strcmp(g_pDefGW,gw)) {
  fprintf(stderr,"IPCONFIG parse: ip=[%s] mask=[%s] gw=[%s], expected [%s] [%s] [%s]\n",g_pDefIP,g_pDefMask,g_pDefGW,ip,mask,gw);
  exit(1);
 }
}
/* the source sits at the very end of a heap block, so a read past its terminator is caught */
static void edit(char cells[][4],const char *address) {
 size_t size=strlen(address)+1;
 char *copy=malloc(size);
 memcpy(copy,address,size);
 _set_ips(cells,copy);
 free(copy);
}
int main(void) {
 char cells[4][4];
 parse("192.168.1.10\n255.255.255.0\n192.168.1.1\n","192.168.1.10","255.255.255.0","192.168.1.1");       /* as SMS saves it */
 parse("192.168.1.10 255.255.255.0 192.168.1.1","192.168.1.10","255.255.255.0","192.168.1.1");
 parse("192.168.1.10\r\n255.255.255.0\r\n192.168.1.1\r\n","192.168.1.10","255.255.255.0","192.168.1.1");  /* Windows editor */
 parse("192.168.1.10  255.255.255.0  192.168.1.1","192.168.1.10","255.255.255.0","192.168.1.1");
 parse("\r\n192.168.1.10\n255.255.255.0\n192.168.1.1","192.168.1.10","255.255.255.0","192.168.1.1");
 parse("192.168.1.10\r\n","192.168.1.10","255.0.0.0","10.0.0.1");                                         /* absent fields keep their value */
 parse("123.123.123.123 255.255.255.255 234.234.234.234\n","123.123.123.123","255.255.255.255","234.234.234.234");

 memset(cells,'x',sizeof(cells));
 edit(cells,"192.168.1.100");
 assert(!strcmp(cells[0],"192")&&!strcmp(cells[1],"168")&&!strcmp(cells[2],"1")&&!strcmp(cells[3],"100"));
 edit(cells,"10.0.0.7");                              /* shorter octets replace longer ones */
 assert(!strcmp(cells[0],"10")&&!strcmp(cells[1],"0")&&!strcmp(cells[2],"0")&&!strcmp(cells[3],"7"));
 edit(cells,"1.22222.3.4");                           /* an over-long octet stays inside its cell */
 assert(!strcmp(cells[0],"1")&&!strcmp(cells[1],"222")&&!strcmp(cells[2],"3")&&!strcmp(cells[3],"4"));
 edit(cells,"172.16");                                /* truncated address */
 assert(!strcmp(cells[0],"172")&&!strcmp(cells[1],"16")&&!cells[2][0]&&!cells[3][0]);
 edit(cells,"");                                      /* empty field, e.g. no netmask in the file */
 assert(!cells[0][0]&&!cells[1][0]&&!cells[2][0]&&!cells[3][0]);
 return 0;
}
"""

with tempfile.TemporaryDirectory() as temp:
    src = Path(temp) / "ipconfig.c"
    src.write_text(harness)
    exe = Path(temp) / "ipconfig"
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    "-Wall", "-Werror", str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=10)
print("IPCONFIG: LF, space, CRLF, doubled-separator and partial files parse; editor cells stay bounded and terminated")
