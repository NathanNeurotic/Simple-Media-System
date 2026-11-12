/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2006-2007 Eugene Plotnikov <e-plotnikov@operamail.com>
# (c) 2007      Petr Otoupal (HDTV support)
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#include "SMS.h"
#include "SMS_GS.h"
#include "SMS_DMA.h"
#include "SMS_VIF.h"
#include "SMS_INTC.h"
#include "SMS_Config.h"
#include "SMS_Locale.h"

#include <kernel.h>
#include <malloc.h>

extern int g_XShift;

GSContext g_GSCtx = {
 .m_Width    = 640U,
 .m_Height   = 480U,
 .m_PWidth   = 640U,
 .m_OffsetX  =    0,
 .m_OffsetY  =    0,
 .m_BkColor  =    0UL,
 .m_CodePage = GSCodePage_WinLatin1
};

static GSLoadImage  s_CLUTLoadImage __attribute__(  (  aligned( 64 )  )   );
static unsigned int s_CLUT[ 16 ]    __attribute__(  (  aligned( 64 )  )   );

/* 720p draws a 1216x676 canvas into a 1280x720 raster, so 64 columns and 44 rows of the active
 * area are never painted. GS_InitDC anchors the canvas at the START of active video ( its 720p
 * arm adds 302 to DX and 24 to DY, SMS_GS_0.S ), which piles that entire shortfall onto the
 * right edge and the bottom -- the asymmetric black bars in the 720p report. Splitting it
 * centres the canvas so the unpainted margin is even on all four sides, which reads as a border
 * rather than as a broken frame, and most sets overscan it away.
 * This does NOT eliminate the margin. Painting the full 1280x720 would, but the canvas size is
 * a VRAM compromise: at 720p the framebuffer already takes ~3.3 MB of 4 MB, and growing it eats
 * the headroom the font glyphs and icons live in -- which is exactly the collision that caused
 * the 1080i picket-fence text. Not worth risking for a border.
 * Derived from the live canvas rather than hardcoded, because m_DispWH is user-editable per
 * mode from the Display menu, so a fixed +32 / +22 would be wrong the moment anyone resizes it.
 * Never shifts left or up -- a canvas at or beyond the raster gets no adjustment at all. The
 * user's own m_OffsetX / m_OffsetY still apply on top, so "Adjust image" behaves as before, and
 * since this is derived at init rather than stored, nothing is written into their config.
 * SD modes take the aMode test and come out byte-identical.
 * 1080i has the same shortfall ( 1820x1018 in 1920x1080, anchored by DX 238 / DY 40 ): a 100
 * column band on the right and 62 rows at the bottom. The player draws it at half width and
 * the GS doubles it, so the width it covers is taken from GS_InitDC's DW ( w << ( 1920 / w - 1 ) ),
 * and the vertical pad is kept even like every other DY change here, so the fields keep their order. */
void GS_DisplayOffsets ( GSVideoMode aMode, int* apDX, int* apDY ) {

 int lPadX = 0;
 int lPadY = 0;

 *apDX = g_GSCtx.m_OffsetX;
 *apDY = g_GSCtx.m_OffsetY;

 if ( aMode == GSVideoMode_DTV_1280x720P ) {

  lPadX = (  1280 - ( int )g_GSCtx.m_PWidth   ) >> 1;
  lPadY = (   720 - ( int )g_GSCtx.m_PHeight  ) >> 1;

 } else if ( aMode == GSVideoMode_DTV_1920x1080I ) {

  int lW = g_GSCtx.m_PWidth;

  lPadX =    (  1920 - ( lW << ( 1920 / lW - 1 ) )  ) >> 1;
  lPadY = (  (  1080 - ( int )g_GSCtx.m_PHeight  ) >> 1  ) & ~1;

 }  /* end if */

 if ( lPadX > 0 ) *apDX += lPadX;
 if ( lPadY > 0 ) *apDY += lPadY;

}  /* end GS_DisplayOffsets */

