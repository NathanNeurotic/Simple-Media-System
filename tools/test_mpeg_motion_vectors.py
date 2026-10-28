"""Production MPEG reference setup against independent signed-coordinate math.

Only the MMI macroblock clamp is replaced with equivalent host scalar code.
Native DMA/MMI interpolation is exercised by tools/mpeg_motion_probe instead.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/libmpeg.c").read_text()
header = (root / "include/libmpeg_internal.h").read_text()
start = source.index("static void _mpeg12_get_ref (")
end = source.index("}  /* end _mpeg12_get_ref */", start) + len("}  /* end _mpeg12_get_ref */")
function = source[start:end]
function, replaced = re.subn(r" __asm__ __volatile__\(.*?\n \);", """
 lMBX = lMBX < 0 ? 0 : lMBX >= s_MPEG12Ctx.m_MBWidth ? s_MPEG12Ctx.m_MBWidth - 1 : lMBX;
 lMBY = lMBY < 0 ? 0 : lMBY >= s_MPEG12Ctx.m_MBHeight ? s_MPEG12Ctx.m_MBHeight - 1 : lMBY;
""", function, flags=re.S)
assert replaced == 1, "Recheck the host adapter after a clamp change"
types = ""
for name in ("_MPEGMacroBlock8", "_MPEGMotion", "_MPEGMotions"):
    first = header.index("typedef struct " + name + " {")
    last = header.index("} " + name + ";", first) + len("} " + name + ";")
    types += header[first:last] + "\n"
arrays = source[source.index("static void ( *LumaOp"):source.index("static void ( *PutBlockOp")]
stubs = ""
for name in re.findall(r"_MPEG_(?:put|avg)_(?:luma|chroma)(?:_XY|_X|_Y)?\b", arrays):
    stubs += "static void " + name + ("(_MPEGMotion *p) { (void)p; }\n" if "luma" in name else "(void) {}\n")
prefix = "#include <assert.h>\n#include <stdlib.h>\n#include <stdio.h>\n" + types + stubs + arrays + r'''
static struct { int m_MBWidth,m_MBHeight; _MPEGMotions *m_pCurMotions; } s_MPEG12Ctx;
static int floor_half(int v) { return v>=0?v/2:-((-v+1)/2); }
static int toward_zero_half(int v) { return v<0?-((-v)/2):v/2; }
'''
main = r'''
int main(void) {
 _MPEGMacroBlock8 *frame=calloc(32*32,sizeof(*frame));assert(frame);
 _MPEGMotions motions;unsigned cases=0;
 short residual[384];motions.m_pSPRRes=(unsigned char*)residual;
 s_MPEG12Ctx.m_MBWidth=s_MPEG12Ctx.m_MBHeight=32;
 s_MPEG12Ctx.m_pCurMotions=&motions;
 for(int h=16;h>=8;h-=8)for(int field=0;field<(h==8?2:1);++field)
  for(int dst=0;dst<(h==8?16:8);dst+=8)for(int avg=0;avg<2;++avg)
   for(int dy=-65;dy<=65;++dy)for(int dx=-65;dx<=65;++dx) {
    int step=h==8?2:1,ax=128,ay=step==2?64:128;
    motions.m_nMotions=0;
    _mpeg12_get_ref(frame,ax,ay,dx,dy,h,field,dst,avg);
    _MPEGMotion *m=&motions.m_Motion[0];assert(motions.m_nMotions==1);
    int x=ax+floor_half(dx),y=(ay+floor_half(dy))*step+field;
    int bx=x/16,by=y/16;
    int uvx=toward_zero_half(dx),uvy=toward_zero_half(dy);
    int cx=ax/2+floor_half(uvx),cy=(ay/2+floor_half(uvy))*step+field;
    assert(m->m_pSrc==(unsigned char*)(frame+by*32+bx));
    assert(m->m_X==x-bx*16 && m->m_Y==y-by*16);
    assert(m->m_ChromaX==cx-bx*8 && m->m_ChromaY==cy-by*8);
    assert(m->m_ChromaX<=8 && m->m_ChromaY<=9);
    assert(m->MC_Luma==LumaOp[(dx%2!=0)+2*(dy%2!=0)+4*avg]);
    assert(m->MC_Chroma==ChromaOp[(uvx%2!=0)+2*(uvy%2!=0)+4*avg]);
    assert(m->m_H==h && m->m_fInt==(step==2) && m->m_Field==field);
    assert(m->m_pDstY==residual+dst*16 && m->m_pDstCbCr==residual+256+dst*4);
    ++cases;
   }
 free(frame);printf("MPEG motion setup: %u signed-vector, boundary, field and averaged-prediction cases passed\n",cases);
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temporary:
    tmp = Path(temporary)
    fixture, exe = tmp / "vectors.c", tmp / "vectors"
    fixture.write_text(prefix + function + main)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c99", "-O1", "-g", "-no-pie",
                    "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    str(fixture), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=15)
