/* Native oracle: actual C vector setup, SPR DMA and MMI interpolation. */
#include <stddef.h>
#include DECODER_SOURCE

#ifndef VECTOR_MIN
#define VECTOR_MIN -17
#endif
typedef char motion_stride_check[sizeof(_MPEGMotion) == 40 ? 1 : -1];
typedef char motion_callback_check[offsetof(_MPEGMotion, MC_Luma) == 32 ? 1 : -1];

static unsigned cases, pixels, failures;
static _MPEGMacroBlock8 reference[64] __attribute__((aligned(128)));

static int sample(int plane, int x, int y) {
 return (x * (plane + 3) + y * (plane + 7) + (x ^ (y * 3)) * 11) & 255;
}

static int prediction(int plane, int x, int y, int dx, int dy, int stride) {
 int sum=sample(plane,x,y), shift=0;
 if(dx) { sum+=sample(plane,x+1,y);shift=1; }
 if(dy) {
  sum+=sample(plane,x,y+stride);
  if(dx)sum+=sample(plane,x+1,y+stride);
  ++shift;
 }
 return (sum+(shift ? 1<<(shift-1) : 0))>>shift;
}

static int check(int dx,int dy,int h,int field,int dst,int avg) {
 int i,j,plane,step=h==8?2:1;
 int ax=64,ay=step==2?32:64;
 _MPEGMotions* motions=&s_MPEG12Ctx.m_MC[0];
 short* res=(short*)0x70000000;
 motions->m_nMotions=0;
 motions->m_pSPRRes=(void*)res;
 s_MPEG12Ctx.m_pCurMotions=motions;
 for(i=0;i<384;++i)res[i]=77;
 _mpeg12_get_ref(reference,ax,ay,dx,dy,h,field,dst,avg);
 _MPEG_dma_ref_image((_MPEGMacroBlock8*)0x70000600,motions->m_Motion,1,8);
 while(*(volatile unsigned*)0x1000D400 & 0x100) {}
 _MPEG_do_mc(motions->m_Motion);
 ++cases;
 for(plane=0;plane<3;++plane) {
  int sub=plane?2:1,width=16/sub,height=h/sub;
  int vx=plane?dx/2:dx,vy=plane?dy/2:dy;
  int x=ax/sub+(vx>>1),y=(ay/sub+(vy>>1))*step+field;
  short* output=plane?res+256+(plane-1)*64+dst*4:res+dst*16;
  for(j=0;j<height;++j)for(i=0;i<width;++i) {
   int expected=prediction(plane,x+i,y+j*step,vx&1,vy&1,step);
   int actual=output[j*width+i];
   if(avg)expected=(expected+77+1)>>1;
   ++pixels;
   if(actual!=expected) {
    printf("[sms-motion] mismatch dx=%d dy=%d h=%d field=%d dst=%d avg=%d plane=%d x=%d y=%d actual=%d expected=%d\n",dx,dy,h,field,dst,avg,plane,i,j,actual,expected);
    ++failures;return 0;
   }
  }
 }
 return 1;
}

int main(void) {
 int bx,by,x,y,h,field,dst,avg,dx,dy;
 memset(&s_MPEG12Ctx,0,sizeof(s_MPEG12Ctx));
 s_MPEG12Ctx.m_MBWidth=s_MPEG12Ctx.m_MBHeight=8;
 for(by=0;by<8;++by)for(bx=0;bx<8;++bx) {
  _MPEGMacroBlock8* block=&reference[by*8+bx];
  for(y=0;y<16;++y)for(x=0;x<16;++x)block->m_Y[y][x]=sample(0,bx*16+x,by*16+y);
  for(y=0;y<8;++y)for(x=0;x<8;++x) {
   block->m_Cb[y][x]=sample(1,bx*8+x,by*8+y);
   block->m_Cr[y][x]=sample(2,bx*8+x,by*8+y);
  }
 }
 FlushCache(0);
 for(h=16;h>=8;h-=8)for(field=0;field<(h==8?2:1);++field)
  for(dst=0;dst<(h==8?16:8);dst+=8)for(avg=0;avg<2;++avg)
   for(dy=VECTOR_MIN;dy<=17;++dy)for(dx=VECTOR_MIN;dx<=17;++dx)
    if(!check(dx,dy,h,field,dst,avg))goto done;
done:
 printf("[sms-motion] exit=%d cases=%u pixels=%u failures=%u\n",failures?1:0,cases,pixels,failures);
 fflush(stdout);SleepThread();return 0;
}