void GSContext_Init ( GSVideoMode aMode, GSZTest aZTest, GSDoubleBuffer aDblBuf ) {

 unsigned int lSize;
 int          lPixSize;
 int          lColorDepth;
 int          lDX, lDY;
 int          lf16  = g_Config.m_ColorDepth;
 GSParams*    lpPar = GS_Params ();

 lpPar -> m_PARNTSC = g_Config.m_PAR[ 0 ];
 lpPar -> m_PARPAL  = g_Config.m_PAR[ 1 ];

 if ( aMode == GSVideoMode_Default ) aMode = g_pBXDATASYS[ 6 ] == 'E' ? GSVideoMode_PAL : GSVideoMode_NTSC;

 lSize = GS_VMode2Index ( aMode );

 g_GSCtx.m_PWidth     = g_Config.m_DispWH[ lSize ][ 0 ];
 g_GSCtx.m_Width      = g_Config.m_DispWH[ lSize ][ 0 ];
 g_GSCtx.m_PHeight    = g_Config.m_DispWH[ lSize ][ 1 ];
 g_GSCtx.m_Height     = g_Config.m_DispWH[ lSize ][ 1 ];
 g_GSCtx.m_DrawDelay  = g_Config.m_SyncPar[ lSize ][ aDblBuf ];
 g_GSCtx.m_DrawDelay2 = g_Config.m_SyncPar[ lSize ][ 2 ];

 if ( aMode == GSVideoMode_NTSC || aMode == GSVideoMode_PAL )
  g_GSCtx.m_Height = 480;
 else if ( aMode == GSVideoMode_DTV_1920x1080I ) {
  lf16             = aDblBuf;
  g_GSCtx.m_PWidth = g_GSCtx.m_PWidth >> !aDblBuf;
 }  /* end if */

 if ( lf16 ) {
  lColorDepth = GSPixelFormat_PSMCT16;
  lPixSize    = 2;
 } else {
  lColorDepth = GSPixelFormat_PSMCT24;
  lPixSize    = 3;
 }  /* end if */

 g_GSCtx.m_OffsetY -= g_GSCtx.m_OffsetY & 1;

 GS_DisplayOffsets ( aMode, &lDX, &lDY );

 GS_Reset ( GSInterlaceMode_On, aMode, GSFieldMode_Field );
 GS_InitDC ( &g_GSCtx.m_DispCtx, lColorDepth, g_GSCtx.m_PWidth, g_GSCtx.m_PHeight, lDX, lDY );

 GIF_MODE() = 0;

 lSize = g_GSCtx.m_PWidth;

 while ( 1 ) {
  int lVal = lSize * lPixSize;
  if (  !( lVal & 15 ) && !( lVal % lPixSize )  ) break;
  ++lSize;
 }  /* end while */

 g_GSCtx.m_LWidth  = lSize;
 g_GSCtx.m_PixSize = lPixSize;

 lSize = GS_InitGC ( 0, &g_GSCtx.m_DrawCtx[ 0 ], lColorDepth, g_GSCtx.m_PWidth, g_GSCtx.m_PHeight, aZTest );
         GS_InitGC ( 1, &g_GSCtx.m_DrawCtx[ 1 ], lColorDepth, g_GSCtx.m_PWidth, g_GSCtx.m_PHeight, aZTest );
 GS_InitClear ( &g_GSCtx.m_ClearPkt, 0, 0, g_GSCtx.m_Width << 1, g_GSCtx.m_Height << 1, g_GSCtx.m_BkColor, aZTest );

 SyncDCache (   &g_GSCtx, (  ( unsigned char* )&g_GSCtx  ) + sizeof ( GSContext )   );

 g_GSCtx.m_VRAMPtr = g_GSCtx.m_VRAMPtr2 = g_GSCtx.m_DrawCtx[ 0 ].m_ZBUFVal.ZBP;

 if ( g_GSCtx.m_pDBuf ) {
  free ( g_GSCtx.m_pDBuf );
  g_GSCtx.m_pDBuf = NULL;
 }  /* end if */

 GS_SetDC (  &g_GSCtx.m_DispCtx, ( aMode <= GSVideoMode_PAL ) || ( aMode == GSVideoMode_DTV_1920x1080I )  );
 GS_SetGC ( &g_GSCtx.m_DrawCtx[ 0 ] );
 GS_SetGC ( &g_GSCtx.m_DrawCtx[ 1 ] );
 GS_Clear ( &g_GSCtx.m_ClearPkt     );

 if ( aZTest == GSZTest_On )

  g_GSCtx.m_VRAMPtr += lSize;

 else if ( aDblBuf && !g_GSCtx.m_pDBuf ) {

  g_GSCtx.m_pDBuf = ( unsigned char* )memalign (
   64, lSize = (  ( g_GSCtx.m_LWidth * g_GSCtx.m_PHeight * lPixSize + 63 ) & ~63  )
  );
  InvalidDCache ( g_GSCtx.m_pDBuf, g_GSCtx.m_pDBuf + lSize );

 }  /* end else */

 g_GSCtx.m_VRAMPtr <<= 5;

 GS_InitLoadImage (  UNCACHED_SEG( &s_CLUTLoadImage ), 0, 1, GSPixelFormat_PSMCT32, 0, 0, 8, 2  );

 for ( lSize = 0; lSize < 4; ++lSize ) GSContext_SetTextColor ( lSize, 0x0000000080FFFFFFL );

 if ( lColorDepth == GSPixelFormat_PSMCT16 )
  g_GSCtx.m_VRAMFontPtr = g_GSCtx.m_VRAMPtr;
 else g_GSCtx.m_VRAMFontPtr = 0;

 GSFont_Init ();

 if ( g_GSCtx.m_pDisplayList[ 0 ] ) free ( g_GSCtx.m_pDisplayList[ 0 ] );
 if ( g_GSCtx.m_pDisplayList[ 1 ] ) free ( g_GSCtx.m_pDisplayList[ 1 ] );

 g_GSCtx.m_pDisplayList[ 0 ] = NULL;
 g_GSCtx.m_pDisplayList[ 1 ] = NULL;
 g_GSCtx.m_nAlloc      [ 0 ] = 0;
 g_GSCtx.m_nAlloc      [ 1 ] = 0;
 g_GSCtx.m_PutIndex    [ 0 ] = 0;
 g_GSCtx.m_PutIndex    [ 1 ] = 0;

}  /* end GSContext_Init */

