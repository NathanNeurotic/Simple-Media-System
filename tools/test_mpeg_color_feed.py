"""Actual decoder probe feed with anonymous host memory standing in for MMIO.

This checks chunk accounting and register values, not EE/IPU DMA execution.
"""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "tools/mpeg_color_probe/main.c").read_text()
start = source.index("static int feed (")
end = source.index("\nstatic void* sequence", start)
prefix = r'''
#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
static unsigned sent,transfers,padded_input_size;
static void *input=(void *)0x01000000;
'''
main = r'''
int main(void) {
 void *mapped=mmap((void *)0x1000B000,4096,PROT_READ|PROT_WRITE,
                  MAP_PRIVATE|MAP_ANONYMOUS|MAP_FIXED_NOREPLACE,-1,0);
 assert(mapped==(void *)0x1000B000);
 volatile unsigned *control=(volatile unsigned *)0x1000B400;
 volatile unsigned *address=(volatile unsigned *)0x1000B410;
 volatile unsigned *qwc=(volatile unsigned *)0x1000B420;
 const unsigned sizes[]={16,1048560,1048576,2097152,8388608};
 for(unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
  sent=transfers=0;padded_input_size=sizes[i];
  unsigned total=0;
  while(total<padded_input_size) {
   *control=0;assert(feed(NULL)==1);
   assert(*address==0x01000000+total && *qwc>0 && *qwc<=65535);
   assert(*control==0x101);total+=*qwc*16;assert(sent==total);
  }
  assert(total==padded_input_size && !feed(NULL));
  assert(transfers==(padded_input_size+1048559)/1048560);
 }
 assert(!munmap(mapped,4096));return 0;
}
'''
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    fixture, exe = tmp / "feed.c", tmp / "feed"
    fixture.write_text(prefix + source[start:end] + main)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-g", "-no-pie",
                    "-Wno-pointer-to-int-cast", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    str(fixture), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
print("MPEG color feed: single/multiple DMA chunks, 16-bit QWC limit, continuity and EOF accounting passed")
