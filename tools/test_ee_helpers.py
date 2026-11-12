"""SMS_EE.c helpers compiled from production code.

SMS_Rescale with a missing timestamp; MUL64 (assembly on the console) is a plain 64-bit
multiply here. SMS_EEScanDir with file names too long for the config fields its two lists
feed (skin and subtitle-font names); fio and the list are stubbed.
"""
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
ee = (ROOT / "src/SMS_EE.c").read_text(encoding="utf-8", errors="replace")
sms = (ROOT / "include/SMS.h").read_text(encoding="utf-8", errors="replace")
config = (ROOT / "include/SMS_Config.h").read_text(encoding="utf-8", errors="replace")
name_size = {re.search(field + r"\[ *(\d+) *\]", config).group(1) for field in ("m_SkinName", "m_MBFName")}
assert len(name_size) == 1


def function(name):
    start = re.search(r"^\w+ +" + name + r" \(", ee, re.M).start()
    tail = "}  /* end " + name + " */"
    return ee[start:ee.index(tail, start) + len(tail)]


harness = r"""
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef int64_t s64;
typedef uint64_t u64;
#define SMS_INT64( c ) c##LL
""" + re.search(r"^#define SMS_NOPTS_VALUE[^\n]*", sms, re.M).group(0) + r"""
static s64 MUL64(s64 a,s64 b) { return (s64)((u64)a*(u64)b); }
typedef struct { char name[256]; } fio_dirent_t;
typedef struct SMS_List SMS_List;
#define NAME_SIZE """ + name_size.pop() + r"""
static const char *entries[8];
static char listed[8][256];
static int entry, count;
static int fioDopen(const char *path) { (void)path; return 0; }
static int fioDread(int fd,fio_dirent_t *out) { (void)fd; if(!entries[entry]) return 0; strcpy(out->name,entries[entry++]); return 1; }
static int fioDclose(int fd) { (void)fd; return 0; }
static void *SMS_ListFind(SMS_List *list,const char *name) { (void)list;(void)name; return NULL; }
static void SMS_ListPushBack(SMS_List *list,const char *name) { (void)list; strcpy(listed[count++],name); }
""" + function("SMS_Rescale") + function("SMS_EEScanDir") + r"""
static void scan(void) {
 char fits[NAME_SIZE+8], over[NAME_SIZE+8], *field;
 int i;
 memset(fits,'a',NAME_SIZE-1); strcpy(fits+NAME_SIZE-1,".smi");
 memset(over,'b',NAME_SIZE);   strcpy(over+NAME_SIZE,".smi");
 entries[0]="sample.smi"; entries[1]=over; entries[2]=fits; entries[3]=".smi"; entries[4]="notes.txt";
 SMS_EEScanDir("mass0:/SMS/Skins",".smi",NULL);
 assert(count==2 && !strcmp(listed[0],"sample") && strlen(listed[1])==NAME_SIZE-1);
 for(i=0;i<count;++i) {                              /* the menu handlers strcpy a picked name into the config */
  field=malloc(NAME_SIZE); strcpy(field,listed[i]); free(field);
 }
}
int main(void) {
 scan();
 assert(SMS_Rescale(90000,1000,90000)==1000);
 assert(SMS_Rescale(-90000,1000,90000)==-1000);
 assert(SMS_Rescale((s64)1<<40,(s64)1<<33,(s64)1<<33)==(s64)1<<40);   /* 128-bit path */
 assert(SMS_Rescale(-INT64_MAX,1,1)==-INT64_MAX);
 assert(SMS_Rescale(SMS_NOPTS_VALUE,1000,90000)==SMS_NOPTS_VALUE);     /* a missing timestamp stays missing */
 return 0;
}
"""

with tempfile.TemporaryDirectory() as temp:
    src = Path(temp) / "ee.c"
    src.write_text(harness)
    exe = Path(temp) / "ee"
    subprocess.run([os.environ.get("CC", "cc"), "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    "-Wall", "-Werror", str(src), "-o", str(exe)], check=True)
    try:
        failed = subprocess.run([str(exe)], timeout=20, stderr=subprocess.PIPE, text=True)
    except subprocess.TimeoutExpired:
        sys.exit("SMS_EE helpers: no result within 20 s")
    if failed.returncode:
        sys.exit("SMS_EE helpers failed:\n" + "\n".join(failed.stderr.splitlines()[:6]))
print("SMS_EE helpers: SMS_Rescale returns for a missing timestamp; SMS_EEScanDir lists only names that fit the config")