u64*           GSContext_NewPacket ( int aCtx, int aDWC, GSPaintMethod aMethod ) {

 int            lPutIdx = g_GSCtx.m_PutIndex[ aCtx ];
 unsigned int   lQWC    = ( aDWC + 1 ) >> 1;
 u64*           lpList  = g_GSCtx.m_pDisplayList[ aCtx ];
 unsigned int   lOldAllocSize;

 DMA_Wait ( DMAC_VIF1 );

 lOldAllocSize = (  g_GSCtx.m_nAlloc[ aCtx ] * sizeof ( u64           ) + 63  ) & ~63;

 if ( aDWC > g_GSCtx.m_nAlloc[ aCtx ] - lPutIdx - 8 )

  g_GSCtx.m_nAlloc[ aCtx ] = aDWC + g_GSCtx.m_nAlloc[ aCtx ] + 512;

 g_GSCtx.m_pDisplayList[ aCtx ] = lpList = ( u64*           )SMS_ReallocWithAlign(
  lpList, &lOldAllocSize, (  g_GSCtx.m_nAlloc[ aCtx ] * sizeof ( u64           ) + 63  ) & ~63
 );

 if ( aMethod & 0x02 ) {

  lPutIdx = 0;

  if ( aMethod & 0x01 ) {

   lpList[ 0 ] = DMA_TAG( 8, 1, DMATAG_ID_REF, 0, &g_GSCtx.m_ClearPkt, 0 );
   lpList[ 1 ] = 0;
   lPutIdx += 2;

  }  /* end if */

 }  /* end if */

 if ( aDWC ) {

  lpList[ lPutIdx ] = DMA_TAG( lQWC + 1, 1, DMATAG_ID_CNT, 0, 0, 0 );
  g_GSCtx.m_pLastTag[ aCtx ] = &lpList[ lPutIdx++ ];
  lpList[ lPutIdx++ ] = 0;
  lpList[ lPutIdx++ ] = 0;
  lpList[ lPutIdx++ ] = VIF_DIRECT( lQWC );

 }  /* end if */

 g_GSCtx.m_PutIndex[ aCtx ] = lPutIdx + aDWC;

 return lpList + lPutIdx;

}  /* end GSContext_NewPacket */

void GSContext_Flush ( int aCtx, GSFlushMethod aMethod ) {

 u64*           lpList = g_GSCtx.m_pDisplayList[ aCtx ];

 if ( aMethod != GSFlushMethod_DeleteListsOnly ) {

  DMATag* lpTag   = ( DMATag* )g_GSCtx.m_pLastTag[ aCtx ];
  int     lPutIdx = g_GSCtx.m_PutIndex[ aCtx ];

  if ( lpTag -> ID == DMATAG_ID_CNT )
   lpTag -> ID = DMATAG_ID_END;
  else if ( lpTag -> ID == DMATAG_ID_REF )
   lpTag -> ID = DMATAG_ID_REFE;
  else {
   lpList[ lPutIdx++ ] = DMA_TAG( 0, 0, DMATAG_ID_END, 0, 0, 0 );
   lpList[ lPutIdx   ] = 0;
  }  /* end else */

  SyncDCache ( lpList, lpList + lPutIdx );

  DMA_SendChainT ( DMAC_VIF1, lpList );

  g_GSCtx.m_PutIndex[ aCtx ] = 0;

 }  /* end if */

 if ( aMethod > GSFlushMethod_KeepLists ) {

  DMA_Wait ( DMAC_VIF1 );
  free ( lpList );

  g_GSCtx.m_pDisplayList[ aCtx ] = NULL;
  g_GSCtx.m_nAlloc[ aCtx ]       = 0;

 }  /* end if */

}  /* end GSContext_Flush */

u64*           GSContext_NewList ( unsigned int aDWC ) {

 unsigned int   lQWC   = ( aDWC + 1 ) >> 1;
 u64*           retVal = ( u64*           )memalign (
  64, (  ( aDWC + 2 ) * sizeof ( u64           ) + 63  ) & ~63
 );

 retVal[ 0 ] = 0;
 retVal[ 1 ] = VIF_DIRECT( lQWC );

 return retVal + 2;

}  /* end GSContext_NewList */

void GSContext_DeleteList ( u64*           apList ) {

 if ( apList ) free ( apList - 2 );

}  /* end GSContext_DeleteList */

