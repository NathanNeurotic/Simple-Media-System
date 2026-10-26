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
#include "SMS_GUIMenu.h"
#include "SMS_FileContext.h"
#include "SMS_Player.h"
#include "SMS_Locale.h"
#include "SMS_CDVD.h"
#include "SMS_CDDA.h"
#include "SMS_FileDir.h"
#include "SMS_GUIcons.h"
#include "SMS_SubtitleContext.h"
#include "SMS_GS.h"
#include "SMS_RC.h"
#include "SMS_PAD.h"
#include "SMS_Config.h"
#include "SMS_Timer.h"

#include <malloc.h>
#include <string.h>

extern SMS_Player s_Player;

extern void** SMS_OpenMediaFile ( const char*, int );
extern char   g_LastPlayed[];   /* SMS_GUIFileMenu.c -- basename of the track now playing */

static void _start_player   ( FileContext*, FileContext*, SubtitleFormat );
static void _start_playlist ( SMS_List*                                  );

static void GUICmdProc_Render ( GUIObject* apObj, int aCtx ) {

}  /* end GUICmdProc_Render */

static void GUICmdProc_Cleanup ( GUIObject* apObj ) {

}  /* end GUICmdProc_Cleanup */

extern void RestoreFileDir ( void** );

static int GUICmdProc_HandleEvent ( GUIObject* apObj, u64           anEvent ) {

 int           retVal = GUIHResult_Void;
 u64           lEvent = anEvent & 0xF000000000000000L;

 if ( lEvent == GUI_MSG_FILE || lEvent == GUI_MSG_FOLDER_MP3 ) {

  void** lpParam = ( void** )( unsigned int )(  ( anEvent & 0x0FFFFFFFFFFFFFFFL ) >> 28  );

  if (   (  ( unsigned int )lpParam[ 0 ] & 0xC0000000  ) != 0xC0000000   )
   _start_player (  ( FileContext* )lpParam[ 0 ], ( FileContext* )lpParam[ 1 ], ( SubtitleFormat )lpParam[ 2 ]  );
  else _start_playlist (
   ( SMS_List* )(   (  ( unsigned int )lpParam[ 0 ] << 2  ) >> 2   )
  );

  g_CDDASpeed = 4;

  GUI_UpdateStatus ();

  if ( lEvent == GUI_MSG_FILE )

   free ( lpParam );

  else RestoreFileDir ( lpParam );

  retVal = GUIHResult_Handled;

  GSFont_Unload ();

 }  /* end if */

 return retVal;

}  /* end GUICmdProc_HandleEvent */

GUIObject* GUI_CreateCmdProc ( void ) {

 GUIObject* retVal = ( GUIObject* )calloc (  1, sizeof ( GUIObject )  );

 retVal -> Render      = GUICmdProc_Render;
 retVal -> Cleanup     = GUICmdProc_Cleanup;
 retVal -> HandleEvent = GUICmdProc_HandleEvent;

 return retVal;

}  /* end GUI_CreateCmdProc */

