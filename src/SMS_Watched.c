/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
/* WATCHED MARKERS ( requested: mark a video as watched once it has been seen to the end, so a
 * series can be followed episode by episode from the browser ).
 *
 * Same storage model as SMS_History.c ( resume points ), kept in its own file so neither feature
 * can evict the other: SMS.wtc next to the ELF on a CWD boot, otherwise mc?:/SMS/SMS.wtc. Each
 * record is a 2-byte length followed by the full media path ( the FileContext m_pPath the player
 * saw, i.e. what _make_path in SMS_GUIFileMenu.c produces ). Oldest entries drop off past
 * WTC_SIZE; re-watching a file moves it to the newest end.
 *
 * HDD: every PFS partition is mounted on the same g_pPFS prefix, so a bare path cannot tell
 * "Episode01.avi" on one partition from the same relative path on another. Paths under g_pPFS
 * are therefore keyed as "<partition>|<path>" ( g_Config.m_Partition, set by _action_partition
 * when the partition is mounted ). The player records and the browser looks up through the same
 * _key, so both sides always agree. */
#include "SMS.h"
#include "SMS_Watched.h"
#include "SMS_List.h"
#include "SMS_MC.h"
#include "SMS_Config.h"
#include "SMS_Locale.h"

#include <fileio.h>
#include <string.h>

#define WTC_SIZE 256

extern char g_pBootDir[];   /* SMS_Config.c: "<dev>/path/" on a non-mc boot, empty on mc */

static SMS_List* s_pWtc;

static char s_pWatched[] __attribute__(   (  aligned( 1 ), section( ".data" )  )   ) = "SMS/SMS.wtc";

static void _read_fio ( int aFD ) {

 while ( 1 ) {

  unsigned short lSize;
  SMS_ListNode*  lpNode;

  if (  fioRead ( aFD, &lSize, 2 ) != 2 || !lSize  ) break;

  lpNode = SMS_ListPushBackBuf ( s_pWtc, lSize + 1 );   /* calloc'ed -> always NUL-terminated */

  if (  fioRead ( aFD, _STR( lpNode ), lSize ) != lSize  ) {
   SMS_ListPopBack ( s_pWtc );
   break;
  }  /* end if */

 }  /* end while */

}  /* end _read_fio */

void SMS_WatchedLoad ( void ) {

 char lP[ 128 ];
 int  lFD;

 s_pWtc = SMS_ListInit ();

 if (  SMS_ConfigAssetPath ( lP, sizeof ( lP ), "SMS.wtc" )  ) {   /* CWD copy next to the ELF */

  lFD = fioOpen ( lP, O_RDONLY );

  if ( lFD >= 0 ) {
   _read_fio ( lFD );
   fioClose ( lFD );
   return;
  }  /* end if */

 }  /* end if */

 /* memory-card fallback ( mirrors SMS_HistoryLoad ) */
#ifdef BDM
 {
  char lMC[ 5 + sizeof ( s_pWatched ) ];

  lMC[ 0 ] = 'm'; lMC[ 1 ] = 'c'; lMC[ 2 ] = ( char )( '0' + g_MCSlot ); lMC[ 3 ] = ':'; lMC[ 4 ] = '/';
  strcpy ( &lMC[ 5 ], s_pWatched );

  lFD = fioOpen ( lMC, O_RDONLY );
 }

 if ( lFD < 0 ) return;

 _read_fio ( lFD );
 fioClose  ( lFD );
#else
 lFD = MC_OpenS ( g_MCSlot, 0, s_pWatched, O_RDONLY );

 if ( lFD < 0 ) return;

 while ( 1 ) {

  unsigned short lSize;
  SMS_ListNode*  lpNode;

  if (  MC_ReadS ( lFD, &lSize, 2 ) != 2 || !lSize  ) break;

  lpNode = SMS_ListPushBackBuf ( s_pWtc, lSize + 1 );

  if (  MC_ReadS ( lFD, _STR( lpNode ), lSize ) != lSize  ) {
   SMS_ListPopBack ( s_pWtc );
   break;
  }  /* end if */

 }  /* end while */

 MC_CloseS ( lFD );
#endif

}  /* end SMS_WatchedLoad */