void GSContext_CallList ( int aCtx, u64*           apList ) {

 unsigned int   lQWC    = ( apList[ -1 ] >> 32 ) & 0xFFFF;
 unsigned int   lPutIdx = g_GSCtx.m_PutIndex[ aCtx ];
 u64*           lpList  = g_GSCtx.m_pDisplayList[ aCtx ];
 unsigned int   lOldAllocSize;

 if ( 2 > g_GSCtx.m_nAlloc[ aCtx ] - g_GSCtx.m_PutIndex[ aCtx ] ) {
  lOldAllocSize = ( g_GSCtx.m_nAlloc[ aCtx ] ) * sizeof ( u64           );
  g_GSCtx.m_nAlloc[ aCtx ] = 2 + g_GSCtx.m_nAlloc[ aCtx ] + 512;
  g_GSCtx.m_pDisplayList[ aCtx ] = lpList = ( u64*           )SMS_ReallocWithAlign (
   lpList, &lOldAllocSize, ( g_GSCtx.m_nAlloc[ aCtx ] ) * sizeof ( u64           )
  );
 }

 g_GSCtx.m_pLastTag[ aCtx ] = &lpList[ lPutIdx ];
 lpList[ lPutIdx++ ] = DMA_TAG(  lQWC + 1, 1, DMATAG_ID_REF, 0, ( unsigned int )( apList - 2 ), 0  );
 lpList[ lPutIdx++ ] = 0;
 g_GSCtx.m_PutIndex[ aCtx ] = lPutIdx;

 SyncDCache (  apList - 2, apList + ( lQWC << 1 )  );

}  /* end GSContext_CallList */

void GSContext_CallList2 ( int aCtx, u64*           apList ) {

 unsigned int   lPutIdx = g_GSCtx.m_PutIndex    [ aCtx ];
 u64*           lpList  = g_GSCtx.m_pDisplayList[ aCtx ];
 unsigned int   lOldAllocSize;

 if ( 2 > g_GSCtx.m_nAlloc[ aCtx ] - g_GSCtx.m_PutIndex[ aCtx ] ) {
  lOldAllocSize = ( g_GSCtx.m_nAlloc[ aCtx ] ) * sizeof ( u64           );
  g_GSCtx.m_nAlloc[ aCtx ] = 2 + g_GSCtx.m_nAlloc[ aCtx ] + 512;
  g_GSCtx.m_pDisplayList[ aCtx ] = lpList = ( u64*           )SMS_ReallocWithAlign (
   lpList, &lOldAllocSize, ( g_GSCtx.m_nAlloc[ aCtx ] ) * sizeof ( u64           )
  );
 }

 g_GSCtx.m_pLastTag[ aCtx ] = &lpList[ lPutIdx ];
 lpList[ lPutIdx++ ] = DMA_TAG(  0, 0, DMATAG_ID_CALL, 0, ( unsigned int )apList, 0  );
 lpList[ lPutIdx++ ] = 0;
 g_GSCtx.m_PutIndex[ aCtx ] = lPutIdx;

}  /* end GSContext_CallList2 */

void GSContext_SetTextColor ( unsigned int aColorIdx, u64           aColor ) {

 u64           lDMA[ 8 ] __attribute__(   (  aligned( 64 )  )  );
 unsigned int* lpCLUT    = UNCACHED_SEG(  s_CLUT          );
 GSLoadImage*  lpLoadImg = UNCACHED_SEG( &s_CLUTLoadImage );

 lpCLUT[ 0 ] = 0;
 lpCLUT[ 1 ] = GS_SET_RGBAQ( 0, 0, 0, aColor >> 25, 0 );
 lpCLUT[ 2 ] = aColor;
 lpCLUT[ 3 ] = 0;
 lpCLUT[ 4 ] = aColor;
 lpCLUT[ 5 ] = GS_SET_RGBAQ( 0, 0, 0, aColor >> 25, 0 );

 lpLoadImg -> m_BitBltBufReg.DBP = 0x3FC0;

 GS_LoadImage ( &s_CLUTLoadImage, s_CLUT );

 lDMA[ 0 ] = GIF_TAG( 2, 1, 0, 0, 0, 1 );
 lDMA[ 1 ] = GIFTAG_REGS_AD;
 lDMA[ 2 ] = 0;
 lDMA[ 3 ] = GS_TEXFLUSH;
 lDMA[ 4 ] = GS_SET_TEX0( g_GSCtx.m_VRAMFontPtr, 8, g_GSCtx.m_FontTexFmt, 9, 9, GS_TEX_TCC_RGBA, GS_TEX_TFX_DECAL, 0x3FC0, GSPixelFormat_PSMCT32, GS_TEX_CSM_CSM1, aColorIdx, GS_TEX_CLD_LOAD );
 lDMA[ 5 ] = GS_TEX0_1;
 SyncDCache ( lDMA, lDMA + 6 );
 DMA_Send ( DMAC_GIF, lDMA, 3 );
 DMA_Wait ( DMAC_GIF );

}  /* end GSContext_SetTextColor */

