"""Actual MPEG4 predictor allocation and codec/player init failure branches.

The table warm-up, shared MPEG context and PS2 register/VU writes are stubs.
Predictor/codec-specific allocations, aliases, reference-counted destructors
and the player's Init-result check come from the production C functions.
"""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]


def function(path, name):
    source = (root / path).read_text()
    marker = '}  /* end ' + name + ' */'
    end = source.index(marker) + len(marker)
    starts = list(re.finditer(r'^.*\b' + name + r'\s*\([^;]*?\)\s*\{', source[:end], re.M))
    return source[starts[-1].start():end]


harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef struct SMS_CodecContext SMS_CodecContext;
typedef struct {int (*Init)(SMS_CodecContext *);} SMS_Codec;
struct SMS_CodecContext {
 int m_FrameRate,m_FrameRateBase,m_Width,m_Height,m_HardwareError,m_UserDataLen;
 void *m_pUserData;
 void (*HWCtl)(SMS_CodecContext *,int);
 SMS_Codec *m_pCodec;
};
static struct {
 int m_B8Stride,m_MBH,m_MBStride,m_H263Pred,m_MSPerFrame,m_BitCtx;
 int16_t (*m_pACValBase)[16],(*m_pACVal[3])[16];
 int16_t *m_pDCValBase,*m_pDCVal[3];
 void (*DCT_UnquantizeIntra)(void),(*DCT_UnquantizeInter)(void),(*DecodeMB)(void);
 const uint8_t *m_pChromaQScaleTbl,*m_pY_DCScaleTbl,*m_pC_DCScaleTbl;
 uint8_t *m_pCBPTbl,*m_pPredDirTbl,*m_pCodedBlockBase,*m_pCodedBlock;
} g_MPEGCtx;
#define BASECTX() g_MPEGCtx
static const uint8_t g_chroma_qscale_table[32],s_y_dc_scale_table[32],s_c_dc_scale_table[32];
static void SMS_H263_DCTUnquantizeIntra(void){}
static void SMS_H263_DCTUnquantizeInter(void){}
static void _msmpeg4_decode_mb(void){}
static void MPEG4_CommonHWCtl(SMS_CodecContext *p,int c){p->m_HardwareError=0;}
static int fail_at,allocations,live,hardware_writes,context_destroys,headers,table_frees;
static void *shared_context;
static int mpeg4_refs,msmpeg4_refs;
static void *allocate(size_t n,size_t size) {
 if(++allocations==fail_at)return NULL;
 void *p=calloc(n,size);assert(p);++live;return p;
}
static void release(void *p){if(p){--live;free(p);}}
#define calloc allocate
#define free release
#define SMS_VLC_Free(...) (++table_frees)
#define SMS_RL_Free(...) (++table_frees)
#define SMS_VLC_RL_Free(...) (++table_frees)
#define SMS_MPEG_InitScanTable(...) ((void)0)
#define SMS_InitGetBits(...) ((void)0)
#define NB_RL_TABLES 6
static int Codec_MPEG4_DecodeHeader(void){++headers;return -1;}
static void SMS_MPEGContext_Init(int width,int height) {
 assert(!shared_context);shared_context=malloc(32);assert(shared_context);++live;
 memset(&g_MPEGCtx,0,sizeof(g_MPEGCtx));
 g_MPEGCtx.m_MBH=(height+15)/16;g_MPEGCtx.m_MBStride=(width+15)/16+1;
 g_MPEGCtx.m_B8Stride=((width+15)/16)*2+1;
}
static void SMS_MPEGContext_Destroy(void){assert(shared_context);free(shared_context);shared_context=NULL;++context_destroys;}
static void MPEG4_CommonDestroy(void);
static void MPEG4_Destroy(SMS_CodecContext *);
static void MSMPEG4_Destroy(SMS_CodecContext *);
'''
common = function('src/SMS_MPEG4.c', 'MPEG4_CommonInit')
common = re.sub(r' memcpy \(  \( void\* \)0x1100[04]000,.*?;', ' ++hardware_writes;', common)
common = re.sub(r' __asm__ __volatile__\(.*?\);', ' ++hardware_writes;', common, flags=re.S)
harness += common
harness += function('src/SMS_MPEG4.c', 'MPEG4_CommonDestroy')
for path, name, refs in (
    ('src/SMS_MPEG4.c', 'MPEG4', 'mpeg4_refs'),
    ('src/SMS_MSMPEG4.c', 'MSMPEG4', 'msmpeg4_refs'),
):
    init = function(path, name + '_Init')
    # Retain the actual init tail starting at shared-context/predictor creation.
    # The preceding static VLC/code-table warm-up has no role in these faults.
    start = init.index('  SMS_MPEGContext_Init (')
    init = 'static int ' + name + '_Init(SMS_CodecContext *apCtx) {\n if (!s_Init) {\n int i;\n' + init[start:]
    harness += init.replace('s_Init', refs)
    harness += function(path, name + '_Destroy').replace('s_Init', refs)

player_source = (root / 'src/SMS_Player.c').read_text()
start = player_source.index('        s_Player.m_pVideoCodec = lpVideoCtx -> m_pCodec;')
end = player_source.index('        s_Player.m_VideoIdx = i;', start) + len('        s_Player.m_VideoIdx = i;')
cleanup_start = player_source.index('\nerror:\n', player_source.index('SMS_Player* SMS_InitPlayer')) + len('\nerror:\n')
cleanup_end = player_source.index('  if (  ( int )apFileCtx', cleanup_start)
harness += r'''
static int STR_OUT_OF_MEMORY,player_destroyed,hardware_started;
static void *s_pAPBuff,*s_pVPBuff,*audio_packet_ring;
static struct{SMS_Codec *m_pVideoCodec;void *m_pErrorMsg;int m_VideoIdx;}s_Player;
static void failed_player_storage(void) {
 assert(!audio_packet_ring);
''' + player_source[cleanup_start:cleanup_end] + r'''
}
static void reserve_audio_first(void) {
 assert(!s_pAPBuff&&!s_pVPBuff&&!audio_packet_ring);
 /* These allocations precede the injected video Init failure. */
 s_pAPBuff=malloc(128);audio_packet_ring=malloc(32);
 assert(s_pAPBuff&&audio_packet_ring);live+=2;
}
static int player_init(SMS_CodecContext *lpVideoCtx) {
 int i=7;
''' + player_source[start:end] + r'''
 ++hardware_started;
 lpVideoCtx->HWCtl(lpVideoCtx,0);
 return 1;
destroy:
 ++player_destroyed;
 /* The real container destroys the earlier audio stream's packet ring. */
 free(audio_packet_ring);audio_packet_ring=NULL;
 failed_player_storage();
 return 0;
}
static void assert_empty(void) {
 assert(!live&&!shared_context&&!mpeg4_refs&&!msmpeg4_refs);
 assert(!s_pAPBuff&&!s_pVPBuff&&!audio_packet_ring);
 assert(!g_MPEGCtx.m_pACValBase&&!g_MPEGCtx.m_pDCValBase);
 assert(!g_MPEGCtx.m_pCBPTbl&&!g_MPEGCtx.m_pPredDirTbl&&!g_MPEGCtx.m_pCodedBlockBase&&!g_MPEGCtx.m_pCodedBlock);
 for(int i=0;i<3;++i)assert(!g_MPEGCtx.m_pACVal[i]&&!g_MPEGCtx.m_pDCVal[i]);
}
static void reset(int fail) {
 assert_empty();memset(&g_MPEGCtx,0,sizeof(g_MPEGCtx));
 fail_at=fail;allocations=hardware_writes=context_destroys=headers=table_frees=0;
 player_destroyed=hardware_started=0;memset(&s_Player,0,sizeof(s_Player));s_Player.m_VideoIdx=-1;
}
int main(void) {
 for(int fail=1;fail<=2;++fail) {
  reset(fail);SMS_CodecContext c={25,1,640,480,0,0,NULL,NULL,NULL};
  g_MPEGCtx.m_MBH=30;g_MPEGCtx.m_MBStride=41;g_MPEGCtx.m_B8Stride=81;
  assert(!MPEG4_CommonInit(&c)&&c.m_HardwareError&&!hardware_writes);
  assert(allocations==fail);assert_empty();MPEG4_CommonDestroy();assert_empty();
  fail_at=allocations=0;assert(MPEG4_CommonInit(&c)&&!c.m_HardwareError&&live==2);
  int n=81*61+2*41*31;for(int i=0;i<n;++i)assert(g_MPEGCtx.m_pDCValBase[i]==1024);
  assert(g_MPEGCtx.m_pACVal[0]==g_MPEGCtx.m_pACValBase+82);
  assert(g_MPEGCtx.m_pDCVal[0]==g_MPEGCtx.m_pDCValBase+82);
  MPEG4_CommonDestroy();MPEG4_CommonDestroy();assert_empty();
 }
 for(int codec=0;codec<2;++codec) for(int fail=1;fail<=(codec?3:4);++fail) {
  reset(fail);SMS_Codec selected={codec?MSMPEG4_Init:MPEG4_Init};
  unsigned char extra=0;SMS_CodecContext c={25,1,640,480,0,1,&extra,NULL,&selected};
  for(int attempt=1;attempt<=3;++attempt) {
   reserve_audio_first();allocations=0;
   assert(!player_init(&c)&&c.m_HardwareError&&player_destroyed==attempt&&!hardware_started);
   assert(s_Player.m_pErrorMsg==&STR_OUT_OF_MEMORY&&s_Player.m_VideoIdx==-1);
   assert(context_destroys==attempt&&!headers&&table_frees>0);assert_empty();
   failed_player_storage();assert_empty();
  }
  if(codec)MSMPEG4_Destroy(&c);else MPEG4_Destroy(&c);assert_empty();
  fail_at=allocations=0;assert(player_init(&c)&&!c.m_HardwareError&&hardware_started==1);
  assert(s_Player.m_VideoIdx==7&&headers==!codec);
  /* Successful Init retains the existing per-codec reference contract. */
  assert(selected.Init(&c));
  if(codec)MSMPEG4_Destroy(&c);else MPEG4_Destroy(&c);assert(live);
  if(codec)MSMPEG4_Destroy(&c);else MPEG4_Destroy(&c);assert_empty();
 }
 reset(0);s_pVPBuff=malloc(128);assert(s_pVPBuff);++live;
 failed_player_storage();failed_player_storage();assert_empty();
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    c, exe = Path(temp) / 'mpeg4-init.c', Path(temp) / 'mpeg4-init'
    c.write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), '-g', '-no-pie', '-fsanitize=address,undefined',
                    '-fno-sanitize-recover=all', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True, timeout=10)
print('MPEG4/MSMPEG4 init: AC/DC and codec-table failures, aliases/partial cleanup, audio-first player rejection, repeated failure/retry, storage cleanup and shared lifetime passed')