static const char* _key ( const char* apPath ) {

 static char s_Key[ 1024 + sizeof ( g_Config.m_Partition ) + 2 ];

 unsigned int lPFS = strlen ( g_pPFS );

 if (  !apPath || !g_Config.m_Partition[ 0 ] || strncmp ( apPath, g_pPFS, lPFS )  ) return apPath;
 if (  strlen ( g_Config.m_Partition ) + strlen ( apPath ) + 2 > sizeof ( s_Key )  ) return apPath;

 strcpy ( s_Key, g_Config.m_Partition );
 strcat ( s_Key, "|"                  );
 strcat ( s_Key, apPath               );

 return s_Key;

}  /* end _key */

static SMS_ListNode* _find ( const char* apPath ) {

 SMS_ListNode* lpNode;

 apPath = _key ( apPath );

 if ( !s_pWtc || !apPath ) return NULL;

 for ( lpNode = s_pWtc -> m_pHead; lpNode; lpNode = lpNode -> m_pNext )
  if (   !strcmp (  apPath, _STR( lpNode )  )   ) return lpNode;

 return NULL;

}  /* end _find */

int SMS_WatchedLook ( const char* apPath ) {

 return _find ( apPath ) != NULL;

}  /* end SMS_WatchedLook */

void SMS_WatchedAdd ( const char* apPath ) {

 SMS_ListNode* lpNode;

 if ( !s_pWtc || !apPath || !apPath[ 0 ] ) return;

 if (  ( lpNode = _find ( apPath ) )  ) {
  if ( lpNode == s_pWtc -> m_pTail ) return;   /* already the newest entry */
  SMS_ListRemove ( s_pWtc, lpNode );
 }  /* end if */

 apPath = _key ( apPath );

 if (  strlen ( apPath ) > 0xFFFF  ) return;

 SMS_ListPushBack ( s_pWtc, apPath );

 while ( s_pWtc -> m_Size > WTC_SIZE ) SMS_ListPop ( s_pWtc );

}  /* end SMS_WatchedAdd */

int SMS_WatchedRemove ( const char* apPath ) {

 SMS_ListNode* lpNode = _find ( apPath );

 if ( !lpNode ) return 0;

 SMS_ListRemove ( s_pWtc, lpNode );

 return 1;

}  /* end SMS_WatchedRemove */

void SMS_WatchedSave ( void ) {

 int  lFD;
 char lPath[ 128 ];

 if ( !s_pWtc ) return;

 if ( g_pBootDir[ 0 ] ) {   /* CWD boot -> save next to the ELF */
  if (  strlen ( g_pBootDir ) + 8 > sizeof ( lPath )  ) return;
  strcpy ( lPath, g_pBootDir );
  strcat ( lPath, "SMS.wtc" );
 } else {
  lPath[ 0 ] = 'm';
  lPath[ 1 ] = 'c';
  lPath[ 2 ] = '0' + g_MCSlot;
  lPath[ 3 ] = ':';
  lPath[ 4 ] = '/';
  strcpy ( &lPath[ 5 ], s_pWatched );
 }  /* end else */

 /* O_TRUNC: variable-length records, see the note in SMS_HistorySave. */
 lFD = fioOpen ( lPath, O_CREAT | O_WRONLY | O_TRUNC );

 if ( lFD >= 0 ) {

  SMS_ListNode* lpNode;

  for ( lpNode = s_pWtc -> m_pHead; lpNode; lpNode = lpNode -> m_pNext ) {
   unsigned short lLen = strlen (  _STR( lpNode )  );
   fioWrite (  lFD, &lLen,          2 );
   fioWrite (  lFD, _STR( lpNode ), lLen );
  }  /* end for */

  fioClose ( lFD );

 }  /* end if */

}  /* end SMS_WatchedSave */