void GSContext_RenderTexSprite ( GSTexSpritePacket* apPkt, int aX, int anY, int aWidth, int aHeight, int aTX, int aTY, int aTW, int aTH ) {

 u64           lXYZ0;
 u64           lXYZ1;

 __asm__ __volatile__ (
  ".set noreorder\n\t"
  ".set noat\n\t"
  "pcpyld   $ra, $ra, $ra\n\t"
  "addu     $v0, %3, %5\n\t"
  "move     $a0, %2\n\t"
  "dsll32   $v0, $v0, 0\n\t"
  "move     $a1, %3\n\t"
  "move     $a2, $zero\n\t"
  "jal      GS_XYZ\n\t"
  "or       $a1, $a1, $v0\n\t"
  "lw       $at, g_XShift\n\t"
  "dsrl32   $a1, $a1, 0\n\t"
  "srav     %2, %2, $at\n\t"
  "srav     %4, %4, $at\n\t"
  "addu     $v1, %2, %4\n\t"
  "move     %0, $v0\n\t"
  "sll      $v1, $v1, 4\n\t"
  "or       %1, $v1, $a1\n\t"
  "pcpyud   $ra, $ra, $ra\n\t"
  ".set at\n\t"
  ".set reorder\n\t"
  : "=r"( lXYZ0 ), "=r"( lXYZ1 ) : "r"( aX ), "r"( anY ), "r"( aWidth ), "r"( aHeight ) : "a0", "a1", "a2", "v0", "v1"
 );

 apPkt -> m_VIFCodes[ 0 ]       = 0;
 apPkt -> m_VIFCodes[ 1 ]       = VIF_DIRECT( 7 );
 apPkt -> m_Tag0.m_HiLo.m_Lo    = GIF_TAG( 1, 0, 0, 0, 0, 1 );
 apPkt -> m_Tag0.m_HiLo.m_Hi    = GIFTAG_REGS_AD;
 apPkt -> m_TEXFLUSHVal.m_Value = 0;
 apPkt -> m_TEXFLUSHTag         = GS_TEXFLUSH;
 apPkt -> m_Tag1.m_HiLo.m_Lo    = GIF_TAG( 1, 0, 0, 0, 1, 2 );
 apPkt -> m_Tag1.m_HiLo.m_Hi    = GS_TEX0_1 | ( GS_PRIM << 4 );
 apPkt -> m_TEX0Val.m_Value     = GS_SET_TEX0( g_GSCtx.m_VRAMTexPtr, g_GSCtx.m_TBW, GSPixelFormat_PSMCT32, g_GSCtx.m_TW, g_GSCtx.m_TH, GS_TEX_TCC_RGBA, GS_TEX_TFX_DECAL, 0, 0, 0, 0, 0 );
 apPkt -> m_PRIMVal.m_Value     = GS_SET_PRIM( GS_PRIM_PRIM_SPRITE, GS_PRIM_IIP_FLAT, GS_PRIM_TME_ON, GS_PRIM_FGE_OFF, GS_PRIM_ABE_ON, GS_PRIM_AA1_OFF, GS_PRIM_FST_UV, GS_PRIM_CTXT_1, GS_PRIM_FIX_UNFIXED );
 apPkt -> m_Tag2.m_HiLo.m_Lo    = GIF_TAG( 2, 1, 0, 0, 1, 2 );
 apPkt -> m_Tag2.m_HiLo.m_Hi    = GS_UV | ( GS_XYZ2 << 4 );
 apPkt -> m_UV0Val.m_Value      = GS_SET_UV( ++aTX << 4, ++aTY << 4 );
 apPkt -> m_XYZ0Val.m_Value     = lXYZ0;
 apPkt -> m_UV1Val.m_Value      = GS_SET_UV(  ( aTX + --aTW ) << 4, ( aTY + --aTH ) << 4  );
 apPkt -> m_XYZ1Val.m_Value     = lXYZ1;

}  /* end GSContext_RenderTexSprite */

void GSContext_RenderVGRect ( u64*           apDMA, int aX, int anY, int aWidth, int aHeight, u64           aClr0, u64           aClr1 ) {

 GSRegXYZ lXYZ0;
 GSRegXYZ lXYZ1;

 __asm__ __volatile__ (
  ".set noreorder\n\t"
  ".set noat\n\t"
  "pcpyld   $ra, $ra, $ra\n\t"
  "addu     $v0, %3, %5\n\t"
  "move     $a0, %2\n\t"
  "dsll32   $v0, $v0, 0\n\t"
  "move     $a1, %3\n\t"
  "move     $a2, $zero\n\t"
  "jal      GS_XYZ\n\t"
  "or       $a1, $a1, $v0\n\t"
  "lw       $at, g_XShift\n\t"
  "dsrl32   $a1, $a1, 0\n\t"
  "srav     %2, %2, $at\n\t"
  "srav     %4, %4, $at\n\t"
  "addu     $v1, %2, %4\n\t"
  "move     %0, $v0\n\t"
  "sll      $v1, $v1, 4\n\t"
  "or       %1, $v1, $a1\n\t"
  "pcpyud   $ra, $ra, $ra\n\t"
  ".set at\n\t"
  ".set reorder\n\t"
  : "=r"( lXYZ0.m_Value ), "=r"( lXYZ1.m_Value ) : "r"( aX ), "r"( anY ), "r"( aWidth ), "r"( aHeight ) : "a0", "a1", "a2", "v0", "v1"
 );

 apDMA[ 0 ] = GIF_TAG( 1, 1, 0, 0, 1, 7 );
 apDMA[ 1 ] = ( u64 )GS_PRIM | (  ( u64 )GS_RGBAQ <<  4  ) | (  ( u64 )GS_XYZ2  <<  8  )
                             | (  ( u64 )GS_XYZ2  << 12  ) | (  ( u64 )GS_RGBAQ << 16  )
                             | (  ( u64 )GS_XYZ2  << 20  ) | (  ( u64 )GS_XYZ2  << 24  )
                             | (  ( u64 )GS_NOP   << 28  );
 apDMA[ 2 ] = GS_SET_PRIM( GS_PRIM_PRIM_TRISTRIP, 1, 0, 0, 1, 0, 0, 0, 0 );
 apDMA[ 3 ] = aClr0;
 apDMA[ 4 ] = lXYZ0.m_Value;
 apDMA[ 5 ] = GS_SET_XYZ( lXYZ1.X, lXYZ0.Y, 0 );
 apDMA[ 6 ] = aClr1;
 apDMA[ 7 ] = GS_SET_XYZ( lXYZ0.X, lXYZ1.Y, 0 );
 apDMA[ 8 ] = lXYZ1.m_Value;
 apDMA[ 9 ] = 0;

}  /* end GSContext_RenderVGRect */

