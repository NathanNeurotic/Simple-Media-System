/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2007 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#include "SMS_GUIMenu.h"
#include "SMS_Config.h"
#include "SMS_Locale.h"
#include "SMS_FileDir.h"
#include "SMS_IOP.h"
#include "SMS_GUIcons.h"
#include "SMS_PgInd.h"
#include "SMS_GS.h"
#include "SMS_PAD.h"
#include "SMS_RC.h"
#include "SMS_SMB.h"

#include <malloc.h>
#include <string.h>
#include <fileio.h>
#include <fileXio_rpc.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct SMBInfo {

 SMString     m_Title;
 GUIMenuItem* m_pItems;
 SMString*    m_pStrings;
 int          m_CurIdx;

} SMBInfo;

extern void GUIMenuSMS_UpdateStatus ( GUIMenu*                  );
extern int  GUIMenuSMS_HandleEvent  ( GUIObject*, u64           );

static int GUIMenuSMB_HandleEvent ( GUIObject* apObj, u64           anEvent ) {

 return GUIMenuSMS_HandleEvent ( apObj, anEvent );

}  /* end GUIMenuSMB_HandleEvent */

static void _user_data_destructor ( void* apArg ) {

 SMBInfo* lpInfo = ( SMBInfo* )apArg;

 free ( lpInfo -> m_Title.m_pStr );
 free ( lpInfo -> m_pItems       );
 free ( lpInfo -> m_pStrings     );

}  /* end _user_data_destructor */

/* ----------------------------------------------------------------------------
 * In-GUI SMB server configuration (add / edit / delete + writer).
 * Lets the user create SMS.smb entries from scratch, with no hand-made file.
 * ------------------------------------------------------------------------- */

extern void SMS_SaveSMBInfo ( void );

void        _smb_menu   ( GUIMenu* apMenu );
static void _smb_reopen ( GUIMenu* apMenu );

/* The SMB server-editor field labels are now in the translatable string table
 * ( SMS_Locale, STR_SMB_* at indices 297+ ) -- no longer hardcoded here. */

/* working copy of the record being added / edited */
static SMBLoginInfo s_AddInfo  __attribute__(   (  section( ".bss" )  )   );
static char         s_AddIP[ 4 ][ 4 ] __attribute__(   (  section( ".bss" )  )   );
static char         s_AddPort[ 8 ]    __attribute__(   (  section( ".bss" )  )   );  /* port as typed text */
static char         s_AddDescr[ 64 ]  __attribute__(   (  section( ".bss" )  )   );

static SMS_ListNode* s_pEditNode __attribute__(   (  section( ".bss" )  )   );

/* SMString views for the form's right-hand value column */
static SMString s_StrAddIP  [ 4 ] __attribute__(   (  section( ".data" )  )   ) = {
 { 0, s_AddIP[ 0 ] }, { 0, s_AddIP[ 1 ] }, { 0, s_AddIP[ 2 ] }, { 0, s_AddIP[ 3 ] }
};
static SMString s_StrValName   __attribute__(   (  section( ".data" )  )   ) = { 0, s_AddInfo.m_ServerName };
static SMString s_StrValUser   __attribute__(   (  section( ".data" )  )   ) = { 0, s_AddInfo.m_UserName   };
static SMString s_StrValPass   __attribute__(   (  section( ".data" )  )   ) = { 0, s_AddInfo.m_Password   };
static SMString s_StrValPort   __attribute__(   (  section( ".data" )  )   ) = { 0, s_AddPort            };
static SMString s_StrValShare  __attribute__(   (  section( ".data" )  )   ) = { 0, s_AddInfo.m_Share    };

/* ----------------------------------------------------------------------------
 * On-screen keyboard ( GUI_TextInput )
 *
 * A self-contained, modal text-entry sub-screen rendered with the same GS
 * round-rect + GSFont primitives the rest of the GUI uses. The user navigates
 * a 2-D key grid with the D-pad ( UP / DOWN / LEFT / RIGHT, all of which the
 * input model delivers to a custom handler -- see GUIDevMenu_HandlePad ) and
 * types with Cross. Both letter cases, all digits and the common host / user /
 * password symbols are on-screen at once, so nothing needs paging.
 *
 *   Cross    : type the focused key ( or run SPACE / DEL / OK action keys )
 *   Circle   : backspace
 *   Square   : space ( shortcut )
 *   Triangle : commit ( same as the OK key )
 *   Start    : commit
 *   L1 / R1  : jump the cursor in the typed string
 *
 * The whole thing is driven by an in-place GUI_ReadButtons () poll loop (the
 * same approach GUI_WaitButtons / _wait_user use for the modal dialogs); on
 * exit a full GUI_Redraw restores the form underneath.
 * ------------------------------------------------------------------------- */

extern int ( *GUI_ReadButtons ) ( void );

/* The on-screen keyboard labels ( SPACE / DEL / OK / help ) are now in the translatable
 * string table ( SMS_Locale, STR_KB_* at indices 291..294 ) -- no longer hardcoded. */

