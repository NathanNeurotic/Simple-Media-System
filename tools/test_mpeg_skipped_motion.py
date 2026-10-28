"""Actual slice/MC dispatch: H.262 7.6.6 implicit skipped-block modes.

VLC/IPU/device calls are controlled. This tests state flow, not native decoding.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/libmpeg.c").read_text()
header = (root / "include/libmpeg_internal.h").read_text()
public = (root / "include/libmpeg.h").read_text()


def function(name):
    start = source.index("static " + ("int " if name == "_mpeg12_slice" else "void ") + name + " (")
    end = source.index("}  /* end " + name + " */", start) + len("}  /* end " + name + " */")
    return source[start:end]


sequence = public[public.index("typedef struct MPEGSequenceInfo {"):public.index("} MPEGSequenceInfo;") + len("} MPEGSequenceInfo;")]
types = header[header.index("typedef struct _MPEGMBXY {"):header.index("} _MPEGContext;") + len("} _MPEGContext;")]
types = re.sub(r"#ifdef _EE.*?#endif", "", types, flags=re.S)
constants = "\n".join(re.findall(r"^#define _MPEG_\w+\s+[^\n]*", header, re.M))
arrays = source[source.index("static void ( *PutBlockOp"):source.index("static float s_FrameRate")]
names = sorted(set(re.findall(r"_MPEG_(?:put|add)_block_\w+", arrays)))
prefix = r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef long long s64;
#define UNCACHED_SEG(p) (p)
''' + sequence + types + "\n" + constants + "\n" + "\n".join("static void " + name + "(_MPEGMotions *p) { (void)p; }" for name in names) + arrays + r'''
static _MPEGContext s_MPEG12Ctx;
static int decoded,dispatched,increments,is_b,structure,initial_motion;
static int _MPEG_NextStartCode(void) { return _MPEG_CODE_SLICE_MIN; }
static int _MPEG_GetBits(int n) { return n==5?1:0; }
static int _MPEG_ShowBits(int n) { assert(n==23);return 1; }
static int _MPEG_GetMBAI(void) { return ++increments==1?1:3; }
static int _MPEG_WaitBDEC(void) { return 1; }
static void _xtra_bitinf(void) { assert(0); }
static int _mpeg12_dec_mb(int *type,int *motion,int pmv[2][2][2],int fields[2][2],int dual[2]) {
 int i,j,k;int seed=++decoded*100;
 *type=_MPEG_MBT_PATTERN|_MPEG_MBT_MOTION_FORWARD|(is_b?_MPEG_MBT_MOTION_BACKWARD:0);
 *motion=decoded==1?initial_motion:_MPEG_MC_FRAME;
 for(i=0;i<2;++i)for(j=0;j<2;++j) {
  fields[i][j]=(i+j+1)&1;
  for(k=0;k<2;++k)pmv[i][j][k]=seed+10*i+2*j+k;
 }
 dual[0]=dual[1]=0;return 1;
}
static void _mpeg12_get_refs(int x,int y,int type,int motion,int pmv[2][2][2],int fields[2][2],int dual[2]) {
 int skipped=dispatched%3!=0;
 int expected=dispatched<3?initial_motion:_MPEG_MC_FRAME;
 if(skipped)expected=structure==_MPEG_PS_FRAME?_MPEG_MC_FRAME:_MPEG_MC_FIELD;
 assert(motion==expected);
 for(int i=0;i<2;++i)for(int j=0;j<2;++j)for(int k=0;k<2;++k) {
  int want=(dispatched<3?100:200)+10*i+2*j+k;
  if(skipped && !is_b && j==0)want=0;
  assert(pmv[i][j][k]==want);
 }
 if(skipped && structure!=_MPEG_PS_FRAME) {
  int field=structure==_MPEG_PS_BOTTOM_FIELD;
  assert(fields[0][0]==field && fields[0][1]==field);
 } else for(int i=0;i<2;++i)for(int j=0;j<2;++j)assert(fields[i][j]==((i+j+1)&1));
 (void)x;(void)y;(void)type;(void)dual;
}
static void verify_dispatch(void) {
 int field=structure!=_MPEG_PS_FRAME,skipped=dispatched%3!=0;
 int motion=dispatched<3?initial_motion:_MPEG_MC_FRAME;
 if(skipped)motion=field?_MPEG_MC_FIELD:_MPEG_MC_FRAME;
 void (*expected)(_MPEGMotions*)=skipped?PutBlockOp[!field]:AddBlockOp[field][motion&_MPEG_MC_FIELD];
 assert(s_MPEG12Ctx.m_pCurMotions->BlockOp==expected);
 ++dispatched;
}
static void (*DoMC)(void)=verify_dispatch;
'''
main = r'''
int main(void) {
 _MPEGMBXY locations[6];unsigned char frame[6*384],residual[768];
 for(int i=0;i<6;++i) { locations[i].m_X=i;locations[i].m_Y=0; }
 for(is_b=0;is_b<=1;++is_b)for(structure=_MPEG_PS_TOP_FIELD;structure<=_MPEG_PS_FRAME;++structure)
  for(initial_motion=_MPEG_MC_FIELD;initial_motion<=_MPEG_MC_FRAME;++initial_motion) {
   memset(&s_MPEG12Ctx,0,sizeof(s_MPEG12Ctx));
   decoded=dispatched=increments=0;
   s_MPEG12Ctx.m_PictCodingType=is_b?_MPEG_PT_B:_MPEG_PT_P;
   s_MPEG12Ctx.m_PictStruct=structure;s_MPEG12Ctx.m_MBWidth=6;
   s_MPEG12Ctx.m_pMBXY=locations;
   s_MPEG12Ctx.m_pCurFrameY=frame;s_MPEG12Ctx.m_pCurFrameCbCr=frame+256;
   for(int i=0;i<2;++i) { s_MPEG12Ctx.m_MC[i].m_pSPRRes=residual;s_MPEG12Ctx.m_MC[i].m_pSPRBlk=residual; }
   assert(_mpeg12_slice(6)==-1 && decoded==2 && dispatched==6);
  }
 puts("MPEG skipped motion: H.262 implicit modes, B predictor retention, P reset, field parity and put/add layouts passed");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    fixture, exe = tmp / "skipped.c", tmp / "skipped"
    fixture.write_text(prefix + function("_mpeg2_mc") + function("_mpeg12_slice") + main)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-O1", "-g", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    str(fixture), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