void GS_InitStoreImage ( GSStoreImage* apPkt, unsigned int aSrc, int aX, int anY, int aWidth, int aHeight ) {

 GSPixelFormat lPSM = g_GSCtx.m_DrawCtx[ 0 ].m_FRAMEVal.PSM;
 unsigned int  lFBW = g_GSCtx.m_DrawCtx[ 0 ].m_FRAMEVal.FBW;

 apPkt -> m_VIFCodes[ 0 ]        = 0x0000000006008000L;
 apPkt -> m_VIFCodes[ 1 ]        = 0x5000000613000000L;
 apPkt -> m_Tag.m_HiLo.m_Lo      = GIF_TAG( 5, 1, 0, 0, 0, 1 );
 apPkt -> m_Tag.m_HiLo.m_Hi      = GIFTAG_REGS_AD;
 apPkt -> m_BITBLTBUFVal.m_Value = GS_SET_BITBLTBUF ( aSrc << 5, lFBW, lPSM, 0, 0, 0 );
 apPkt -> m_BITBLTBUFTag         = GS_BITBLTBUF;
 apPkt -> m_TRXREGVal.m_Value    = GS_SET_TRXREG( aWidth, aHeight );
 apPkt -> m_TRXREGTag            = GS_TRXREG;
 apPkt -> m_TRXPOSVal.m_Value    = GS_SET_TRXPOS( aX, anY, 0, 0, GS_TRXPOS_DIR_LR_UD );
 apPkt -> m_TRXPOSTag            = GS_TRXPOS;
 apPkt -> m_FINISHVal.m_Value    = GS_SET_FINISH( 0 );
 apPkt -> m_FINISHTag            = GS_FINISH;
 apPkt -> m_TRXDIRVal.m_Value    = GS_SET_TRXDIR( GS_TRXDIR_LOCAL_TO_HOST );
 apPkt -> m_TRXDIRTag            = GS_TRXDIR;

}  /* end GS_InitStoreImage */