/* The character grid. Every printable key the SMB login fields need is here at
 * once -- both cases, digits and the common symbols ( . _ - / @ : etc. ). The
 * rows are padded to KB_COLS so grid navigation is a simple ( row, col ). */
#define KB_COLS 13
#define KB_ROWS  7

static const char s_KbRows[ KB_ROWS ][ KB_COLS + 1 ] __attribute__(   (  section( ".rodata" )  )   ) = {
 "1234567890-_.",
 "qwertyuiop@:/",
 "asdfghjkl+=~?",
 "zxcvbnm,;'\"()",
 "QWERTYUIOP[]{",
 "ASDFGHJKL!#$%",
 "ZXCVBNM&*<>^}"
};

/* action keys sit on a final row */
#define KB_ACT_SPACE 0
#define KB_ACT_DEL   1
#define KB_ACT_OK    2
#define KB_ACT_COUNT 3

static char        s_KbBuf  [ 80 ] __attribute__(   (  section( ".bss" )  )   );  /* working copy of the text */
static char        s_KbCell [  2 ] __attribute__(   (  section( ".bss" )  )   );  /* one focused char + NUL   */

/* Draw one immediate frame of the keyboard on context 1, over whatever is on
 * screen ( the panel is opaque, so it fully repaints itself each frame ). */
