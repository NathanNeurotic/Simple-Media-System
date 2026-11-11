"""SMS_EE.c helpers compiled from production code: SMS_Rescale with a missing timestamp.

MUL64 (assembly on the console) is a plain 64-bit multiply here.
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
""" + function("SMS_Rescale") + r"""
int main(void) {
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
print("SMS_EE helpers: SMS_Rescale returns for a missing timestamp")