void GS_StoreImage ( GSStoreImage* apPkt, void* apDst ) {

 int  lW, lH, lHH, lSize;
 int  lRemB, lRemQ, lDMAQWC, lRSize;
 int  lnDMA, lDMARem;
 u128 lTmp;

 volatile unsigned int*  lpVIFStat  = ( volatile unsigned int*  )0x10003C00;
 volatile u64*           lpGSBusDir = ( volatile u64*           )0x12001040;
 volatile u128*          lpVIFIFO   = ( volatile u128*          )0x10005000;

 lW = apPkt -> m_TRXREGVal.RRW;
 lH = apPkt -> m_TRXREGVal.RRH;

 switch ( apPkt -> m_BITBLTBUFVal.SPSM ) {

  case GSPixelFormat_PSMCT32:

   lSize   = ( lW * lH ) << 2;
   lRemB   = lSize & 15;
   lRemQ   = ( lSize >> 4 ) &  7;
   lDMAQWC = ( lSize >> 4 ) & ~7;

   if ( !lRemB ) {
    lRSize = 0;
    lHH    = lH;
   } else {
    lHH      = ( lH + 3 ) & ~3;
    lRSize   = (  ( lW * lHH ) >> 2  ) - lDMAQWC - lRemQ - 1;
   }  /* end else */

  break;

  case GSPixelFormat_PSMCT24:

   lSize   = lW * lH * 3;
   lRemB   = lSize & 15;
   lRemQ   = ( lSize >> 4 ) &  7;
   lDMAQWC = ( lSize >> 4 ) & ~7;

   if ( !lRemB ) {
    lRSize = 0;
    lHH    = lH;
   } else {
    lHH    = ( lH + 15 ) & ~15;
    lRSize = (  ( lW * lHH * 3 ) >> 4  ) - lDMAQWC - lRemQ - 1;
   }  /* end else */

  break;

  case GSPixelFormat_PSMCT16:

   lSize   = ( lW * lH ) << 1;
   lRemB   = lSize & 15;
   lRemQ   = ( lSize >> 4 ) &  7;
   lDMAQWC = ( lSize >> 4 ) & ~7;

   if ( !lRemB ){
    lRSize = 0;
    lHH    = lH;
   } else {
    lHH    = ( lH + 7 ) & ~7;
    lRSize = (  ( lW * lHH ) >> 3  ) - lDMAQWC - lRemQ - 1;
   }  /* end else */

  break;

  default: return;

 }  /* end switch */

 if ( lRemB ) apPkt -> m_TRXREGVal.m_Value = GS_SET_TRXREG( lW, lHH );

 lnDMA   = lDMAQWC / 0xFFF8;
 lDMARem = lDMAQWC % 0xFFF8;

 *GS_CSR = 2;

 SyncDCache ( apPkt, apPkt + 1 );

 DMA_Send ( DMAC_VIF1, apPkt, 7 );

 while (  !( *GS_CSR & 2 )  );

 lpVIFStat [ 0 ] = 0x00800000;
 lpGSBusDir[ 0 ] = 0x00000001;

 while ( lnDMA-- ) {
  DMA_RecvA ( DMAC_VIF1, apDst, 0xFFF8 );
  apDst = (  ( u128* )apDst  ) + 0xFFF8;
  DMA_Wait ( DMAC_VIF1 );
 }  /* end while */

 if ( lDMARem ) {
  DMA_RecvA ( DMAC_VIF1, apDst, lDMARem );
  apDst = (  ( u128* )apDst  ) + lDMARem;
  DMA_Wait ( DMAC_VIF1 );
 }  /* end if */

 for ( lW = 0; lW < lRemQ; ++lW ) {
  while (   !( lpVIFStat[ 0 ] & 0x1F000000 )  );
  (  ( u128* )apDst  )[ lW ] = lpVIFIFO[ 0 ];
 }  /* end for */

 apDst = (  ( u128* )apDst  ) + lRemQ;

 if ( lRemB ) {
  while (   !( lpVIFStat[ 0 ] & 0x1F000000 )  );
  lTmp = lpVIFIFO[ 0 ];
  for ( lW = 0; lW < lRemB; ++lW )
   (  ( unsigned char* )apDst  )[ lW ] = (  ( unsigned char* )&lTmp  )[ lW ];
  while ( lRSize-- ) {
   while (   !( lpVIFStat[ 0 ] & 0x1F000000 )  );
   lpVIFIFO[ 0 ];
  }  /* end for */
 }  /* end if */

 lpVIFStat [ 0 ] = 0;
 lpGSBusDir[ 0 ] = 0;
 *GS_CSR         = 2;
 lpVIFIFO  [ 0 ] = 0x06000000;

}  /* end GS_StoreImage */

/* Smooth-corner replacement for the assembly GS_RenderRoundRect ( still present
 * as GS_RenderRoundRect_asm ). Byte-compatible packet: same 34-dword layout, the
 * same VIF DIRECT( 17 ) transfer, the same PRIM flags and vertex counts -- only
 * the vertex DISTRIBUTION changed: corners are now true arcs ( 5 segments each )
 * instead of the old coarse facets, so small radii ( 8 and below ) render as a
 * rectangle with smooth corners rather than an octagon. */