static void _kb_render ( const char* apTitle, int aRow, int aCol, int aLen, int aCurs ) {

 int  lW   = g_GSCtx.m_Width  - ( g_GSCtx.m_Width >> 3 );
 int  lH   = g_GSCtx.m_Height - ( g_GSCtx.m_Height >> 3 );
 int  lX   = ( g_GSCtx.m_Width  - lW ) >> 1;
 int  lY   = ( g_GSCtx.m_Height - lH ) >> 1;
 int  lCW  = ( lW - 32 ) / KB_COLS;          /* key cell width  */
 int  lGY  = lY + 96;                         /* grid top        */
 int  lRH  = 30;                              /* key row height  */
 int  lGX  = lX + 16;
 int  i, lRow, lCol;
 u64* lpDMA;
/* Focus highlight. The panel body is the BrowserABCIdx palette colour, which defaults to
 * BLUE ( idx 10 -> g_Palette[ 9 ] = GS_SET_RGBAQ( 0x00, 0x00, 0xFF ) ), and the focus was
 * a single azure LINESTRIP ( 0x80FF8000 = RGB 0,128,255, aRad<0 in GS_RenderRoundRect =
 * outline, not fill ): a 1px near-blue hairline on a blue panel, so you could not tell
 * which letter you were on. Replaced by a dark FILL + a 2px AMBER outline -- amber is the
 * complement of blue, so it pops on the default theme. The glyph stays white because the
 * font colour is a CLUT index ( GS_SET_TEX0 ) and cannot be recoloured per-key, which is
 * exactly why the fill must be DARK rather than bright. Colours are 0xAABBGGRR. */
 u64  lSelRing = 0x8000D0FFUL;  /* amber,     RGB( 255, 208, 0 ) -- the focus outline  */
 u64  lSelFill = 0x80303030UL;  /* dark grey, RGB(  48,  48, 48 ) -- the focused cell  */

 GSContext_NewPacket ( 1, 0, GSPaintMethod_Init );

 /* opaque background panel ( shadow + body, like GUIMenu_Render ) */
 lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
 GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lX, lY, lW, lH, -12, 0x80008000UL  );
 lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
 GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lX, lY, lW, lH, 12, g_Palette[ g_Config.m_BrowserABCIdx - 1 ]  );

 /* title */
 g_GSCtx.m_TextColor = 1;
 i     = strlen ( apTitle );
 lpDMA = GSContext_NewPacket (  1, GS_TXT_PACKET_SIZE( i ), GSPaintMethod_Continue  );
 GSFont_RenderEx (  ( char* )apTitle, i, lX + 16, lY + 8, lpDMA, -2, 0  );

 /* typed-text box */
 lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
 GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lX + 12, lY + 40, lW - 24, 34, -8, 0x80303030UL  );

 if ( aLen ) {
  lpDMA = GSContext_NewPacket (  1, GS_TXT_PACKET_SIZE( aLen ), GSPaintMethod_Continue  );
  g_GSCtx.m_TextColor = 1;
  GSFont_RenderEx ( s_KbBuf, aLen, lX + 20, lY + 44, lpDMA, -2, 0 );
 }  /* end if */

 /* text cursor ( a thin bar after aCurs characters ) */
 {
  char lSave = s_KbBuf[ aCurs ];
  int  lCX;
  s_KbBuf[ aCurs ] = '\x00';
  lCX = lX + 20 + GSFont_WidthEx ( s_KbBuf, aCurs, -2 );
  s_KbBuf[ aCurs ] = lSave;
  lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
/* amber caret, 3px: matches the key-focus ring so "where you are" is one colour, and it
 * still contrasts hard against the dark ( 0x80303030 ) text box. */
  GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lCX, lY + 44, 3, 24, 0, lSelRing  );
 }

 /* key grid */
 for ( lRow = 0; lRow < KB_ROWS; ++lRow ) {

  int lKY = lGY + lRow * lRH;

  for ( lCol = 0; lCol < KB_COLS; ++lCol ) {

   char lCh = s_KbRows[ lRow ][ lCol ];
   int  lKX;

   if ( !lCh ) continue;

   lKX = lGX + lCol * lCW;

   if ( lRow == aRow && lCol == aCol ) {
/* The whole focused cell lights up, so it reads at TV distance:
 *  1. a DARK FILL ( +rad = fill ) -- the white glyph is more legible on it than on the
 *     blue panel, and the cell visibly changes state rather than gaining a hairline;
 *  2. an AMBER OUTLINE ( -rad = linestrip ) on the SAME rect -- GS_RenderRoundRect
 *     clamps both the same way so fill and outline corners align exactly;
 *  3. a second outline 1px out, making the ring ~2px and readable on a CRT. */
    lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
    GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lKX - 2, lKY - 2, lCW - 2, lRH - 2,  8, lSelFill  );
    lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
    GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lKX - 2, lKY - 2, lCW - 2, lRH - 2, -8, lSelRing  );
    lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
    GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lKX - 3, lKY - 3, lCW,     lRH,     -8, lSelRing  );
   }  /* end if */

   s_KbCell[ 0 ]       = lCh;
   s_KbCell[ 1 ]       = '\x00';
   g_GSCtx.m_TextColor = 1;
   lpDMA = GSContext_NewPacket (  1, GS_TXT_PACKET_SIZE( 1 ), GSPaintMethod_Continue  );
   GSFont_RenderEx ( s_KbCell, 1, lKX + ( lCW >> 1 ) - 4, lKY, lpDMA, -2, 0 );

  }  /* end for */

 }  /* end for */

 /* action row : SPACE | DEL | OK */
 {
  int      lAY  = lGY + KB_ROWS * lRH + 6;
  int      lAW  = ( lW - 48 ) / KB_ACT_COUNT;
  SMString* lpAct[ KB_ACT_COUNT ] = { &STR_KB_SPACE, &STR_KB_DEL, &STR_KB_OK };

  for ( i = 0; i < KB_ACT_COUNT; ++i ) {

   int lAX = lX + 16 + i * ( lAW + 4 );

   if ( aRow == KB_ROWS && aCol == i ) {
/* same dark fill + double amber outline as the key grid, so focus reads identically on
 * the action row ( the unselected buttons keep their plain dark outline, below ). */
    lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
    GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lAX, lAY, lAW, 30,  8, lSelFill  );
    lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
    GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lAX, lAY, lAW, 30, -8, lSelRing  );
    lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
    GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lAX - 1, lAY - 1, lAW + 2, 32, -8, lSelRing  );
   } else {
    lpDMA = GSContext_NewPacket (  1, GS_RRT_PACKET_SIZE (), GSPaintMethod_Continue  );
    GS_RenderRoundRect (  ( GSRoundRectPacket* )( lpDMA - 2 ), lAX, lAY, lAW, 30, -8, 0x80303030UL  );
   }  /* end else */

   g_GSCtx.m_TextColor = 1;
   lpDMA = GSContext_NewPacket (  1, GS_TXT_PACKET_SIZE( lpAct[ i ] -> m_Len ), GSPaintMethod_Continue  );
   GSFont_RenderEx (
    lpAct[ i ] -> m_pStr, lpAct[ i ] -> m_Len,
    lAX + ( lAW >> 1 ) - ( GSFont_WidthEx ( lpAct[ i ] -> m_pStr, lpAct[ i ] -> m_Len, -2 ) >> 1 ),
    lAY, lpDMA, -2, 0
   );

  }  /* end for */

  /* help line */
  g_GSCtx.m_TextColor = 2;
  lpDMA = GSContext_NewPacket (  1, GS_TXT_PACKET_SIZE( STR_KB_HELP.m_Len ), GSPaintMethod_Continue  );
  GSFont_RenderEx ( STR_KB_HELP.m_pStr, STR_KB_HELP.m_Len, lX + 16, lY + lH - 28, lpDMA, -2, 0 );

 }

 GSContext_Flush ( 1, GSFlushMethod_KeepLists );

}  /* end _kb_render */

/* Modal on-screen keyboard. Fills apBuf ( capacity aMaxLen including NUL ).
 * Returns 1 if committed ( OK / Triangle / Start ), 0 if cancelled. */