void _start_player ( FileContext* apFileCtx, FileContext* apSubCtx, SubtitleFormat aSubFmt ) {

 SMS_Player* lpPlayer = SMS_InitPlayer ( apFileCtx, apSubCtx, aSubFmt );

 if ( !lpPlayer ) {

  if ( g_CMedia == 1 ) CDVD_Stop ();

  GUI_Error ( s_Player.m_pErrorMsg -> m_pStr );

 } else {

/* CONTINUOUS PLAYBACK ( requested: "listen to the songs without having to tap them one by one" ).
 * SMS could already do this, but ONLY via the file context menu -> Play all -> Audio, which
 * builds a filtered folder list and runs it through _start_playlist. Picking a single track
 * played exactly that track and dropped back to the browser -- so the feature existed and was
 * simply undiscoverable. Playing one song now continues into the rest of the folder, which is
 * what every music player does.
 *
 * Deliberately mirrors _start_playlist's loop instead of inventing a second playback path:
 * Play() ALREADY returns SMS_FLAGS_USER_STOP ( SMS_Player.c ), i.e. it distinguishes "the
 * track ended" from "the user stopped it" -- this call site was just discarding the return.
 * So the rule is: advance on a natural end, stop dead on a user stop ( BACK / STOP must still
 * mean back to the browser, never "play the next one" ).
 *
 * AUDIO ONLY, by design -- the current row must itself be GUICON_MP3. Auto-advancing videos
 * would be a surprise, and the next file after a video is usually unrelated.
 * SMS_OpenMediaFile rewrites g_LastPlayed each time, so the loop walks forward naturally and
 * cannot revisit a track. Every exit path falls through to the same GUI_Initialize( 0 ), so
 * the browser is restored exactly as before whichever way the loop ends. */
  int lfUserStop = lpPlayer -> Play ( lpPlayer );

  lpPlayer -> Destroy ( lpPlayer );

  while ( !lfUserStop ) {

   SMS_ListNode* lpNode;
   void**        lpParam;

   if ( !g_pFileList ) break;

   lpNode = SMS_ListFindI ( g_pFileList, g_LastPlayed );

   if (  !lpNode || ( int )lpNode -> m_Param != GUICON_MP3  ) break;   /* audio only */

/* SHUFFLE (#25): the "Randomize playlist" toggle ( SMS_PF_RAND, the MP3
 * settings page ) already shuffles M3U playlists; extend it to continuous
 * folder playback so the next track is a random audio entry other than the
 * one that just played. Pure random per pick -- an immediate repeat becomes
 * possible late in a folder, the same tradeoff the M3U shuffle makes. */
   if ( g_Config.m_PlayerFlags & SMS_PF_RAND ) {

    unsigned int  lnCandidates = 0, lPick, lIdx = 0;
    SMS_ListNode* lpCurrent = lpNode;
    SMS_ListNode* lpScan;

    g_RandSeed = ( int )g_Timer;

    for ( lpScan = g_pFileList -> m_pHead; lpScan; lpScan = lpScan -> m_pNext )
     if (  ( int )lpScan -> m_Param == GUICON_MP3 && lpScan != lpCurrent  ) ++lnCandidates;

    if ( !lnCandidates ) break;

    lPick = ( unsigned int )SMS_rand () % lnCandidates;
    lpNode = NULL;

    for ( lpScan = g_pFileList -> m_pHead; lpScan; lpScan = lpScan -> m_pNext )
     if (   ( int )lpScan -> m_Param == GUICON_MP3 && lpScan != lpCurrent &&
            lIdx++ == lPick   ) {
      lpNode = lpScan;
      break;
     }  /* end if */

    if ( !lpNode ) break;   /* defensive; lPick < lnCandidates guarantees a hit */

   } else {

    do {
     lpNode = lpNode -> m_pNext;
    } while (  lpNode && ( int )lpNode -> m_Param != GUICON_MP3  );

    if ( !lpNode ) break;   /* last track in the folder */

   }  /* end else */

   GUI_Initialize ( -1 );

   lpParam = SMS_OpenMediaFile (  _STR( lpNode ), SMS_FA_FLAGS_AVI  );

   if ( !lpParam ) break;

   lpPlayer = SMS_InitPlayer (
    ( FileContext* )lpParam[ 0 ], ( FileContext* )lpParam[ 1 ], ( SubtitleFormat )lpParam[ 2 ]
   );

   free ( lpParam );

   if ( !lpPlayer ) break;

   lfUserStop = lpPlayer -> Play ( lpPlayer );

   lpPlayer -> Destroy ( lpPlayer );

  }  /* end while */

  GUI_Initialize ( 0 );

 }  /* end else */

}  /* end _start_player */

static int _playlist_menu ( SMS_List*, SMS_ListNode** );

static void _start_playlist ( SMS_List* apList ) {

 SMS_ListNode* lpNode = apList -> m_pHead;

/* SHUFFLE (#25): "Randomize playlist" ( SMS_PF_RAND, the MP3 settings page )
 * already shuffles M3U playlists and the continuous-audio path; extend it to
 * Play-all folders -- VIDEO included, which is what ambient playback wants.
 * Fisher-Yates over the node pointers, then relink in place: node storage ( and
 * so each item's inline string, _STR ) moves with the pointer, nothing is freed
 * or reallocated. */
 if (  ( g_Config.m_PlayerFlags & SMS_PF_RAND ) && apList -> m_Size > 1  ) {

  unsigned int   lnStm = apList -> m_Size, i;
  SMS_ListNode** lpNodes = ( SMS_ListNode** )malloc ( lnStm * sizeof ( SMS_ListNode* ) );

  if ( lpNodes ) {

   g_RandSeed = ( int )g_Timer;

   for ( i = 0; i < lnStm; ++i ) lpNodes[ i ] = SMS_ListAt ( apList, i );

   for ( i = lnStm - 1; i > 0; --i ) {

    unsigned int  j   = ( unsigned int )SMS_rand () % ( i + 1 );
    SMS_ListNode* lpT = lpNodes[ i ];

    lpNodes[ i ] = lpNodes[ j ];
    lpNodes[ j ] = lpT;

   }  /* end for */

   apList -> m_pHead = lpNodes[ 0         ];
   apList -> m_pTail = lpNodes[ lnStm - 1 ];

   for ( i = 0; i < lnStm; ++i ) {

    lpNodes[ i ] -> m_pPrev = i          ? lpNodes[ i - 1 ] : NULL;
    lpNodes[ i ] -> m_pNext = i + 1 < lnStm ? lpNodes[ i + 1 ] : NULL;

   }  /* end for */

   free ( lpNodes );

  }  /* end if */

 }  /* end if */

 if (  !_playlist_menu ( apList, &lpNode )  ) {

  while ( lpNode ) {

   int lfUserStop = 0;

   if ( !lpNode -> m_Param ) {

    void**      lpParam  = SMS_OpenMediaFile (  _STR( lpNode ), SMS_FA_FLAGS_AVI  );
    SMS_Player* lpPlayer = SMS_InitPlayer (
     ( FileContext* )lpParam[ 0 ], ( FileContext* )lpParam[ 1 ], ( SubtitleFormat )lpParam[ 2 ]
    );

    if ( lpPlayer ) {

     lfUserStop = lpPlayer -> Play ( lpPlayer );
     lpPlayer -> Destroy ( lpPlayer );

     GUI_Initialize ( -1 );

    }  /* end if */

   }  /* end if */

   if ( lfUserStop && lpNode -> m_pNext ) {
    if (  _playlist_menu ( apList, &lpNode )  ) break;
   } else lpNode = lpNode -> m_pNext;

  }  /* end while */

  GUI_Initialize ( 0 );

 } else GUI_Redraw ( GUIRedrawMethod_Redraw );

}  /* end _start_playlist */

