"""Actual JPEG rescaler init: failed growth preserves both old buffers/context."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'src/SMS_Rescale.c').read_text()

def function(name):
    marker = '}  /* end ' + name + ' */'
    end = source.index(marker) + len(marker)
    starts = list(re.finditer(r'^.*\b' + name + r'\s*\([^;]*?\)\s*\{', source[:end], re.M))
    return source[starts[-1].start():end]

harness = r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#define __SMS_H
#include "SMS_Rescale.h"
static int fail_at,allocations,live;
static void *allocate(size_t n) {
 if(++allocations==fail_at)return NULL;
 void *p=malloc(n);assert(p);++live;return p;
}
static void *zero_alloc(size_t n,size_t size) {void *p=allocate(n*size);if(p)memset(p,0,n*size);return p;}
static void release(void *p) {if(p){--live;free(p);}}
#define malloc allocate
#define calloc zero_alloc
#define free release
static void _rescale_stripe(SMS_RescaleContext *p){}
static void _rescale_buffer(SMS_RescaleContext *p,void *out){}
static void _copy_stripe(SMS_RescaleContext *p){}
static void _copy_buffer(SMS_RescaleContext *p,void *out){}
'''
for name in ('_precalc_x', '_precalc_y', 'SMS_RescaleInit', 'SMS_RescaleDestroy'):
    harness += function(name)
harness += r'''
int main(void) {
 for(int fail=0;fail<=3;++fail) {
  allocations=0;fail_at=fail;
  SMS_RescaleContext *p=SMS_RescaleInit(NULL,64,64,16,16);
  assert((p!=NULL)==!fail);
  SMS_RescaleDestroy(p);assert(!live);
 }
 for(int fail=1;fail<=2;++fail) {
  fail_at=allocations=0;
  SMS_RescaleContext *p=SMS_RescaleInit(NULL,64,64,16,16),snapshot;
  assert(p&&live==3);memset(p->m_pTemp,0x5A,p->m_nAlloc);memcpy(&snapshot,p,sizeof(snapshot));
  fail_at=fail;allocations=0;
  assert(!SMS_RescaleInit(p,1024,1024,16,16));
  assert(!memcmp(&snapshot,p,sizeof(snapshot))&&live==3);
  for(unsigned i=0;i<p->m_nAlloc;++i)assert(((unsigned char *)p->m_pTemp)[i]==0x5A);
  fail_at=allocations=0;
  assert(SMS_RescaleInit(p,1024,1024,16,16)==p&&live==3);
  assert(p->m_NewWidth>0&&p->m_NewWidth<=1024&&p->m_NewHeight>0&&p->m_NewHeight<=1024);
  SMS_RescaleDestroy(p);assert(!live);
 }
 fail_at=allocations=0;
 SMS_RescaleContext *p=SMS_RescaleInit(NULL,64,64,8,8);
 void *temp=p->m_pTemp,*stripe=p->m_pStripe;fail_at=1;allocations=0;
 assert(SMS_RescaleInit(p,64,64,8,8)==p&&!allocations&&p->m_pTemp==temp&&p->m_pStripe==stripe);
 SMS_RescaleDestroy(p);assert(!live);
 fail_at=allocations=0;
 assert(!SMS_RescaleInit(NULL,0,16,16,16));assert(!SMS_RescaleInit(NULL,16,16,0,16));
 assert(!SMS_RescaleInit(NULL,65536,16,16,16));assert(!allocations);
 for(int tall=0;tall<2;++tall) {
  p=SMS_RescaleInit(NULL,tall?1:65520,tall?65535:1,16,16);
  assert(p&&p->m_NewWidth>=1&&p->m_NewHeight>=1);
  assert(p->m_NewWidth<=1024&&p->m_NewHeight<=1024);
  SMS_RescaleDestroy(p);assert(!live);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src, exe = Path(tmp) / 'rescale.c', Path(tmp) / 'rescale'
    src.write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), '-g', '-no-pie', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-I', str(root / 'include'), str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
print('Rescaler: first/later allocation failure, transactional reuse/retry, buffer reuse and narrow images passed')