int GUI_TextInput ( char* apBuf, int aMaxLen, const char* apTitle ) {

 int lRow  = 1;   /* start on the lower-case home row */
 int lCol  = 0;
 int lCurs;
 int lMax  = aMaxLen - 1;
 int lBtn;
 int retVal = 0;

 if ( lMax > ( int )sizeof ( s_KbBuf ) - 1 ) lMax = ( int )sizeof ( s_KbBuf ) - 1;

 memset ( s_KbBuf, 0, sizeof ( s_KbBuf ) );
 strncpy ( s_KbBuf, apBuf, lMax );
 s_KbBuf[ lMax ] = '\x00';
 lCurs = ( int )strlen ( s_KbBuf );

 while (  GUI_ReadButtons ()  );  /* swallow the press that opened us */

 while ( 1 ) {

  int lLen = ( int )strlen ( s_KbBuf );

  _kb_render ( apTitle, lRow, lCol, lLen, lCurs );

  do { lBtn = GUI_ReadButtons (); } while ( !lBtn );

  switch ( lBtn ) {

   case SMS_PAD_UP:
   case RC_PREV:
    if ( --lRow < 0 ) lRow = KB_ROWS;
    if ( lRow == KB_ROWS && lCol >= KB_ACT_COUNT ) lCol = KB_ACT_COUNT - 1;
   break;

   case SMS_PAD_DOWN:
   case RC_NEXT:
    if ( ++lRow > KB_ROWS ) lRow = 0;
    if ( lRow == KB_ROWS && lCol >= KB_ACT_COUNT ) lCol = KB_ACT_COUNT - 1;
   break;

   case SMS_PAD_LEFT:
   case RC_LEFT: {
    int lMaxCol = ( lRow == KB_ROWS ? KB_ACT_COUNT : KB_COLS ) - 1;
    if ( --lCol < 0 ) lCol = lMaxCol;
   } break;

   case SMS_PAD_RIGHT:
   case RC_RIGHT: {
    int lMaxCol = ( lRow == KB_ROWS ? KB_ACT_COUNT : KB_COLS ) - 1;
    if ( ++lCol > lMaxCol ) lCol = 0;
   } break;

   case SMS_PAD_L1:
    if ( lCurs > 0 ) --lCurs;
   break;

   case SMS_PAD_R1:
    if ( lCurs < lLen ) ++lCurs;
   break;

   case SMS_PAD_CROSS:
   case RC_ENTER:
   case RC_PLAY:

    if ( lRow == KB_ROWS ) {  /* action key */

     if ( lCol == KB_ACT_OK ) { retVal = 1; goto done; }

     if ( lCol == KB_ACT_DEL ) {
      if ( lCurs > 0 ) {
       memmove ( &s_KbBuf[ lCurs - 1 ], &s_KbBuf[ lCurs ], lLen - lCurs + 1 );
       --lCurs;
      }  /* end if */
      break;
     }  /* end if */

     /* KB_ACT_SPACE falls through as a space insert */
     if ( lLen < lMax ) {
      memmove ( &s_KbBuf[ lCurs + 1 ], &s_KbBuf[ lCurs ], lLen - lCurs + 1 );
      s_KbBuf[ lCurs++ ] = ' ';
     }  /* end if */

    } else {  /* character key */

     char lCh = s_KbRows[ lRow ][ lCol ];

     if ( lCh && lLen < lMax ) {
      memmove ( &s_KbBuf[ lCurs + 1 ], &s_KbBuf[ lCurs ], lLen - lCurs + 1 );
      s_KbBuf[ lCurs++ ] = lCh;
     }  /* end if */

    }  /* end else */

   break;

   case SMS_PAD_CIRCLE:   /* backspace */
    if ( lCurs > 0 ) {
     memmove ( &s_KbBuf[ lCurs - 1 ], &s_KbBuf[ lCurs ], lLen - lCurs + 1 );
     --lCurs;
    }  /* end if */
   break;

   case SMS_PAD_SQUARE:   /* space shortcut */
    if ( lLen < lMax ) {
     memmove ( &s_KbBuf[ lCurs + 1 ], &s_KbBuf[ lCurs ], lLen - lCurs + 1 );
     s_KbBuf[ lCurs++ ] = ' ';
    }  /* end if */
   break;

   case SMS_PAD_TRIANGLE:
   case SMS_PAD_START:
   case RC_RETURN:
   case RC_STOP:
    retVal = 1;
   goto done;

  }  /* end switch */

  while (  GUI_ReadButtons ()  );  /* debounce : wait for release */

 }  /* end while */
done:
 while (  GUI_ReadButtons ()  );

 if ( retVal ) {
  strncpy ( apBuf, s_KbBuf, aMaxLen - 1 );
  apBuf[ aMaxLen - 1 ] = '\x00';
 }  /* end if */

 GUI_Redraw ( GUIRedrawMethod_Redraw );

 return retVal;

}  /* end GUI_TextInput */

/* ---- add / edit form ---- */

static void _addip_roll ( GUIMenu* apMenu, int aOctet, int aDir ) {

 int lNum = atoi ( s_AddIP[ aOctet ] );

 lNum += aDir;

 if ( lNum < 0 )
  lNum = 255;
 else if ( lNum > 255 ) lNum = 0;

 sprintf ( s_AddIP[ aOctet ], "%d", lNum );

 s_StrAddIP[ aOctet ].m_Len = strlen ( s_AddIP[ aOctet ] );

 apMenu -> Redraw ( apMenu );

}  /* end _addip_roll */