void GS_RenderRoundRect ( GSRoundRectPacket* apPack, int anX, int anY, int aW, int aH, int aRad, s64 aColor ) {

 static const float s_Arc6[ 6 ] = {   /* sin 0,18,36,54,72,90 deg -- outline corner arcs */
  0.000000F, 0.309017F, 0.587785F, 0.809017F, 0.951057F, 1.000000F
 };
 static const float s_Arc7[ 7 ] = {   /* sin 0,15,30,45,60,75,90 deg -- fill row arcs    */
  0.000000F, 0.258819F, 0.500000F, 0.707107F, 0.866025F, 0.965926F, 1.000000F
 };

 u64*  lpDMA = ( u64* )apPack;
 float lX0   = ( float )anX;
 float lY0   = ( float )anY;
 float lX1   = ( float )( anX + aW );
 float lY1   = ( float )( anY + aH );
 int   lIdx  = 8;
 int   i;

 lpDMA[ 0 ] = 0UL;
 lpDMA[ 1 ] = 0x5000001100000000ULL;              /* VIF: NOP, DIRECT( 17 qwords )     */
 lpDMA[ 2 ] = 0x2400000000000001ULL;              /* GIFtag: REGLIST, NLOOP 1, NREG 2  */
 lpDMA[ 3 ] = 0x10UL;                             /* regs: PRIM, RGBAQ                 */
 lpDMA[ 5 ] = ( u64 )aColor;

 if ( aRad < 0 ) {   /* outline: linestrip around the perimeter, 27 verts ( 25 used + 2 pads ) */

  float lR   = ( float )-aRad;
  float lMax = ( float )(  ( aW < aH ? aW : aH ) >> 1  );

  if ( lR > lMax ) lR = lMax;   /* match the fill clamp so outline + fill corners stay aligned */

  lpDMA[ 4 ] = 0x182UL;                           /* LINESTRIP | AA1 | FST             */
  lpDMA[ 6 ] = 27UL | 0x8000UL | ( 1ULL << 58 ) | ( 1ULL << 60 );
  lpDMA[ 7 ] = 5UL;                               /* reg: XYZ2                         */

  for ( i = 0; i < 6; ++i )   /* top-left: ( x0, y0+r ) -> ( x0+r, y0 ) */
   lpDMA[ lIdx++ ] = GS_XYZ (
    ( int )( lX0 + lR - lR * s_Arc6[ 5 - i ] + 0.5F ),
    ( int )( lY0 + lR - lR * s_Arc6[ i     ] + 0.5F ), 0
   );
  for ( i = 0; i < 6; ++i )   /* top-right: ( x1-r, y0 ) -> ( x1, y0+r ) */
   lpDMA[ lIdx++ ] = GS_XYZ (
    ( int )( lX1 - lR + lR * s_Arc6[ i     ] + 0.5F ),
    ( int )( lY0 + lR - lR * s_Arc6[ 5 - i ] + 0.5F ), 0
   );
  for ( i = 0; i < 6; ++i )   /* bottom-right: ( x1, y1-r ) -> ( x1-r, y1 ) */
   lpDMA[ lIdx++ ] = GS_XYZ (
    ( int )( lX1 - lR + lR * s_Arc6[ 5 - i ] + 0.5F ),
    ( int )( lY1 - lR + lR * s_Arc6[ i     ] + 0.5F ), 0
   );
  for ( i = 0; i < 6; ++i )   /* bottom-left: ( x0+r, y1 ) -> ( x0, y1-r ) */
   lpDMA[ lIdx++ ] = GS_XYZ (
    ( int )( lX0 + lR - lR * s_Arc6[ i     ] + 0.5F ),
    ( int )( lY1 - lR + lR * s_Arc6[ 5 - i ] + 0.5F ), 0
   );

  lpDMA[ lIdx ] = lpDMA[ 8 ];                     /* close the loop back to the start  */
  lpDMA[ lIdx + 1 ] = lpDMA[ lIdx ];              /* 2 zero-length pads up to NLOOP 27 */
  lpDMA[ lIdx + 2 ] = lpDMA[ lIdx ];
  lpDMA[ lIdx + 3 ] = lpDMA[ lIdx ];              /* REGLIST qword pad ( GIF skips it ) */

 } else {   /* fill: one vertical tristrip zig-zag, 14 arc-following rows x 2 = 28 verts */

  float lR   = ( float )aRad;
  float lMax = ( float )(  ( aW < aH ? aW : aH ) >> 1  );

  if ( lR > lMax ) lR = lMax;   /* corners <= half the shorter side -> fold-proof for thin rects */

  lpDMA[ 4 ] = 0x144UL;                           /* TRISTRIP | ABE | FST -- type 4, NOT the fan
                                                   * 5 ( 0x145 ): these verts are emitted in strip
                                                   * order with no center pivot, so PRIM=fan drew
                                                   * diagonal wedge triangles + gaps ( the "triangular
                                                   * glitches" on panels and the selection bar ).   */
  lpDMA[ 6 ] = 28UL | 0x8000UL | ( 1ULL << 58 ) | ( 1ULL << 60 );
  lpDMA[ 7 ] = 5UL;                               /* reg: XYZ2                         */

  for ( i = 0; i < 7; ++i ) {   /* top corner region: rows y0 .. y0+r */
   float lInset = lR - lR * s_Arc7[ i ];
   float lRowY  = lY0 + lR - lR * s_Arc7[ 6 - i ];
   lpDMA[ lIdx++ ] = GS_XYZ (  ( int )( lX0 + lInset + 0.5F ), ( int )( lRowY + 0.5F ), 0  );
   lpDMA[ lIdx++ ] = GS_XYZ (  ( int )( lX1 - lInset + 0.5F ), ( int )( lRowY + 0.5F ), 0  );
  }  /* end for */

  for ( i = 0; i < 7; ++i ) {   /* bottom corner region: rows y1-r .. y1 */
   float lInset = lR - lR * s_Arc7[ 6 - i ];
   float lRowY  = lY1 - lR + lR * s_Arc7[ i ];
   lpDMA[ lIdx++ ] = GS_XYZ (  ( int )( lX0 + lInset + 0.5F ), ( int )( lRowY + 0.5F ), 0  );
   lpDMA[ lIdx++ ] = GS_XYZ (  ( int )( lX1 - lInset + 0.5F ), ( int )( lRowY + 0.5F ), 0  );
  }  /* end for */

 }  /* end else */

}  /* end GS_RenderRoundRect */