extern int ( *CtxMenu_HandleEventBase ) ( GUIObject*, u64           );
extern int CtxMenu_HandleEvent          ( GUIObject*, u64           );

static GUIMenu* s_pMenu;

static void _file_handler ( GUIMenu* apMenu, int aDir ) {


}  /* end _file_handler */

static void _playlist_menu_redraw ( GUIMenu* apMenu ) {

 GSContext_NewPacket ( 1, 0, GSPaintMethod_Init );
 s_pMenu -> Render (  ( GUIObject* )s_pMenu, 1  );

 GS_VSync ();
 GSContext_Flush ( 1, GSFlushMethod_KeepLists );

}  /* end _playlist_menu_redraw */

static int _playlist_menu ( SMS_List* apList, SMS_ListNode** appNode ) {

 int           lCount  = 0;
 SMS_ListNode* lpNode  = apList -> m_pHead;
 GUIMenu*      lpMenu  = ( GUIMenu* )GUI_CreateMenu ();
 int           lWidth  = g_GSCtx.m_Width  - ( g_GSCtx.m_Width  >> 2 );
 int           lHeight = g_GSCtx.m_Height - ( g_GSCtx.m_Height >> 2 );
 char*         lpSel   = NULL;
 GUIMenuItem*  lpItems;
 GUIMenuState* lpState;
 SMString*     lpNames;

 CtxMenu_HandleEventBase = lpMenu -> HandleEvent;
 s_pMenu                 = lpMenu;

 lpMenu -> m_Color      = 0x80301010UL;
 lpMenu -> m_X          = ( g_GSCtx.m_Width  - lWidth  ) >> 1;
 lpMenu -> m_Y          = (  ( g_GSCtx.m_Height - lHeight ) >> 1  ) + 8;
 lpMenu -> m_Width      = lWidth;
 lpMenu -> m_Height     = lHeight;
 lpMenu -> m_pActiveObj = g_pActiveNode;
 lpMenu -> m_pState     = SMS_ListInit ();
 lpMenu -> m_IGroup     = GUIcon_Browser;
 lpMenu -> Redraw       = _playlist_menu_redraw;
 lpMenu -> HandleEvent  = CtxMenu_HandleEvent;

 while ( lpNode ) {
  if ( !lpNode -> m_Param ) {
   ++lCount;
   if (   !strcmp (  _STR( lpNode ), _STR( appNode[ 0 ] )  )   ) lpSel = _STR( lpNode );
  }  /* end if */
  lpNode = lpNode -> m_pNext;
 }  /* end while */

 lpNode  = apList -> m_pHead;
 lpItems = ( GUIMenuItem* )calloc (  lCount, sizeof ( GUIMenuItem )  );
 lpNames = ( SMString*    )calloc (  lCount, sizeof ( SMString    )  );
 lCount  = 0;

 while ( lpNode ) {
  if ( !lpNode -> m_Param ) {
   lpNames[ lCount ].m_Len         = strlen (  _STR( lpNode )  );
   lpNames[ lCount ].m_pStr        = _STR( lpNode );
   lpItems[ lCount ].m_pOptionName = &lpNames[ lCount ];
   lpItems[ lCount ].m_IconLeft    = GUICON_AVI;
   lpItems[ lCount ].Handler       = _file_handler;
   ++lCount;
  }  /* end if */
  lpNode = lpNode -> m_pNext;
 }  /* end while */

 lpState = GUI_MenuPushState ( lpMenu );
 lpState -> m_pTitle = &STR_SELECT_FILE;
 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = lpItems;
 lpState -> m_pLast  = &lpItems[ lCount - 1 ];

 GUIMenu_SelectItemByName ( lpMenu, lpSel );

 GUI_AddObject (  STR_SELECT_FILE.m_pStr, ( GUIObject* )lpMenu  );

 lpMenu -> Redraw ( lpMenu );

 GUI_Run ();

 appNode[ 0 ] = SMS_ListFind ( apList, lpState -> m_pCurr -> m_pOptionName -> m_pStr );

 GUI_DeleteObject ( STR_SELECT_FILE.m_pStr );

 free ( lpNames );
 free ( lpItems );

 return ( int )lpMenu -> m_pUserData;

}  /* end _playlist_menu */
