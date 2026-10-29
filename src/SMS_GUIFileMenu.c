/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2006/7 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#include "SMS_GUI.h"
#include "SMS_GS.h"
#include "SMS_Config.h"
#include "SMS_List.h"
#include "SMS_PAD.h"
#include "SMS_DMA.h"
#include "SMS_FileDir.h"
#include "SMS_GUIcons.h"
#include "SMS_VIF.h"
#include "SMS_Locale.h"
#include "SMS_CDVD.h"
#include "SMS_FileContext.h"
#include "SMS_SubtitleContext.h"
#include "SMS_SPU.h"
#include "SMS_Sounds.h"
#include "SMS_RC.h"
#include "SMS_PgInd.h"
#include "SMS_IOP.h"
#include "SMS_ioctl.h"
#include "SMS_SMB.h"
#include "SMS_GUIClock.h"
#include "SMS_Artwork.h"
#include "SMS_JPEG.h"
#include "SMS_Rescale.h"

#include <kernel.h>
#include <malloc.h>
#include <string.h>
#include <fileio.h>
#include <fileXio_rpc.h>
#include <fcntl.h>

static char s_pPPHDL[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "PP.HDL.";
static char s_pHDLdr[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "HDLoade";

extern int  TranslateUTF8 ( int, char*, int, const char* );   /* SMS_InverseCodePages.c */
extern char g_SMSLng[ 12 ] __attribute__(   (  aligned( 1 ), section( ".data" )  )   );
extern char g_SMSPal[ 13 ] __attribute__(   (  aligned( 1 ), section( ".data" )  )   );
extern char g_SMSSMB[ 128 ] __attribute__(   (  aligned( 1 ), section( ".data" )  )   );

extern FileContext* GUI_MiniBrowser ( FileContext*, char*, void** );
extern void         GUI_FileCtxMenu ( char*, char*, int, int      );

typedef struct _LevelInfo {

 char* m_pCurrent;
 int   m_YOffset;
 int   m_Pos;

} _LevelInfo;

typedef struct GUIFileMenu {

 DECLARE_GUI_OBJECT()

 unsigned int*  m_pColor;
 SMS_List*      m_pFileList;
 SMS_List*      m_pFiltList;
 SMS_List*      m_pLevels;
 SMS_ListNode*  m_pCurrent;
 SMS_ListNode*  m_pFirst;
 SMS_ListNode*  m_pLast;
 int            m_YOffset;
 u64*           m_pSelRect;
 int            m_Active;
 int            m_ActiveY;
 int            m_ArtworkMode;
 SMS_ArtworkCache m_Artwork;

} GUIFileMenu;

typedef struct _FileMenuItem {

 u64            m_IconPack[ 32 ];
 u64*           m_pTxtPack;

} _FileMenuItem;

static u64           s_BitBltPack[ sizeof ( GSLoadImage ) * 9 ] __attribute__(   (  aligned( 16 ), section( ".data" )  )   );

/* All cached buffers are DMA sources. Never evict one while a preceding
 * display chain can still refer to its pixels or upload packet. */
static void _artwork_drain ( void ) {
 DMA_Wait ( DMAC_VIF1 );
 DMA_Wait ( DMAC_GIF );
}

static void _artwork_restore_gs ( void ) {
 /* GS_XYZ/GS_XYZv use VU0 vf31 for the current display's Y scale. JPEG's
  * IDCT owns VU0 registers. GUI packets otherwise use VIF DIRECT and require
  * no VU microprogram, but this macro-mode register must be restored before
  * building coordinates or resuming the clock. */
 const float* lpScale = GS_Params () -> m_AspectRatio;
 __asm__ __volatile__( "lqc2 $vf31, 0(%0)\n\t" :: "r"( lpScale ) : "memory" );
}

static void _artwork_position ( GUIFileMenu* apMenu ) {
 SMS_ListNode* lpNode = apMenu -> m_pFiltList -> m_pHead;
 unsigned int lIndex = 0, i;
 while ( lpNode && lpNode != apMenu -> m_pCurrent ) { lpNode = lpNode -> m_pNext; ++lIndex; }
 if ( !lpNode ) {
  apMenu -> m_pCurrent = apMenu -> m_pFiltList -> m_pHead;
  lIndex = 0;
 }
 apMenu -> m_YOffset = lIndex % SMS_ARTWORK_SLOTS;
 apMenu -> m_pFirst = SMS_ListAt ( apMenu -> m_pFiltList, lIndex - apMenu -> m_YOffset );
 lpNode = apMenu -> m_pFirst;
 apMenu -> m_pLast = NULL;
 for ( i = 0; i < SMS_ARTWORK_SLOTS && lpNode; ++i ) {
  apMenu -> m_pLast = lpNode;
  lpNode = lpNode -> m_pNext;
 }
}

static void _artwork_move ( GUIFileMenu* apMenu, int aDelta ) {
 SMS_ListNode* lpNode = apMenu -> m_pFiltList -> m_pHead;
 unsigned int lIndex = 0;
 while ( lpNode && lpNode != apMenu -> m_pCurrent ) { lpNode = lpNode -> m_pNext; ++lIndex; }
 apMenu -> m_pCurrent = SMS_ListAt ( apMenu -> m_pFiltList,
  SMS_ArtworkMove ( apMenu -> m_pFiltList -> m_Size, lIndex, aDelta ) );
 _artwork_position ( apMenu );
}

static unsigned char* _artwork_load ( const void* apKey, void* apParam ) {
 const SMS_ListNode* lpMedia = ( const SMS_ListNode* )apKey;
 SMS_ListNode* lpArt = NULL;
 FileContext* lpFile = NULL;
 SMS_JPEGContext* lpJPEG = NULL;
 unsigned char* lpData = NULL;
 unsigned char* retVal = NULL;
 char lName[ 320 ], lPath[ 1024 ];
 unsigned int lPrefix, lLength;
 int i;
 ( void )apParam;

 if ( ( int )lpMedia -> m_Param != GUICON_AVI && ( int )lpMedia -> m_Param != GUICON_AVIS &&
      ( int )lpMedia -> m_Param != GUICON_MP3 && ( int )lpMedia -> m_Param != GUICON_M3U ) return NULL;
 /* Resolve against the unfiltered listing: preserve its real case even on a
  * case-sensitive network share. .jpg wins over .jpeg; no speculative opens. */
 for ( i = 0; i < 2 && !lpArt; ++i )
  if ( SMS_ArtworkName ( lName, sizeof ( lName ), _STR( lpMedia ), i ) ) lpArt = SMS_ListFindI ( g_pFileList, lName );
 if ( !lpArt ) return NULL;
 lPrefix = strlen ( g_CWD );
 lLength = strlen ( _STR( lpArt ) );
 if ( !lPrefix || lPrefix + lLength + 2 > sizeof ( lPath ) ) return NULL;
 memcpy ( lPath, g_CWD, lPrefix );
 if ( lPath[ lPrefix - 1 ] != '/' ) lPath[ lPrefix++ ] = '/';
 strcpy ( lPath + lPrefix, _STR( lpArt ) );

 if ( g_CMedia == 1 && g_pCDDACtx ) lpFile = CDDA_InitFileContext ( lPath + 7, g_pCDDACtx );
 else lpFile = STIO_InitFileContext ( lPath, NULL );
 if ( !lpFile ) return NULL;
 if ( !lpFile -> m_Size || lpFile -> m_Size > SMS_ARTWORK_MAX_FILE ) goto end;
 lLength = lpFile -> m_Size;
 lpData = ( unsigned char* )memalign ( 64, lLength + 64 );
 if ( !lpData ) goto end;
 memset ( lpData + lLength, 0, 64 );
 if ( lpFile -> Read ( lpFile, lpData, lLength ) != ( int )lLength || !SMS_ArtworkJPEGSize ( lpData, lLength ) ) goto end;
 lpFile -> Destroy ( lpFile );
 lpFile = NULL;

 SMS_GUIClockSuspend ();
 _artwork_drain ();
 lpJPEG = SMS_JPEGInit ( NULL, NULL );
 if ( lpJPEG && SMS_JPEGLoad ( lpJPEG, lpData, lLength ) && lpJPEG -> m_pRC &&
      lpJPEG -> m_nBitmapBytes == lpJPEG -> m_pRC -> m_NewWidth * lpJPEG -> m_pRC -> m_NewHeight * 3 ) {
  retVal = ( unsigned char* )memalign ( 64, SMS_ARTWORK_BYTES );
  /* JPEG ProcessBuffer writes through the uncached alias. Sample that same
   * alias: cached lines from a reused heap block may still hold older data. */
  if ( retVal && !SMS_ArtworkThumbnail ( retVal, UNCACHED_SEG( lpJPEG -> m_pBitmap ),
       lpJPEG -> m_pRC -> m_NewWidth, lpJPEG -> m_pRC -> m_NewHeight ) ) {
   free ( retVal ); retVal = NULL;
  }
  if ( retVal ) SyncDCache ( retVal, retVal + SMS_ARTWORK_BYTES );
 }
 DMA_Wait ( DMAC_VIF0 );
 _artwork_drain ();
 SMS_JPEGDestroy ( lpJPEG );
 _artwork_restore_gs ();
 SMS_GUIClockResume ();
end:
 if ( lpFile ) lpFile -> Destroy ( lpFile );
 free ( lpData );
 return retVal;
}

/* The ordinary icon renderer already owns 0x3FC0..0x4000 as transient
 * PSMCT32 scratch. Upload/draw each 64x64 thumbnail in the same VIF chain;
 * TEXFLUSH precedes its sprite and the next upload follows that draw. */
static void _artwork_packet ( u64* apDMA, unsigned char* apPixels, int aX, int aY, int aSide ) {
 apDMA[  0 ] = DMA_TAG( 6, 0, DMATAG_ID_CNT, 0, 0, 0 );
 apDMA[  1 ] = VIF_DIRECT( 6 );
 apDMA[  2 ] = GIF_TAG( 4, 0, 0, 0, GIFTAG_FLG_PACKED, 1 );
 apDMA[  3 ] = GIFTAG_REGS_AD;
 apDMA[  4 ] = GS_SET_BITBLTBUF( 0, 0, 0, 0x3FC0, 1, GSPixelFormat_PSMCT32 );
 apDMA[  5 ] = GS_BITBLTBUF;
 apDMA[  6 ] = GS_SET_TRXPOS( 0, 0, 0, 0, 0 );
 apDMA[  7 ] = GS_TRXPOS;
 apDMA[  8 ] = GS_SET_TRXREG( SMS_ARTWORK_SIDE, SMS_ARTWORK_SIDE );
 apDMA[  9 ] = GS_TRXREG;
 apDMA[ 10 ] = GS_SET_TRXDIR( GS_TRXDIR_HOST_TO_LOCAL );
 apDMA[ 11 ] = GS_TRXDIR;
 apDMA[ 12 ] = GIF_TAG( SMS_ARTWORK_BYTES / 16, 0, 0, 0, GIFTAG_FLG_IMAGE, 0 );
 apDMA[ 13 ] = 0;
 apDMA[ 14 ] = DMA_TAG( SMS_ARTWORK_BYTES / 16, 0, DMATAG_ID_REF, 0, ( unsigned int )apPixels, 0 );
 apDMA[ 15 ] = VIF_DIRECT( SMS_ARTWORK_BYTES / 16 );
 apDMA[ 16 ] = DMA_TAG( 7, 0, DMATAG_ID_RET, 0, 0, 0 );
 apDMA[ 17 ] = VIF_DIRECT( 7 );
 apDMA[ 18 ] = GIF_TAG( 3, 0, 0, 0, GIFTAG_FLG_PACKED, 1 );
 apDMA[ 19 ] = GIFTAG_REGS_AD;
 apDMA[ 20 ] = 0;
 apDMA[ 21 ] = GS_TEXFLUSH;
 apDMA[ 22 ] = GS_SET_TEX0( 0x3FC0, 1, GSPixelFormat_PSMCT32, 6, 6, GS_TEX_TCC_RGBA, GS_TEX_TFX_DECAL, 0, 0, 0, 0, 0 );
 apDMA[ 23 ] = GS_TEX0_1;
 apDMA[ 24 ] = GS_SET_PRIM( GS_PRIM_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0 );
 apDMA[ 25 ] = GS_PRIM;
 apDMA[ 26 ] = GIF_TAG( 2, 1, 0, 0, GIFTAG_FLG_REGLIST, 2 );
 apDMA[ 27 ] = GS_UV | ( GS_XYZ2 << 4 );
 apDMA[ 28 ] = GS_SET_UV( 8, 8 );
 apDMA[ 29 ] = GS_XYZ ( aX, aY, 0 );
 apDMA[ 30 ] = GS_SET_UV( ( SMS_ARTWORK_SIDE << 4 ) - 8, ( SMS_ARTWORK_SIDE << 4 ) - 8 );
 apDMA[ 31 ] = GS_XYZ ( aX + aSide, aY + aSide, 0 );
}

static int _filter_item ( SMS_ListNode* apNode ) {

 int lType = ( int )apNode -> m_Param;

 if (  lType == GUICON_PARTITION && (
        (  !( g_Config.m_BrowserFlags & SMS_BF_HDLP ) &&
            (  !strncmp (  _STR( apNode ), s_pPPHDL, 7  ) ||
               !strncmp (  _STR( apNode ), s_pHDLdr, 7  )
            )
        ) ||
        (  g_Config.m_BrowserFlags & SMS_BF_SYSP &&
           _STR( apNode )[ 0 ] == '_' &&
           _STR( apNode )[ 1 ] == '_'
        )
       )
 ) return 1;

 if ( lType == GUICON_PARTITION || lType == GUICON_FOLDER || lType == GUICON_SHARE ) return 0;

 if (  ( g_Config.m_BrowserFlags & SMS_BF_AVIF ) &&
       !(  lType == GUICON_AVI     ||
           lType == GUICON_MP3     ||
           lType == GUICON_M3U     ||
           lType == GUICON_AVIS    ||
           lType == GUICON_PICTURE
        )
 ) return 1;

 return 0;

}  /* end _filter_item */

/* Filenames arrive as UTF-8 and the font is a single-byte codepage, so an accented letter
 * is TWO bytes and gets drawn as TWO glyphs -- "Canción" measures 30px where "Cancion"
 * measures 15px for the same letter. That extra width is what pushes an accented name past
 * the column and makes the fit loop below chop the tail, which is what a Spanish tester
 * reported: remove the accents and the name shows in full.
 *
 * ( The signed-char width lookup fixed in adf377e was a genuine out-of-bounds read on the
 *   same bytes and had to go, but it is not the whole story: correcting it leaves each
 *   accent still costing two glyphs. )
 *
 * bdmfs_fatfs.irx is the module SMS loads, and it really does emit UTF-8 -- its code carries
 * the encoder signature ( the < 0x80 / < 0x800 range tests, the 0x3F continuation mask and
 * the 0xC0 lead-byte construction ); the unused bdmfs_vfat.irx has none of it.
 *
 * TranslateUTF8 already does this conversion for subtitles, and collapses each sequence to
 * one codepage byte -- so the name both measures and DRAWS correctly. Unmappable characters
 * become '?' ( NOT_AVAILABLE_CHAR ), never NUL, so a name can never be cut short by this.
 *
 * DISPLAY ONLY. The node's own string stays untouched and is what every fioOpen uses; a
 * transcoded name would not open. */
static int _is_utf8 ( const char* apStr ) {

 int lfAny = 0;

 while ( *apStr ) {

  unsigned char c = ( unsigned char )*apStr++;

  if ( c < 0x80 ) continue;

  if ( c >= 0xC2 && c <= 0xDF ) {                       /* 2-byte */
   if (  (  ( unsigned char )*apStr & 0xC0  ) != 0x80  ) return 0;
   ++apStr;
  } else if ( c >= 0xE0 && c <= 0xEF ) {                /* 3-byte */
   if (  (  ( unsigned char )apStr[ 0 ] & 0xC0  ) != 0x80 ||
         (  ( unsigned char )apStr[ 1 ] & 0xC0  ) != 0x80  ) return 0;
   apStr += 2;
  } else return 0;                                      /* stray continuation or 4-byte */

  lfAny = 1;

 }  /* end while */

 return lfAny;   /* pure ASCII -> 0: nothing to convert, leave it alone */

}  /* end _is_utf8 */

static _FileMenuItem* GUIFileMenu_RenderItem ( SMS_ListNode* apNode, int anY, int aWidth, int afDim ) {

 int            lLen;
 _FileMenuItem* retVal;
 char           lName[ 320 ];
 const char*    lpName = _STR( apNode );

 if (  _is_utf8 ( lpName )  &&
       TranslateUTF8 (  g_GSCtx.m_CodePage, lName, sizeof ( lName ) - 1, lpName  ) > 0
 ) lpName = lName;

 lLen = strlen ( lpName );

 if (  ( g_Config.m_BrowserFlags & SMS_BF_AVIF ) &&
       (  ( int )apNode -> m_Param == GUICON_AVI ||
          ( int )apNode -> m_Param == GUICON_MP3 ||
          ( int )apNode -> m_Param == GUICON_M3U ||
          ( int )apNode -> m_Param == GUICON_AVIS
       )
 ) {

  if (  lpName[ lLen - 4 ] == '.'  )
   lLen -= 4;
  else lLen -= 5;

 }  /* end if */

 while (   GSFont_WidthEx (  ( char* )lpName, lLen, -2  ) > aWidth   ) --lLen;

 retVal = ( _FileMenuItem* )SMS_SyncMalloc (  sizeof ( _FileMenuItem )  );

/* SDTV ( NTSC / PAL ) centres each row's icon + label between the row-frame edges, but the
 * DTV / HDTV component modes ( 480p / 576p / 720p / 1080i -- m_GSCRTMode >= 0x50 ) render
 * them ~2px high. Nudge both down 2px in those modes so they sit centred. Applies to the
 * icon and the text identically, so they stay aligned. ( Nad, PS2 Scene graphics review. ) */
 {
  int lYd = ( GS_Params () -> m_GSCRTMode >= GSVideoMode_DTV_720x480P ) ? ( anY + 2 ) : anY;

  GUI_DrawIcon (
   ( int )apNode -> m_Param + afDim, 8, lYd, GUIcon_Browser,
   UNCACHED_SEG( retVal -> m_IconPack )
  );
  retVal -> m_pTxtPack = GSContext_NewList (  GS_TXT_PACKET_SIZE( lLen )  );
  GSFont_RenderEx (  ( char* )lpName, lLen, 46, lYd, retVal -> m_pTxtPack, -2, 0  );
 }

 return retVal;

}  /* end GUIFileMenu_RenderItem */

static void _artwork_render ( GUIFileMenu* apMenu, int aCtx ) {
 SMS_ListNode* lpNode;
 _FileMenuItem* lpPack;
 const void* lKeys[ SMS_ARTWORK_SLOTS ];
 unsigned int i, lCount = 0;
 int lCellW = ( g_GSCtx.m_Width - 16 ) / SMS_ARTWORK_COLUMNS;
 int lCellH = ( g_GSCtx.m_Height - 116 ) / 2;
 int lSide = lCellH - 42;

 if ( lSide > lCellW - 20 ) lSide = lCellW - 20;
 if ( lSide > 128 ) lSide = 128;
 if ( lSide < 16 ) lSide = 16;

 if ( !apMenu -> m_pGSPacket ) {
  u64* lpDMA;
  _artwork_position ( apMenu );
  lpNode = apMenu -> m_pFirst;
  while ( lpNode && lCount < SMS_ARTWORK_SLOTS ) { lKeys[ lCount++ ] = lpNode; lpNode = lpNode -> m_pNext; }
  _artwork_drain ();
  SMS_ArtworkPage ( &apMenu -> m_Artwork, lKeys, lCount );
  /* Decode first. Display lists/packets live on the heap; the decoder owns
   * scratchpad 0x70000000..0x70001fff and VU memory. No packet is submitted
   * during decoding, and all coordinate construction follows GS restoration. */
  for ( i = 0; i < lCount; ++i ) SMS_ArtworkGet ( &apMenu -> m_Artwork, i, _artwork_load, NULL );

  lpDMA = GSContext_NewList ( GS_RRT_PACKET_SIZE() );
  GS_RenderRoundRect ( ( GSRoundRectPacket* )( lpDMA - 2 ), 0, 60,
   g_GSCtx.m_Width - 1, g_GSCtx.m_Height - 101, -8, g_Palette[ *apMenu -> m_pColor - 1 ] );
  apMenu -> m_pGSPacket = lpDMA;
  lpNode = apMenu -> m_pFirst;
  g_GSCtx.m_TextColor = 1;

  for ( i = 0; i < lCount; ++i ) {
   unsigned char* lpPixels = apMenu -> m_Artwork.m_Slots[ i ].m_pPixels;
   char lName[ 320 ];
   const char* lpName = _STR( lpNode );
   int lLen, lX = 8 + ( i % SMS_ARTWORK_COLUMNS ) * lCellW;
   int lY = 68 + ( i / SMS_ARTWORK_COLUMNS ) * lCellH;
   lpPack = ( _FileMenuItem* )SMS_SyncMalloc ( sizeof ( _FileMenuItem ) );
   if ( !lpPack ) break;
   if ( lpPixels ) _artwork_packet ( UNCACHED_SEG( lpPack -> m_IconPack ), lpPixels,
    lX + ( lCellW - lSide ) / 2, lY + 5, lSide );
   else GUI_DrawIcon ( ( int )lpNode -> m_Param, lX + ( lCellW - 32 ) / 2,
    lY + 5 + ( lSide - 32 ) / 2, GUIcon_Browser, UNCACHED_SEG( lpPack -> m_IconPack ) );

   if ( _is_utf8 ( lpName ) && TranslateUTF8 ( g_GSCtx.m_CodePage, lName, sizeof ( lName ) - 1, lpName ) > 0 ) lpName = lName;
   lLen = strlen ( lpName );
   while ( lLen > 0 && GSFont_WidthEx ( ( char* )lpName, lLen, -6 ) > lCellW - 12 ) --lLen;
   lpPack -> m_pTxtPack = GSContext_NewList ( GS_TXT_PACKET_SIZE( lLen ) );
   GSFont_RenderEx ( ( char* )lpName, lLen, lX + 6, lY + lSide + 9, lpPack -> m_pTxtPack, -6, -8 );
   SMS_ListPushBack ( apMenu -> m_pFileList, _STR( lpNode ) ) -> m_Param = ( unsigned int )lpPack;

   if ( lpNode == apMenu -> m_pCurrent ) {
    u64 lColor = g_Palette[ g_Config.m_BrowserSCIdx - 1 ];
    if ( !apMenu -> m_pSelRect ) apMenu -> m_pSelRect = GSContext_NewList ( GS_RRT_PACKET_SIZE() << 1 );
    GS_RenderRoundRect ( ( GSRoundRectPacket* )( apMenu -> m_pSelRect + GS_RRT_PACKET_SIZE() - 2 ),
     lX, lY, lCellW - 4, lCellH - 4, 8, ( lColor & 0x00FFFFFF ) | 0x18000000 );
    GS_RenderRoundRect ( ( GSRoundRectPacket* )( apMenu -> m_pSelRect - 2 ),
     lX, lY, lCellW - 4, lCellH - 4, -8, lColor );
    apMenu -> m_pSelRect[ -1 ] = VIF_DIRECT( GS_RRT_PACKET_SIZE() );
    apMenu -> m_ActiveY = lY;
   }
   lpNode = lpNode -> m_pNext;
  }
  SMS_InitBitBlt ( s_BitBltPack, 9, 59, g_GSCtx.m_Height - 59 - 36 );
 }
 GSContext_CallList ( aCtx, apMenu -> m_pGSPacket );
 if ( apMenu -> m_pSelRect && apMenu -> m_pFileList -> m_Size ) GSContext_CallList ( aCtx, apMenu -> m_pSelRect );
 for ( lpNode = apMenu -> m_pFileList -> m_pHead; lpNode; lpNode = lpNode -> m_pNext ) {
  lpPack = ( _FileMenuItem* )( unsigned int )lpNode -> m_Param;
  GSContext_CallList2 ( aCtx, lpPack -> m_IconPack );
  GSContext_CallList ( aCtx, lpPack -> m_pTxtPack );
 }
}

static void GUIFileMenu_Render ( GUIObject* apObj, int aCtx ) {

 GUIFileMenu*   lpMenu = ( GUIFileMenu* )apObj;
 SMS_ListNode*  lpNode;
 _FileMenuItem* lpPack;

 if ( lpMenu -> m_ArtworkMode ) { _artwork_render ( lpMenu, aCtx ); return; }

 if ( !lpMenu -> m_pGSPacket ) {

  u64*           lpDMA   = GSContext_NewList (  GS_RRT_PACKET_SIZE()  );
  int            lHeight = g_GSCtx.m_Height - 101;
  int            lWidth  = g_GSCtx.m_Width  - 48;
  int            lnItems = ( lHeight - 2 ) / 34;
  int            lY      = 62;
  int            lSelY   = lY + 34 * lpMenu -> m_YOffset;

  GS_RenderRoundRect (
   ( GSRoundRectPacket* )( lpDMA - 2 ), 0, 60, g_GSCtx.m_Width - 1, lHeight, -8,
   g_Palette[ *lpMenu -> m_pColor - 1 ]
  );
  lpMenu -> m_pGSPacket = lpDMA;

  if (  ( lpNode = lpMenu -> m_pFirst )  ) {

   if ( lpNode -> m_pPrev ) {

    g_GSCtx.m_TextColor = 2;
    lpPack = GUIFileMenu_RenderItem ( lpNode, lY, lWidth, 1 );

    SMS_ListPushBack (
     lpMenu -> m_pFileList, _STR( lpNode )
    ) -> m_Param = ( unsigned int )lpPack;

    lpMenu -> m_pLast = lpNode;
    lY += 34; --lnItems;
    lpNode = lpNode -> m_pNext;

   }  /* end if */

   g_GSCtx.m_TextColor = 1;

   while ( lnItems > 1 && lpNode ) {

    lpPack = GUIFileMenu_RenderItem ( lpNode, lY, lWidth, 0 );

    SMS_ListPushBack (
     lpMenu -> m_pFileList, _STR( lpNode )
    ) -> m_Param = ( unsigned int )lpPack;

    lpMenu -> m_pLast = lpNode;
    lY += 34; --lnItems;
    lpNode = lpNode -> m_pNext;

   }  /* end while */

   if ( lpNode ) {

    int lClr = lpNode -> m_pNext ? 2 : 1;

    g_GSCtx.m_TextColor = lClr;

    lpPack = GUIFileMenu_RenderItem ( lpNode, lY, lWidth, lClr - 1 );

    SMS_ListPushBack (
     lpMenu -> m_pFileList, _STR( lpNode )
    ) -> m_Param = ( unsigned int )lpPack;
    lpMenu -> m_pLast = lpNode;

   }  /* end if */

  }  /* end if */

  if ( lpMenu -> m_pCurrent ) {

   u64           lColor = g_Palette[ g_Config.m_BrowserSCIdx - 1 ];
   unsigned int  lW     = lWidth + 40;

   if ( !lpMenu -> m_pSelRect ) lpMenu -> m_pSelRect = GSContext_NewList (  GS_RRT_PACKET_SIZE() << 1  );

   GS_RenderRoundRect (
    ( GSRoundRectPacket* )(  lpMenu -> m_pSelRect + GS_RRT_PACKET_SIZE() - 2  ),
    4, lSelY, lW, 34, 8, ( lColor & 0x00FFFFFF ) | 0x10000000
   );
   GS_RenderRoundRect (
    ( GSRoundRectPacket* )( lpMenu -> m_pSelRect - 2 ),
    4, lSelY, lW, 34, -8, lColor
   );
   lpMenu -> m_pSelRect[ -1 ] = VIF_DIRECT(  GS_RRT_PACKET_SIZE()  );
   lpMenu -> m_ActiveY        = lSelY;

  }  /* end if */

  SMS_InitBitBlt ( s_BitBltPack, 9, 59, g_GSCtx.m_Height - 59 - 36 );   /* starts below the device bar's Y=54..57 selection arrow */

 }  /* end if */

 GSContext_CallList ( aCtx, lpMenu -> m_pGSPacket );

 lpNode = lpMenu -> m_pFileList -> m_pHead;

 while ( lpNode ) {

  lpPack = ( _FileMenuItem* )( unsigned int )lpNode -> m_Param;

  GSContext_CallList2 ( aCtx, lpPack -> m_IconPack );
  GSContext_CallList  ( aCtx, lpPack -> m_pTxtPack );

  lpNode = lpNode -> m_pNext;

 }  /* end while */

 if ( lpMenu -> m_pSelRect && lpMenu -> m_pFileList -> m_Size ) GSContext_CallList ( aCtx, lpMenu -> m_pSelRect );

}  /* end GUIFileMenu_Render */

void GUIFileMenu_CleanupPackets ( GUIObject* apObj ) {

 GUIFileMenu*   lpMenu = ( GUIFileMenu* )apObj;
 SMS_ListNode*  lpNode = lpMenu -> m_pFileList -> m_pHead;
 _FileMenuItem* lpPack;

 _artwork_drain ();
 GUIObject_Cleanup ( apObj );

 while ( lpNode ) {

  lpPack = ( _FileMenuItem* )( unsigned int )lpNode -> m_Param;

  GSContext_DeleteList ( lpPack -> m_pTxtPack );
  free ( lpPack );

  lpNode = lpNode -> m_pNext;

 }  /* end while */

 SMS_ListDestroy ( lpMenu -> m_pFileList, 0 );

}  /* end GUIFileMenu_CleanupPackets */

static void GUIFileMenu_Cleanup ( GUIObject* apObj ) {
 GUIFileMenu* lpMenu = ( GUIFileMenu* )apObj;
 GUIFileMenu_CleanupPackets ( apObj );
 SMS_ArtworkClear ( &lpMenu -> m_Artwork );
}  /* end GUIFileMenu_Cleanup */

static void _redraw ( GUIFileMenu* apMenu, int afAll ) {

 GUIFileMenu_CleanupPackets (  ( GUIObject* )apMenu  );

 if ( !afAll ) {

  /* The partial scroll redraw only repaints the list panel (s_BitBltPack       */
  /* backdrop restore + list, Y = 59 .. m_Height-36). The GUI is single-        */
  /* buffered, so the bottom bar below that (status/path text + the async       */
  /* clock, Y >= m_Height-36) is never touched here -- any corruption of it is  */
  /* STICKY and accumulates across a long scroll. So we must also repaint the   */
  /* full-width status line every step (g_pStatusLine->Render re-blits its      */
  /* backdrop + panel + cached path text, exactly as GUI_Status does), and      */
  /* serialize with the async VBLANK clock: SMS_GUIClockResume kicks a          */
  /* DMAC_GIF chain, and GSContext_Flush(KeepLists) kicks the scroll's VIF1     */
  /* chain WITHOUT waiting, so without the DMA_Wait the two cross-arbitrate on   */
  /* the GIF and corrupt the clock box. Resume then re-blits the clock cleanly. */
  GSContext_NewPacket ( 1, 0, GSPaintMethod_Init );
  GSContext_CallList2 (  1, ( u64*           )&s_BitBltPack  );
  GUIFileMenu_Render (  ( GUIObject* )apMenu, 1  );
  g_pStatusLine -> Render ( g_pStatusLine, 1 );
  SMS_GUIClockSuspend ();
  GS_VSync2 ( g_GSCtx.m_DrawDelay );
  GSContext_Flush ( 1, GSFlushMethod_KeepLists );
  DMA_Wait ( DMAC_VIF1 );
  SMS_GUIClockResume ();

 } else GUI_Redraw ( GUIRedrawMethod_Redraw );

}  /* end _redraw */

static void GUIFileMenu_SetFocus ( GUIObject* apObj, int afSet ) {

 GUIFileMenu* lpMenu = ( GUIFileMenu* )apObj;

 lpMenu -> m_Active = afSet;
 lpMenu -> m_pColor = ( afSet ? &(g_Config.m_BrowserABCIdx) : &(g_Config.m_BrowserIBCIdx) );

 _redraw ( lpMenu, 0 );

}  /* end GUIFileMenu_SetFocus */

static void _reset ( GUIFileMenu* apMenu, int afAll ) {

 _artwork_drain ();
 SMS_ArtworkClear ( &apMenu -> m_Artwork );
 /* Grid positioning chooses the filtered head when there is no selection.
  * Remove stale entries before redrawing a removed or changing device. */
 SMS_ListDestroy ( apMenu -> m_pFiltList, 0 );
 apMenu -> m_pFirst   = NULL;
 apMenu -> m_pLast    = NULL;
 apMenu -> m_pCurrent = NULL;
 apMenu -> m_YOffset  = 0;

 _redraw ( apMenu, afAll );

}  /* end _reset */

static void _push_state ( GUIFileMenu* apMenu ) {

 _LevelInfo* lpInfo = ( _LevelInfo* )malloc (  sizeof ( _LevelInfo )  );

 lpInfo -> m_pCurrent = ( char* )malloc (   strlen (  _STR( apMenu -> m_pCurrent )  ) + 1   );
 lpInfo -> m_YOffset  = apMenu -> m_YOffset;
 lpInfo -> m_Pos      = strlen ( g_CWD );

 strcpy (  lpInfo -> m_pCurrent, _STR( apMenu -> m_pCurrent )  );

 SMS_ListPushBack (
  apMenu -> m_pLevels, _STR( apMenu -> m_pFirst )
 ) -> m_Param = ( unsigned int )lpInfo;

}  /* end _push_state */

static void _pop_state ( GUIFileMenu* apMenu ) {

 _LevelInfo* lpInfo = ( _LevelInfo* )( unsigned int )apMenu -> m_pLevels -> m_pTail -> m_Param;

 free ( lpInfo -> m_pCurrent );
 free ( lpInfo );

 SMS_ListPopBack ( apMenu -> m_pLevels );

}  /* end _pop_state */

static void _fill ( GUIFileMenu* apMenu, char* apPath, int afAll ) {

 SMS_ListNode* lpNode;
 char lPath[ SMS_CWD_CAPACITY ];
 unsigned int lLength = strlen ( apPath );

 /* A folder action passes its filtered node's name. Reset frees that node. */
 if ( lLength >= sizeof ( lPath ) ) return;
 memcpy ( lPath, apPath, lLength + 1 );
 _reset ( apMenu, afAll );

 SMS_PgIndStart ();
 SMS_FileDirInit ( lPath );

 lpNode = g_pFileList -> m_pHead;
 SMS_ListDestroy ( apMenu -> m_pFiltList, 0 );

 while ( lpNode ) {

  if (  !_filter_item ( lpNode )  ) SMS_ListPushBack (
   apMenu -> m_pFiltList, _STR( lpNode )
  ) -> m_Param = lpNode -> m_Param;

  lpNode = lpNode -> m_pNext;

 }  /* end while */

 apMenu -> m_pFirst   =
 apMenu -> m_pCurrent = apMenu -> m_pFiltList -> m_pHead;

 SMS_PgIndStop ();

}  /* end _fill */

void RestoreFileDir ( void** apParam ) {
 
 apParam[ 0 ] = ( void* )(  ( unsigned int )apParam[ 0 ] & 0x3FFFFFFF  );

 SMS_ListDestroy (  ( SMS_List* )apParam[ 0 ], 1  );
 g_pFileList = ( SMS_List* )apParam[ 1 ];
 strcpy (  g_CWD, ( char* )( apParam + 2 )  );
 free ( apParam );

 GUI_UpdateStatus ();

}  /* end RestoreFileDir */

static int _make_path ( char* apBuff, unsigned int aCapacity, char** appPathEnd, const char* apName ) {

 unsigned int lPrefix = strlen ( g_CWD );
 unsigned int lName = strlen ( apName );
 unsigned int lSlash = lPrefix && g_CWD[ lPrefix - 1 ] != '/';
 *appPathEnd = NULL;
 if ( lPrefix >= aCapacity || lSlash >= aCapacity - lPrefix ||
      lName >= aCapacity - lPrefix - lSlash ) return 0;
 memcpy ( apBuff, g_CWD, lPrefix );
 if ( lSlash ) apBuff[ lPrefix++ ] = '/';
 memcpy ( apBuff + lPrefix, apName, lName + 1 );
 *appPathEnd = apBuff + lPrefix;
 return 1;

}  /* end _make_path */

/* Basename of the file most recently handed to the player. Continuous playback needs to know
 * WHERE in the current listing the playing track sits so it can find the one after it, and the
 * player itself only ever receives a FileContext -- it has no idea which browser row produced
 * it. Recorded here, at the single choke point every play request already goes through, rather
 * than threading a node pointer through the GUI message payload ( which is a packed u64 with
 * no room left ). Basename, matching what g_pFileList stores, so SMS_ListFindI can locate it. */
char g_LastPlayed[ 256 ] __attribute__(   (  section( ".bss" )  )   );

void** SMS_OpenMediaFile ( const char* apName, int aFlags ) {

 char         lPath[ 1024 ];
 char*        lpPathEnd;
 FileContext* lpFileCtx;
 void**       retVal = NULL;

 SMS_PgIndStart ();

 if ( !_make_path ( lPath, sizeof ( lPath ), &lpPathEnd, apName ) ) {
  SMS_PgIndStop ();
  return NULL;
 }

 strncpy (  g_LastPlayed, apName, sizeof ( g_LastPlayed ) - 1  );
 g_LastPlayed[  sizeof ( g_LastPlayed ) - 1  ] = '\0';

 if ( g_CMedia == 1 && g_pCDDACtx )

  lpFileCtx = CDDA_InitFileContext (  &lPath[ 7 ], g_pCDDACtx );

 else lpFileCtx = STIO_InitFileContext ( lPath, NULL );

 if ( lpFileCtx ) {

  u64           lEvent = GUI_MSG_FILE;

  retVal = malloc (   (  sizeof ( void* ) << 1  ) + sizeof ( void* )  );

  retVal[ 0 ] = lpFileCtx;
  retVal[ 1 ] = NULL;

  if ( aFlags & SMS_FA_FLAGS_SUB ) {
   SMS_PgIndStop  ();
    retVal[ 1 ] = GUI_MiniBrowser ( lpFileCtx, lPath, &retVal[ 2 ] );
   SMS_PgIndStart ();
  }  /* end if */

  if (  ( aFlags & SMS_FA_FLAGS_AVI ) && !retVal[ 1 ] && !( aFlags & SMS_FA_FLAGS_SUB ) && ( g_Config.m_PlayerFlags & SMS_PF_SUBS )  ) {

   int            i;
   char*          lpDot;
   char           lName[ 1024 ];
   char*          lExt[ 3 ] = { g_pSrtStr,          g_pSubStr,          g_pTxtStr          };
   SubtitleFormat lFmt[ 3 ] = { SubtitleFormat_SRT, SubtitleFormat_SUB, SubtitleFormat_SUB };

   strcpy ( lName, apName );
   lpDot = strrchr ( lName, '.' );

   if ( !lpDot ) {

    lpDot  = lName + strlen ( lName );
    *lpDot = '.';

   }  /* end if */

   ++lpDot;

   for (  i = 0; i < sizeof ( lFmt ) / sizeof ( lFmt[ 0 ] ); ++i  ) {

    SMS_ListNode* lpNode;

    strcpy ( lpDot, lExt[ i ] );

    lpNode = SMS_ListFindI ( g_pFileList, lName );

    if ( lpNode ) {

     FileContext* lpSubFileCtx;
     char*        lpName = g_CMedia == 1 && g_pCDDACtx ? &lPath[ 7 ] : lPath;

     if ( strlen ( _STR( lpNode ) ) >= sizeof ( lPath ) - ( lpPathEnd - lPath ) ) continue;
     strcpy (  lpPathEnd, _STR( lpNode )  );
     lpSubFileCtx = lpFileCtx -> Open ( lpName, lpFileCtx -> m_pOpenParam );

     if ( lpSubFileCtx ) {

      retVal[ 1 ] = ( void* )lpSubFileCtx;
      retVal[ 2 ] = ( void* )lFmt[ i ];

      break;

     }  /* end if */

    }  /* end if */

   }  /* end for */

  }  /* end if */

  if ( aFlags & SMS_FA_FLAGS_MSG ) {

   lEvent |= (  ( u64           )( unsigned int )retVal  ) << 28;
   GUI_PostMessage ( lEvent );

  }  /* end if */

 }  /* end if */

 SMS_PgIndStop ();

 return retVal;

}  /* end SMS_OpenMediaFile */

static void _perform_file_action ( GUIFileMenu* apMenu, int aFlags ) {

 if ( apMenu -> m_ArtworkMode ) GUIFileMenu_Cleanup ( ( GUIObject* )apMenu );
 if ( !SMS_OpenMediaFile ( _STR( apMenu -> m_pCurrent ), aFlags | SMS_FA_FLAGS_MSG ) && apMenu -> m_ArtworkMode ) _redraw ( apMenu, 0 );

}  /* end _perform_file_action */

static void _action_avi ( GUIFileMenu* apMenu, int afPopup ) {

 int lFlags = SMS_FA_FLAGS_AVI;

 if ( afPopup ) lFlags |= SMS_FA_FLAGS_SUB;

 _perform_file_action ( apMenu, lFlags );

}  /* end _action_avi */

static void _action_file ( GUIFileMenu* apMenu, int afPopup ) {

 _perform_file_action ( apMenu, 0 );

}  /* end _action_file */

static void _action_partition ( GUIFileMenu* apMenu, int afPopup ) {

 int  lPD;
 char lPath[ 2 ] __attribute__(   (  aligned( 4 )  )   );
 char lMountPath[ strlen (  _STR( apMenu -> m_pCurrent )  ) + 6 ];

 strcpy (  lMountPath, g_pDevName[ 2 ]               );
 strcat (  lMountPath, g_ColonStr                    );
 strcat (  lMountPath, _STR( apMenu -> m_pCurrent )  );
 strcpy (  g_CWD,      g_pPFS                        );

 if ( g_PD >= 0 ) g_PD = 0 - !SMS_IOCtl ( g_CWD, PFS_IOCTL_UMOUNT, NULL );

 lPD = SMS_HDDMount ( g_CWD, lMountPath, FIO_MT_RDWR );

 if ( lPD >= 0 ) {

  strcpy (  g_Config.m_Partition, _STR( apMenu -> m_pCurrent )  );

  *( unsigned short* )&lPath[ 0 ] = 0x002F;
  _push_state ( apMenu );
  _fill ( apMenu, lPath, 0 );
  _redraw ( apMenu, 0 );

  g_PD = lPD;

 }  /* end if */

}  /* end _action_partition */

static void _context_action_part ( GUIFileMenu* apMenu, int afPopup ) {

}  /* end _context_action_part */

static void _context_action_file ( GUIFileMenu* apMenu, int afPopup ) {
#ifdef EMBEDDED
 static char sl_SMSsms[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "SMS.sms";
#endif  /* EMBEDDED */
 int lAction = -1;
 int lfPart  = g_Config.m_Partition[ 0 ] != '\x00';

 if ( afPopup == 3 ) {
  lAction =  9;
 } else if ( afPopup == 4 ) {
  lAction = 10;
 } else if ( afPopup == 2 ) {
  if ( g_CMedia == 2 )  /* HDD */
   lAction = 3;         /* delete tree */
  else if ( g_CMedia == 3 || g_CMedia == 4 ) return;  /* CDDA || HOST */
  else if ( lfPart ) lAction = 1;  /* copy tree to HDD */
 } else if ( afPopup == 1 && g_CMedia != 2 && lfPart ) {
  lAction = 0;  /* copy file to HDD */
 } else if ( g_CMedia == 2 && lfPart ) {  /* HDD */
  lAction = 2;                            /* delete file */
 } else lAction = 100;

 if ( lAction >= 0 ) {

  char  lPath[ 1024 ];
  char* lpPathEnd;
  char* lpCur      = _STR( apMenu -> m_pCurrent );
  char  lRoot[ 2 ] = { '\x01', '\x00' };

  if ( !_make_path ( lPath, sizeof ( lPath ), &lpPathEnd, lpCur ) ) return;

  if ( lAction == 10 ) lpCur = lRoot;

  if ( lAction == 0 || lAction == 100 ) {
   if (  !strcmp ( lpCur, &g_SMSLng[ 4 ] )  )
    lAction = 4;  /* update language file */
   else if (  !strcmp ( lpCur, &g_SMSPal[ 5 ] )  )
    lAction = 5;  /* update palette file */
#ifdef EMBEDDED
   else if (  !strcmp ( lpCur, sl_SMSsms )  )
    lAction = 6;  /* update SMS image */
#endif  /* EMBEDDED */
   else if (  !strcmp ( lpCur, "SMS.smb" )  )
    lAction = 7;  /* update SMB server list ( fixed file name; g_SMSSMB dir now varies by boot device ) */
   else {
    int lLen = strlen ( lpCur );
    if ( lLen > 4 && lpCur[ lLen - 4 ] == '.' &&
                     lpCur[ lLen - 3 ] == 's' &&
                     lpCur[ lLen - 2 ] == 'm' &&
                     lpCur[ lLen - 1 ] == 'i'
    ) lAction = 8;  /* add background image */
   }  /* end else */
  }  /* end if */

  GUI_FileCtxMenu ( lAction == 9 || lAction == 10 ? lpCur : lPath, lpPathEnd, lAction, apMenu -> m_ActiveY );

 }  /* end if */

}  /* end _context_action_file */

static void _action_folder ( GUIFileMenu* apMenu, int afPopup ) {

 if ( !afPopup ) {

  unsigned int lLength = strlen ( g_CWD );
  unsigned int lNameLength = strlen ( _STR( apMenu -> m_pCurrent ) );
  unsigned int lSeparator = lLength && g_CWD[ lLength - 1 ] != '/' &&
                           g_CWD[ lLength - 1 ] != '\\' &&
                           _STR( apMenu -> m_pCurrent )[ 0 ] != '/' &&
                           _STR( apMenu -> m_pCurrent )[ 0 ] != '\\';
  /* Check before saving a navigation level or discarding the current list.
   * Reserve a separator and the terminator in the shared CWD workspace. */
  if ( lLength >= SMS_CWD_CAPACITY ||
       lNameLength >= SMS_CWD_CAPACITY - lLength - lSeparator ) return;
  _push_state ( apMenu );
  _fill (  apMenu, _STR( apMenu -> m_pCurrent ), 0  );
  _redraw ( apMenu, 0 );

 } else _context_action_file ( apMenu, 3 );

}  /* end _action_folder */

static void _context_action_fold ( GUIFileMenu* apMenu, int afPopup ) {

 _context_action_file ( apMenu, 2 );

}  /* end _context_action_fold */

static void _action_share ( GUIFileMenu* apMenu, int afPopup ) {

 smbOpenShare_in_t lOpen;
 char              lPath[ 2 ] __attribute__(   (  aligned( 4 )  )   );
 char*             lpPath = _STR( apMenu -> m_pCurrent );
 int               lRes;

 if ( g_SMBU >= 0 ) {

#ifdef BDM
  fileXioDevctl (
   g_pSMBS,
   SMB_DEVCTL_CLOSESHARE,
   NULL,
   0,
   NULL,
   0
  );
#endif

  g_SMBU = 0x80000000;

 }  /* end if */

 memset ( &lOpen, 0, sizeof ( lOpen ) );

 strncpy (
  lOpen.ShareName,
  lpPath,
  sizeof ( lOpen.ShareName ) - 1
 );

 {
  char* lpColon = strchr ( lOpen.ShareName, ':' );

  if ( lpColon ) *lpColon = '\x00';
 }

 lOpen.PasswordType = NO_PASSWORD;

#ifdef BDM
 lRes = fileXioDevctl (
  g_pSMBS,
  SMB_DEVCTL_OPENSHARE,
  &lOpen,
  sizeof ( lOpen ),
  NULL,
  0
 );
#else
 lRes = -999;
#endif

 if ( lRes == 0 ) {

  g_SMBU = 0;

  *( unsigned int* )g_CWD   = *( int* )g_pSMBS;
  g_CWD[ 4 ]                = '\x00';
  *( unsigned short* )lPath = 0x002F;

  _push_state ( apMenu );
  _fill ( apMenu, lPath, 0 );
  _redraw ( apMenu, 0 );

 } else {

  static char s_ErrShare[] __attribute__(
   ( section( ".data" ), aligned( 1 ) )
  ) = "SMB: SHARE NOT FOUND";

  static char s_ErrAccess[] __attribute__(
   ( section( ".data" ), aligned( 1 ) )
  ) = "SMB: ACCESS DENIED";

  static char s_ErrLogon[] __attribute__(
   ( section( ".data" ), aligned( 1 ) )
  ) = "SMB: LOGON FAILED";

  static char s_ErrPassword[] __attribute__(
   ( section( ".data" ), aligned( 1 ) )
  ) = "SMB: WRONG PASSWORD";

  static char s_ErrUser[] __attribute__(
   ( section( ".data" ), aligned( 1 ) )
  ) = "SMB: USERNAME NOT FOUND";

  static char s_ErrIO[] __attribute__(
   ( section( ".data" ), aligned( 1 ) )
  ) = "SMB: I/O ERROR (-5)";

  static char s_ErrUnknown[] __attribute__(
   ( section( ".data" ), aligned( 1 ) )
  ) = "SMB: UNKNOWN ERROR %08X";

  char*        lpError;
  char         lUnknown[ 40 ];
  unsigned int lStatus = ( unsigned int )lRes;

  if ( lStatus == 0xC00000CCU ) {

   lpError = s_ErrShare;

  } else if ( lStatus == 0xC0000022U ) {

   lpError = s_ErrAccess;

  } else if ( lStatus == 0xC000006DU ) {

   lpError = s_ErrLogon;

  } else if ( lStatus == 0xC000006AU ) {

   lpError = s_ErrPassword;

  } else if ( lStatus == 0xC0000064U ) {

   lpError = s_ErrUser;

  } else if ( lStatus == 0xFFFFFFFBU ) {

   lpError = s_ErrIO;

  } else {

   sprintf ( lUnknown, s_ErrUnknown, lStatus );
   lpError = lUnknown;

  }  /* end else */

  GUI_Error ( lpError );

 }  /* end else */
 
}  /* end _action_share */

static void _context_action_share ( GUIFileMenu* apMenu, int afPopup ) {

}  /* end _context_action_share */

static void ( *Action[ 9 ] ) ( GUIFileMenu*, int ) = {
 _action_folder, _action_avi,   _action_file,
 _action_file,   _action_file,  _action_partition,
 _action_avi,    _action_share, _action_file
};

static void ( *ContextAction[ 9 ] ) ( GUIFileMenu*, int ) = {
 _context_action_fold, _context_action_file, _context_action_file,
 _context_action_file, _context_action_file, _context_action_part,
 _context_action_file, _context_action_share, _context_action_file
};

static int _page_scroll (  int ( * ) ( GUIFileMenu*, int ), GUIFileMenu*  );
static int _step_up     ( GUIFileMenu*, int                               );

static int _step_down ( GUIFileMenu* apMenu, int afRoll ) {

 int retVal = 0;

 if ( apMenu -> m_pCurrent ) {

  if ( apMenu -> m_pCurrent -> m_pNext ) {

   apMenu -> m_pCurrent = apMenu -> m_pCurrent -> m_pNext;

   if ( apMenu -> m_pCurrent == apMenu -> m_pLast && apMenu -> m_pCurrent -> m_pNext )
    apMenu -> m_pFirst = apMenu -> m_pFirst -> m_pNext;
   else ++apMenu -> m_YOffset;

   retVal = 1;

  } else if ( afRoll ) while (  _page_scroll ( _step_up, apMenu )  );

 }  /* end if */

 return retVal;

}  /* end _step_down */

static int _step_up ( GUIFileMenu* apMenu, int afRoll ) {

 int retVal = 0;

 if ( apMenu -> m_pCurrent ) {

  if ( apMenu -> m_pCurrent -> m_pPrev ) {

   apMenu -> m_pCurrent = apMenu -> m_pCurrent -> m_pPrev;

   if ( apMenu -> m_pCurrent == apMenu -> m_pFirst && apMenu -> m_pCurrent -> m_pPrev )
    apMenu -> m_pFirst = apMenu -> m_pFirst -> m_pPrev;
   else --apMenu -> m_YOffset;

   retVal = 1;

  } else if ( afRoll ) while (  _page_scroll ( _step_down, apMenu )  );

 }  /* end if */

 return retVal;

}  /* end _step_up */

static int _page_scroll (  int ( *aFunc ) ( GUIFileMenu*, int ), GUIFileMenu* apMenu  ) {

 int           i, j;
 int           lHeight = g_GSCtx.m_Height - 101;
 int           lnItems = ( lHeight - 2 ) / 34;
 int           retVal  = 1;
 SMS_ListNode* lpNode;

 for ( i = 0; i < 10; ++i ) {

  j = lnItems;

  if (  !aFunc ( apMenu, 0 )  ) {

   retVal = 0;
   break;

  }  /* end if */

  lpNode = apMenu -> m_pFirst;

  while ( lpNode && j > 1 ) {

   apMenu -> m_pLast = lpNode;
   --j;
   lpNode = lpNode -> m_pNext;

  }  /* end while */

  if ( lpNode ) apMenu -> m_pLast = lpNode;

 }  /* end for */

 return retVal;

}  /* end _page_scroll */

static int GUIFileMenu_HandleEvent ( GUIObject* apObj, u64           anEvent ) {

 int          retVal   = GUIHResult_Void;
 GUIFileMenu* lpMenu   = ( GUIFileMenu* )apObj;
 int          lObjType;

 if ( lpMenu -> m_pCurrent )
  lObjType = ( unsigned int )lpMenu -> m_pCurrent -> m_Param >> 1;
 else lObjType = 0x80000000;

 if ( anEvent & GUI_MSG_PAD_MASK ) {

  int lBtn = anEvent & GUI_MSG_PAD_MASK;

  if ( lBtn == SMS_PAD_R2 ) {
   GUIFileMenu_Cleanup ( apObj );
   lpMenu -> m_ArtworkMode = !lpMenu -> m_ArtworkMode;
   if ( lpMenu -> m_ArtworkMode ) _artwork_position ( lpMenu );
   else { lpMenu -> m_pFirst = lpMenu -> m_pCurrent; lpMenu -> m_YOffset = 0; }
   _redraw ( lpMenu, 0 );
   return GUIHResult_Handled;
  }

  if ( lpMenu -> m_ArtworkMode ) {
   int lDelta = 0;
   if ( lBtn == SMS_PAD_LEFT ) {
    if ( lpMenu -> m_pCurrent == lpMenu -> m_pFiltList -> m_pHead ) return GUIHResult_ChangeFocus;
    lDelta = -1;
   } else if ( lBtn == SMS_PAD_RIGHT ) lDelta = 1;
   else if ( lBtn == SMS_PAD_UP ) lDelta = -SMS_ARTWORK_COLUMNS;
   else if ( lBtn == SMS_PAD_DOWN ) lDelta = SMS_ARTWORK_COLUMNS;
   else if ( lBtn == SMS_PAD_L1 || lBtn == RC_PREV ) lDelta = -SMS_ARTWORK_SLOTS;
   else if ( lBtn == SMS_PAD_L2 || lBtn == RC_NEXT ) lDelta = SMS_ARTWORK_SLOTS;
   if ( lDelta ) {
    _artwork_move ( lpMenu, lDelta );
    _redraw ( lpMenu, 0 );
    return GUIHResult_Handled;
   }
  }

  if ( lBtn == RC_TOP_MENU || lBtn == SMS_PAD_LEFT || lBtn == SMS_PAD_RIGHT ) {

   retVal = GUIHResult_ChangeFocus;

  } else if ( lBtn == RC_PREV || lBtn == SMS_PAD_L1 ) {

   _page_scroll (  _step_up, lpMenu );
   goto redraw;

  } else if ( lBtn == RC_NEXT || lBtn == SMS_PAD_L2 ) {

   _page_scroll (  _step_down, lpMenu );
   goto redraw;

  } else if ( lBtn == SMS_PAD_DOWN ) {

   _step_down ( lpMenu, 1 );
   goto redraw;

  } else if ( lBtn == SMS_PAD_UP ) {

   _step_up ( lpMenu, 1 );
redraw:
   _redraw ( lpMenu, 0 );

   retVal = GUIHResult_Handled;

  } else if (  lBtn == RC_ENTER || ( lObjType > 0 && lBtn == RC_PLAY ) || lBtn == SMS_PAD_CROSS  ) {

   if ( lObjType >= 0 ) {
    SPU_PlaySound ( SMSound_PAD, g_Config.m_PlayerVolume );
    Action[ lObjType ] ( lpMenu, 0 );
   }  /* end if */

   retVal = GUIHResult_Handled;

  } else if (  lBtn == RC_SUBTITLE || lBtn == SMS_PAD_CIRCLE || ( lObjType == 0 && lBtn == RC_PLAY )  ) {

   if ( lObjType >= 0 ) {
    SPU_PlaySound ( SMSound_PAD, g_Config.m_PlayerVolume );
    Action[ lObjType ] ( lpMenu, 1 );
   }  /* end if */

   retVal = GUIHResult_Handled;

  } else if (  lBtn == RC_DISPLAY || lBtn == ( SMS_PAD_R1 | SMS_PAD_CIRCLE )  ) {

   if (  ( g_CMedia == 2 && g_PD     >= 0 && g_CWD[ 0 ] != 'h' ) ||
         ( g_CMedia == 6 && g_SMBU   >= 0                      ) ||  /* smbman: share open == g_SMBU 0 */
         ( g_CMedia != 2 && g_CMedia != 6                      )
   ) _context_action_file ( lpMenu, 4 );

   retVal = GUIHResult_Handled;

  } else if ( lBtn == RC_PROGRAM || lBtn == SMS_PAD_SQUARE ) {

   if ( lObjType >= 0 ) ContextAction[ lObjType ] ( lpMenu, 1 );

   retVal = GUIHResult_Handled;

  } else if ( lBtn == RC_STOP || lBtn == RC_RETURN || lBtn == SMS_PAD_TRIANGLE ) {

   int lLevel = lpMenu -> m_pLevels -> m_Size;

   if ( lLevel ) {

    SMS_ListNode* lpState = lpMenu -> m_pLevels -> m_pTail;
    SMS_ListNode* lpNode;
    _LevelInfo*   lpInfo  = ( _LevelInfo* )( unsigned int )lpState -> m_Param;
    char          lCWD[ SMS_CWD_CAPACITY ] __attribute__(   (  aligned( 4 )  )   );
    char*         lpPtr;

    SPU_PlaySound ( SMSound_PAD, g_Config.m_PlayerVolume );

    if ( lLevel > 1 ) {

     _LevelInfo* lpInfoPrev  = ( _LevelInfo* )( unsigned int )lpState -> m_pPrev -> m_Param;
     int         lLen        = lpInfo -> m_Pos - lpInfoPrev -> m_Pos;

     strncpy ( lCWD, &g_CWD[ lpInfoPrev -> m_Pos ], lLen );
     lCWD [ lLen                ] = '\x00';
     g_CWD[ lpInfoPrev -> m_Pos ] = '\x00';
     lpPtr = lCWD;

    } else lpPtr = g_EmptyStr;

    _fill ( lpMenu, lpPtr, 0 );

    lpNode = SMS_ListFind (  lpMenu -> m_pFiltList, _STR( lpState )  );

    if ( lpNode ) {

     lpMenu -> m_pFirst   = lpNode;
     lpMenu -> m_pCurrent = SMS_ListFind ( lpMenu -> m_pFiltList, lpInfo -> m_pCurrent );
     lpMenu -> m_YOffset  = lpInfo -> m_YOffset;

    } else {

     lpMenu -> m_pFirst   =
     lpMenu -> m_pCurrent = lpMenu -> m_pFiltList -> m_pHead;
     lpMenu -> m_YOffset  = 0;

    }  /* end else */

    _pop_state ( lpMenu );
    _redraw ( lpMenu, 0 );

    if ( g_CMedia == 2 && !lpMenu -> m_pLevels -> m_Size ) g_Config.m_Partition[ 0 ] = '\x00';

   }  /* end if */

  }  /* end if */

 } else switch ( anEvent & GUI_MSG_USER_MASK ) {

  case GUI_MSG_MEDIA_SELECTED: {

   static int s_fHDDInit = 0;

   while ( lpMenu -> m_pLevels -> m_Size ) _pop_state ( lpMenu );

   if ( g_CMedia == 1 || g_CMedia == 5 ) CDVD_FlushCache ();

   _fill ( lpMenu, g_EmptyStr, anEvent & GUI_MSG_MENU_BIT );

   if ( g_CMedia == 2 && !s_fHDDInit && g_Config.m_Partition[ 0 ] ) {

    SMS_ListNode* lpMountNode = SMS_ListFind ( lpMenu -> m_pFiltList, g_Config.m_Partition );

    if ( lpMountNode ) {

     while ( lpMenu -> m_pCurrent != lpMountNode ) {

      lpMenu -> m_pCurrent = lpMenu -> m_pCurrent -> m_pNext;

      if ( lpMenu -> m_pCurrent == lpMenu -> m_pLast && lpMenu -> m_pCurrent -> m_pNext )
       lpMenu -> m_pFirst = lpMenu -> m_pFirst -> m_pNext;
      else ++lpMenu -> m_YOffset;

     }  /* end while */

     s_fHDDInit = 1;
     _action_partition ( lpMenu, 0 );

     return GUIHResult_Handled;

    }  /* end if */

   }  /* end if */

   _redraw ( lpMenu, anEvent & GUI_MSG_MENU_BIT );

   retVal = GUIHResult_Handled;

   if ( !lpMenu -> m_Active ) GUI_PostMessage ( SMS_PAD_DOWN );

  } break;

  case GUI_MSG_MEDIA_REMOVED: {

   _reset ( lpMenu, anEvent & GUI_MSG_MENU_BIT );

   retVal = GUIHResult_Handled;

  } break;

  case GUI_MSG_RELOAD_BROWSER:
   _fill ( lpMenu, g_DotStr, 0 );
  break;

  case GUI_MSG_REFILL_BROWSER: {

   if ( g_pFileList ) {

    GUIFileMenu_Cleanup ( apObj );
    SMS_ListNode* lpNode = g_pFileList -> m_pHead;
    SMS_ListDestroy ( lpMenu -> m_pFiltList, 0 );

    while ( lpNode ) {

     if (  !_filter_item ( lpNode )  ) SMS_ListPushBack (
      lpMenu -> m_pFiltList, _STR( lpNode )
     ) -> m_Param = lpNode -> m_Param;

     lpNode = lpNode -> m_pNext;

    }  /* end while */

    lpMenu -> m_pFirst   =
    lpMenu -> m_pCurrent = lpMenu -> m_pFiltList -> m_pHead;
    lpMenu -> m_YOffset  = 0;
    lpMenu -> m_pLast    = NULL;

   }  /* end if */

  } break;

 }  /* end switch */

 return retVal;

}  /* end GUIFileMenu_HandleEvent */

GUIObject* GUI_CreateFileMenu ( void ) {

 GUIFileMenu* retVal = ( GUIFileMenu* )calloc (  1, sizeof ( GUIFileMenu )  );

 retVal -> Render      = GUIFileMenu_Render;
 retVal -> HandleEvent = GUIFileMenu_HandleEvent;
 retVal -> SetFocus    = GUIFileMenu_SetFocus;
 retVal -> Cleanup     = GUIFileMenu_Cleanup;
 retVal -> m_pFileList = SMS_ListInit ();
 retVal -> m_pLevels   = SMS_ListInit ();
 retVal -> m_pFiltList = SMS_ListInit ();
 retVal -> m_pColor    = &g_Config.m_BrowserIBCIdx;
 retVal -> m_YOffset   = 0;

 g_PD   = 0x80000000;
 g_SMBU = 0x80000000;

 return ( GUIObject* )retVal;

}  /* end GUI_CreateFileMenu */
