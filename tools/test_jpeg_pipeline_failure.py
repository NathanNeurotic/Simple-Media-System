"""Actual JPEG MCU pipeline abort waits for issued DMA/VU work.

Hardware busy registers/kicks and entropy unpack are deterministic stubs.
This tests abort ordering and scheduling, not physical VU execution/liveness.
"""
import os
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
src = (root / 'src/SMS_JPEG.c').read_text()
marker = '}  /* end _process_MCU */'
end = src.index(marker) + len(marker)
start = src.rfind('\nstatic ', 0, src.index(marker)) + 1
constants = src[src.index('#define STS_VU0IDCT'):src.index('#define MK_SOI')]
harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef uint64_t u64;
#define __SMS_H
#include "SMS_JPEG.h"
#include "SMS_Rescale.h"
''' + constants + r'''
static short scratch[80*12];
static unsigned int dst[2048];
#define BLK_RB_SLOT(c,slot) ((slot)==(c)->m_pBlkEnd?scratch:(slot))
#define BLK_RB_EMPTY(c) ((c)->m_pBlkIn==(c)->m_pBlkOut)
#define BLK_RB_FULL(c) (BLK_RB_SLOT(c,(c)->m_pBlkIn+80)==(c)->m_pBlkOut)
#define BLK_RB_PUSHSLOT(c) ((c)->m_pBlkIn)
#define BLK_RB_POPSLOT(c) ((c)->m_pBlkOut)
#define BLK_RB_PUSHADVANCE(c) ((c)->m_pBlkIn=BLK_RB_SLOT(c,(c)->m_pBlkIn+80))
#define BLK_RB_POPADVANCE(c) ((c)->m_pBlkOut=BLK_RB_SLOT(c,(c)->m_pBlkOut+80))
#define DMAC_GIF 2
static unsigned vif0,vif1,vu0,vu1,dma9,decoded,fail_at,kicks,polls;
static unsigned tick(unsigned *p){unsigned busy=*p;if(busy)--*p;++polls;return busy;}
static unsigned VIF0Busy(void){return tick(&vif0);}
static unsigned VIF1Busy(void){return tick(&vif1);}
static unsigned VU01Busy(void){unsigned a=tick(&vif0),b=tick(&vu0),c=tick(&vu1);return a|b|c;}
static unsigned VU1Busy(void){return tick(&vu1);}
static unsigned DMA9Busy(void){return tick(&dma9);}
static void VIF0KickSPR(void *p){assert(p>= (void*)scratch&&p<(void*)(scratch+80*12));vif0=3;++kicks;}
static void VU0Kick(void){vu0=3;++kicks;}
static void VU1Kick(void){vu1=3;++kicks;}
static void DMA9Kick(void *dst,void *src,unsigned n){dma9=3;++kicks;}
static void DMA_Wait(int ch){assert(ch==DMAC_GIF);}
static void pack(void *p,int stride,int n){}
static void stripe(SMS_RescaleContext *p){p->m_pStripePtr=dst;}
static int _unpack(SMS_JPEGContext *p,short *block,_jpeg_comp_desc *desc) {
 if(++decoded==fail_at){vif0=vif1=vu0=vu1=dma9=4;return 0;}return 1;
}
''' + src[start:end] + r'''
int main(void) {
 for(unsigned fail=1;fail<=24;++fail) {
  SMS_JPEGContext p={0};SMS_RescaleContext rescale={0};
  p.m_nMCU=3;p.m_MW=4;p.m_MH=2;p.m_MCUIncr=8;p.m_nBytesPM=1024;
  p.m_pBlkIn=p.m_pBlkOut=scratch;p.m_pBlkEnd=scratch+80*12;
  p.m_pDst=dst;p.m_pRC=&rescale;p.pack=pack;
  rescale.m_Stride=32;rescale.m_pStripePtr=dst;rescale.ProcessStripe=stripe;
  fail_at=fail;decoded=kicks=polls=vif0=vif1=vu0=vu1=dma9=0;
  assert(!_process_MCU(&p,8));
  assert(decoded==fail&&polls&&!vif0&&!vif1&&!vu0&&!vu1&&!dma9);
  if(fail>1)assert(kicks);
 }
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    c, exe = Path(temp) / 'pipeline.c', Path(temp) / 'pipeline'
    c.write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), '-g', '-no-pie', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', '-I', str(root / 'include'), str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=10)
print('JPEG pipeline: every block failure waits for VIF0/VIF1, VU0/VU1 and DMA9 to finish before returning')