static void _addip1_handler ( GUIMenu* apMenu, int aDir ) { _addip_roll ( apMenu, 0, aDir ); }
static void _addip2_handler ( GUIMenu* apMenu, int aDir ) { _addip_roll ( apMenu, 1, aDir ); }
static void _addip3_handler ( GUIMenu* apMenu, int aDir ) { _addip_roll ( apMenu, 2, aDir ); }
static void _addip4_handler ( GUIMenu* apMenu, int aDir ) { _addip_roll ( apMenu, 3, aDir ); }

static void _addname_handler ( GUIMenu* apMenu, int aDir ) {

 GUI_TextInput (  s_AddInfo.m_ServerName, sizeof ( s_AddInfo.m_ServerName ), STR_SMB_SERVER_NAME.m_pStr  );
 s_StrValName.m_Len = strlen ( s_AddInfo.m_ServerName );

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _addname_handler */

static void _adduser_handler ( GUIMenu* apMenu, int aDir ) {

 GUI_TextInput (  s_AddInfo.m_UserName, sizeof ( s_AddInfo.m_UserName ), STR_SMB_USER_NAME.m_pStr  );
 s_StrValUser.m_Len = strlen ( s_AddInfo.m_UserName );

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _adduser_handler */

static void _addpass_handler ( GUIMenu* apMenu, int aDir ) {

 GUI_TextInput (  s_AddInfo.m_Password, sizeof ( s_AddInfo.m_Password ), STR_SMB_PASSWORD.m_pStr  );
 s_StrValPass.m_Len = strlen ( s_AddInfo.m_Password );

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _addpass_handler */

static void _addport_handler ( GUIMenu* apMenu, int aDir ) {

 /* TCP port for the direct-TCP smbman connection ( default 445 for the
  * PS2-Servers SMB1 host; 445 for a normal SMB server, 139 for NetBIOS ). */
 GUI_TextInput (  s_AddPort, sizeof ( s_AddPort ), STR_SMB_PORT.m_pStr  );

 if ( !s_AddPort[ 0 ] ) strcpy ( s_AddPort, "445" );

 s_StrValPort.m_Len = strlen ( s_AddPort );

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _addport_handler */

static void _addshare_handler ( GUIMenu* apMenu, int aDir ) {

 /* Optional: pre-set the share to open. Left empty, the browser lists the
  * server's shares ( GETSHARELIST ) and the user picks one. */
 GUI_TextInput (  s_AddInfo.m_Share, sizeof ( s_AddInfo.m_Share ), STR_SMB_SHARE.m_pStr  );
 s_StrValShare.m_Len = strlen ( s_AddInfo.m_Share );

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _addshare_handler */

static void _addsave_handler ( GUIMenu* apMenu, int aDir ) {

 SMBLoginInfo* lpInfo;
 char          lIP[ 16 ];

 sprintf (
  lIP, "%s.%s.%s.%s", s_AddIP[ 0 ], s_AddIP[ 1 ], s_AddIP[ 2 ], s_AddIP[ 3 ]
 );

 if (  !s_AddInfo.m_ServerName[ 0 ]  ) {

  GUI_Error ( STR_ERROR.m_pStr );
  return;

 }  /* end if */

 /* SMS_LoadSMBInfo silently drops records with strlen ( m_ServerName ) >= 14,
  * so a 14-15 char name would Save here yet vanish on next boot. Reject it. */
 if (  strlen ( s_AddInfo.m_ServerName ) >= 14  ) {

  GUI_Error ( STR_ERROR.m_pStr );
  return;

 }  /* end if */

 strcpy ( s_AddInfo.m_ServerIP, lIP );

 /* Make the just-added/edited server the active connect target. _lookup_login_info
  * matches the node whose m_ServerIP == g_Config.m_SMBIP (else falls back to the
  * list head), so without this a new server is never the login target. Mirror the
  * write in _smb_handler. */
 strcpy ( g_Config.m_SMBIP, s_AddInfo.m_ServerIP );

 if (  !s_AddInfo.m_ClientName[ 0 ]  ) strcpy ( s_AddInfo.m_ClientName, "PS2" );

 /* Persist the typed TCP port ( default 445 when empty / 0 ). */
 s_AddInfo.m_Port = atoi ( s_AddPort );
 if ( s_AddInfo.m_Port <= 0 || s_AddInfo.m_Port > 65535 ) s_AddInfo.m_Port = 445;

 strupr ( s_AddInfo.m_ServerName );

 s_AddInfo.m_fAsync = 1;

/* The server list has no separate editable description field, so its label
* should always follow the current Server Name. */
strcpy ( s_AddDescr, s_AddInfo.m_ServerName );

 if ( !g_Config.m_pSMBList ) g_Config.m_pSMBList = SMS_ListInit ();

 if ( s_pEditNode ) {

  lpInfo = ( SMBLoginInfo* )( unsigned int )s_pEditNode -> m_Param;

  memcpy (  lpInfo, &s_AddInfo, sizeof ( SMBLoginInfo )  );

  SMS_ListRemove ( g_Config.m_pSMBList, s_pEditNode );

  SMS_ListPushBack ( g_Config.m_pSMBList, s_AddDescr ) -> m_Param = ( unsigned int )lpInfo;

 } else {

  lpInfo = ( SMBLoginInfo* )malloc (  sizeof ( SMBLoginInfo )  );

  memcpy (  lpInfo, &s_AddInfo, sizeof ( SMBLoginInfo )  );

  SMS_ListPushBack ( g_Config.m_pSMBList, s_AddDescr ) -> m_Param = ( unsigned int )lpInfo;

 }  /* end else */

 g_IOPFlags              |= SMS_IOPF_SMBINFO;
 g_Config.m_NetworkFlags |= SMS_DF_SMB;

 SMS_SaveSMBInfo ();

 GUI_MenuPopState ( apMenu );  /* leave the form         */
 _smb_reopen     ( apMenu );  /* rebuild the server list */

}  /* end _addsave_handler */

static GUIMenuItem s_AddMenu[ 10 ] __attribute__(   (  section( ".data" )  )   ) = {
 { MENU_ITEM_TYPE_TEXT, &STR_PS2_IP1,          0, 0, _addip1_handler,  0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_PS2_IP2,          0, 0, _addip2_handler,  0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_PS2_IP3,          0, 0, _addip3_handler,  0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_PS2_IP4,          0, 0, _addip4_handler,  0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_SMB_SERVER_NAME,  0, 0, _addname_handler, 0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_SMB_USER_NAME,    0, 0, _adduser_handler, 0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_SMB_PASSWORD,     0, 0, _addpass_handler, 0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_SMB_PORT,         0, 0, _addport_handler, 0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_SMB_SHARE,        0, 0, _addshare_handler,0, 0 },
 { 0,                   &STR_SAVE_SETTINGS,    0, 0, _addsave_handler, 0, 0 }
};

static void _smb_addedit_form ( GUIMenu* apMenu, SMS_ListNode* apNode ) {

 int           i;
 GUIMenuState* lpState;

 s_pEditNode = apNode;

 memset (  &s_AddInfo, 0, sizeof ( SMBLoginInfo )  );
 s_AddDescr[ 0 ] = '\x00';

 if ( apNode ) {

  SMBLoginInfo* lpSrc = ( SMBLoginInfo* )( unsigned int )apNode -> m_Param;

  memcpy (  &s_AddInfo, lpSrc, sizeof ( SMBLoginInfo )  );

  strncpy (  s_AddDescr, _STR( apNode ), sizeof ( s_AddDescr ) - 1  );

  i = sscanf (
   lpSrc -> m_ServerIP, "%3[^.].%3[^.].%3[^.].%3s",
   s_AddIP[ 0 ], s_AddIP[ 1 ], s_AddIP[ 2 ], s_AddIP[ 3 ]
  );

  if ( i != 4 ) {
   strcpy ( s_AddIP[ 0 ], "0" );
   strcpy ( s_AddIP[ 1 ], "0" );
   strcpy ( s_AddIP[ 2 ], "0" );
   strcpy ( s_AddIP[ 3 ], "0" );
  }  /* end if */

 } else {

  strcpy ( s_AddInfo.m_ClientName, "PS2" );

  strcpy ( s_AddIP[ 0 ], "192" );
  strcpy ( s_AddIP[ 1 ], "168" );
  strcpy ( s_AddIP[ 2 ], "0"   );
  strcpy ( s_AddIP[ 3 ], "1"   );

 }  /* end else */

 /* Port: default 445 ( PS2-Servers ) for a new server or an old record whose
  * m_Port is 0 / unset. */
 sprintf (  s_AddPort, "%d", s_AddInfo.m_Port ? s_AddInfo.m_Port : 445  );

 for ( i = 0; i < 4; ++i ) s_StrAddIP[ i ].m_Len = strlen ( s_AddIP[ i ] );

 s_StrValName.m_Len   = strlen ( s_AddInfo.m_ServerName );
 s_StrValUser.m_Len   = strlen ( s_AddInfo.m_UserName   );
 s_StrValPass.m_Len   = strlen ( s_AddInfo.m_Password   );
 s_StrValPort.m_Len   = strlen ( s_AddPort             );
 s_StrValShare.m_Len  = strlen ( s_AddInfo.m_Share     );

 for ( i = 0; i < 4; ++i ) s_AddMenu[ i ].m_IconRight = ( unsigned int )&s_StrAddIP[ i ];

s_AddMenu[ 4 ].m_IconRight = ( unsigned int )&s_StrValName;
s_AddMenu[ 5 ].m_IconRight = ( unsigned int )&s_StrValUser;
s_AddMenu[ 6 ].m_IconRight = ( unsigned int )&s_StrValPass;
s_AddMenu[ 7 ].m_IconRight = ( unsigned int )&s_StrValPort;
s_AddMenu[ 8 ].m_IconRight = ( unsigned int )&s_StrValShare;
s_AddMenu[ 9 ].m_IconRight = GUICON_SAVE;

 lpState = GUI_MenuPushState ( apMenu );

 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_AddMenu;
 lpState -> m_pLast  = &s_AddMenu[ 9 ];
 lpState -> m_pTitle = &STR_SMB_EDIT_TITLE;

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _smb_addedit_form */

static void _smb_handler ( GUIMenu* apMenu, int aDir ) {

 GUIMenuState* lpState = (  ( GUIMenuState* )( unsigned int )apMenu -> m_pState -> m_pTail -> m_Param  );
 SMS_ListNode* lpNode  = SMS_ListFind ( g_Config.m_pSMBList, lpState -> m_pCurr -> m_pOptionName -> m_pStr );
 SMBLoginInfo* lpInfo  = ( SMBLoginInfo* )( unsigned int )lpNode -> m_Param;

 if (  strcmp ( lpInfo -> m_ServerIP, g_Config.m_SMBIP )  ) {

  SMBInfo* lpMenuInfo = ( SMBInfo* )lpState -> m_pUserData;

  lpMenuInfo -> m_pItems[ lpMenuInfo -> m_CurIdx ].m_IconRight = 0;
  lpState -> m_pCurr -> m_IconRight = GUICON_ON;
  lpMenuInfo -> m_CurIdx            = lpState -> m_pCurr - lpMenuInfo -> m_pItems;

  strcpy ( g_Config.m_SMBIP, lpInfo -> m_ServerIP );

  if ( g_IOPFlags & SMS_IOPF_NET_UP ) {

   /* smbman is synchronous + single-connection: there is no in-progress connect
    * to STOPC. If a session is up, close any open share + LOGOFF first, then
    * trigger a fresh synchronous logon to the newly selected server via the
    * 0x18 ( GUI_MSG_LOGIN ) connect action ( -> _smb_logon ). */
   if ( g_IOPFlags & SMS_IOPF_SMBLOGIN ) {

    GUI_Status ( STR_SMB_CLOSING.m_pStr );
    SMS_PgIndStart ();
#ifdef BDM
     if ( g_SMBU >= 0 ) fileXioDevctl (  g_pSMBS, SMB_DEVCTL_CLOSESHARE, NULL, 0, NULL, 0  );
     fileXioDevctl (  g_pSMBS, SMB_DEVCTL_LOGOFF, NULL, 0, NULL, 0  );   /* smbman SMB is BDM-only */
#endif
    SMS_PgIndStop ();

    g_SMBU      = 0x80000000;
    g_IOPFlags &= ~SMS_IOPF_SMBLOGIN;
    /* Removing the smb: device itself re-posts the 0x18 login ( see
     * SMS_GUIDevMenu.c, lDevID == 6 ), so do NOT post a second login here --
     * doing so logs on twice and adds a duplicate smb: device entry. */
    GUI_PostMessage ( GUI_MSG_SMB );

   } else {

    GUI_PostMessage ( GUI_MSG_MOUNT_BIT | GUI_MSG_LOGIN );

   }  /* end else */

   GUIMenuSMS_UpdateStatus ( apMenu );

  }  /* end if */

  apMenu -> Redraw ( apMenu );

 }  /* end if */

}  /* end _smb_handler */

static SMS_ListNode* _smb_active_node ( void ) {

 SMS_ListNode* lpNode;

 if (  !g_Config.m_pSMBList || !g_Config.m_pSMBList -> m_Size  ) return NULL;

 lpNode = g_Config.m_pSMBList -> m_pHead;

 while ( lpNode ) {

  SMBLoginInfo* lpInfo = ( SMBLoginInfo* )( unsigned int )lpNode -> m_Param;

  if (  !strcmp ( lpInfo -> m_ServerIP, g_Config.m_SMBIP )  ) return lpNode;

  lpNode = lpNode -> m_pNext;

 }  /* end while */

 return g_Config.m_pSMBList -> m_pHead;

}  /* end _smb_active_node */

static void _smb_reopen ( GUIMenu* apMenu ) {

 /* The current top state is the server list. It stashed the parent menu's
  * event handler (GUIMenuSMS_HandleEvent) in m_pState. Restore it before we
  * pop + rebuild so the fresh server list re-installs the SMB handler over a
  * clean parent (otherwise back-navigation would chain the SMB handler). */
 GUIMenuState* lpState = (  ( GUIMenuState* )( unsigned int )apMenu -> m_pState -> m_pTail -> m_Param  );

 int (  *lpParentEvent ) ( GUIObject*, u64  ) = lpState -> HandleEvent;

 GUI_MenuPopState ( apMenu );  /* drop the (now stale) server list */

 if ( lpParentEvent ) apMenu -> HandleEvent = lpParentEvent;

 _smb_menu ( apMenu );

}  /* end _smb_reopen */

static void _smb_add_handler ( GUIMenu* apMenu, int aDir ) {

 _smb_addedit_form ( apMenu, NULL );

}  /* end _smb_add_handler */

static void _smb_edit_handler ( GUIMenu* apMenu, int aDir ) {

 SMS_ListNode* lpNode = _smb_active_node ();

 if ( lpNode ) _smb_addedit_form ( apMenu, lpNode );

}  /* end _smb_edit_handler */

static void _smb_delete_handler ( GUIMenu* apMenu, int aDir ) {

 SMS_ListNode* lpNode = _smb_active_node ();

 if ( !lpNode ) return;

 g_Config.m_SMBIP[ 0 ] = '\x00';

 free (  ( void* )( unsigned int )lpNode -> m_Param  );
 SMS_ListRemove ( g_Config.m_pSMBList, lpNode );

 if ( !g_Config.m_pSMBList -> m_Size ) {
  g_IOPFlags              &= ~SMS_IOPF_SMBINFO;
  g_Config.m_NetworkFlags &= ~SMS_DF_SMB;
 }  /* end if */

 SMS_SaveSMBInfo ();

 _smb_reopen ( apMenu );

}  /* end _smb_delete_handler */

void _smb_menu ( GUIMenu* apMenu ) {

 int           i, lnItems, lnTotal, lFound;
 GUIMenuState* lpState = GUI_MenuPushState ( apMenu );
 SMBInfo*      lpInfo  = ( SMBInfo* )malloc (  sizeof ( SMBInfo )  );
 SMS_ListNode* lpNode  = g_Config.m_pSMBList ? g_Config.m_pSMBList -> m_pHead : NULL;

 lnItems = g_Config.m_pSMBList ? g_Config.m_pSMBList -> m_Size : 0;

 /* server rows + "Add server..." + ( "Edit..." + "Delete" when non-empty ) */
 lnTotal = lnItems + ( lnItems ? 3 : 1 );

 lpInfo -> m_Title.m_pStr = ( char* )calloc ( 1, i = STR_SMB_SERVER.m_Len - 2 );
 strncpy ( lpInfo -> m_Title.m_pStr, STR_SMB_SERVER.m_pStr, --i );
 lpInfo -> m_Title.m_Len  = i;

 lpInfo -> m_pItems   = ( GUIMenuItem* )calloc (  lnTotal, sizeof ( GUIMenuItem )  );
 lpInfo -> m_pStrings = ( SMString*    )calloc (  lnItems ? lnItems : 1, sizeof ( SMString )  );
 lpInfo -> m_CurIdx   = 0;
 lFound               = 0;

 for ( i = 0; i < lnItems; ++i, lpNode = lpNode -> m_pNext ) {

  lpInfo -> m_pItems[ i ].m_pOptionName = lpInfo -> m_pStrings + i;
  lpInfo -> m_pItems[ i ].Handler       = _smb_handler;

  lpInfo -> m_pStrings[ i ].m_pStr = _STR( lpNode );
  lpInfo -> m_pStrings[ i ].m_Len  = strlen (  _STR( lpNode )  );

  if (  !strcmp (
          g_Config.m_SMBIP, (  ( SMBLoginInfo* )( unsigned int )lpNode -> m_Param  ) -> m_ServerIP
         )
  ) {
   lpInfo -> m_CurIdx                  = i;
   lpInfo -> m_pItems[ i ].m_IconRight = GUICON_ON;
   lFound                              = 1;
  }  /* end if */

 }  /* end for */

 if ( lnItems && !lFound ) lpInfo -> m_pItems -> m_IconRight = GUICON_ON;

 lpInfo -> m_pItems[ i   ].m_pOptionName = &STR_SMB_ADD_SERVER;
 lpInfo -> m_pItems[ i   ].m_IconRight   = GUICON_FOLDER;
 lpInfo -> m_pItems[ i++ ].Handler       = _smb_add_handler;

 if ( lnItems ) {

  lpInfo -> m_pItems[ i   ].m_pOptionName = &STR_SMB_EDIT_SERVER;
  lpInfo -> m_pItems[ i++ ].Handler       = _smb_edit_handler;

  lpInfo -> m_pItems[ i   ].m_pOptionName = &STR_DELETE;
  lpInfo -> m_pItems[ i++ ].Handler       = _smb_delete_handler;

 }  /* end if */

 lpState -> m_pItems           =
 lpState -> m_pFirst           =
 lpState -> m_pCurr            =  lpInfo -> m_pItems;
 lpState -> m_pLast            =  lpInfo -> m_pItems + lnTotal - 1;
 lpState -> m_pTitle           = &lpInfo -> m_Title;
 lpState -> m_pUserData        =  lpInfo;
 lpState -> UserDataDestructor =  _user_data_destructor;

 lpState -> HandleEvent = apMenu -> HandleEvent;
 apMenu  -> HandleEvent = GUIMenuSMB_HandleEvent;

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _smb_menu */

