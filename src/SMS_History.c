/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2005-2009 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#include "SMS.h"
#include "SMS_History.h"
#include "SMS_List.h"
#include "SMS_MC.h"
#include "SMS_Config.h"

#include <fileio.h>
#include <string.h>

#define HIST_SIZE 32

extern char g_pBootDir[];   /* SMS_Config.c: "<dev>/path/" on a non-mc boot, empty on mc */

static SMS_List* s_pHst;

static char s_pHistory[] __attribute__(   (  aligned( 1 ), section( ".data" )  )   ) = "SMS/SMS.hst";

void SMS_HistoryLoad ( void ) {

 char lP[ 128 ];
 int  lFD;

 s_pHst = SMS_ListInit ();

/* CWD copy next to the ELF if present, else the memory-card copy -- BOTH read via
 * fio. The mc read MUST be fio ( not libmc ): SMS_HistorySave writes via fio, and on
 * the iomanX+mcman stack libmc cannot see the fio-written file, so resume history
 * was saved but never loaded back on an mc boot ( same class as the SMS.cfg fix ). */
 if (  !SMS_ConfigAssetPath ( lP, sizeof ( lP ), "SMS.hst" )  ) {
  lP[ 0 ] = 'm';
  lP[ 1 ] = 'c';
  lP[ 2 ] = '0' + g_MCSlot;
  lP[ 3 ] = ':';
  lP[ 4 ] = '/';
  strcpy ( &lP[ 5 ], s_pHistory );   /* "mc<slot>:/SMS/SMS.hst" -- same path SMS_HistorySave writes */
 }  /* end if */

 lFD = fioOpen ( lP, O_RDONLY );

 if ( lFD < 0 ) return;

 while ( 1 ) {

  unsigned short lSize;
  SMS_ListNode*  lpNode;

  if (  fioRead ( lFD, &lSize, 2 ) != 2  ) break;

  lpNode = SMS_ListPushBackBuf ( s_pHst, lSize + 1 );
  fioRead ( lFD, _STR( lpNode ),     lSize );
  fioRead ( lFD, &lpNode -> m_Param, 8     );

 }  /* end while */

 fioClose ( lFD );

}  /* end SMS_HistoryLoad */

s64  SMS_HistoryLook ( const char* apPath, void** appNode ) {

 s64           retVal = -1;
 SMS_ListNode* lpNode = s_pHst -> m_pHead;

 while ( lpNode ) {
  if (   !strcmp (  apPath, _STR( lpNode )  )   ) {
   retVal = lpNode -> m_Param;
   if ( appNode ) appNode[ 0 ] = lpNode;
   break;
  }  /* end if */
  lpNode = lpNode -> m_pNext;
 }  /* end while */

 return retVal;

}  /* end SMS_HistoryLook */

void SMS_HistoryAdd ( const char* apPath, s64  aPTS ) {

 SMS_ListNode* lpNode;

 if (   SMS_HistoryLook (  apPath, ( void** )&lpNode  ) != -1   )
  lpNode -> m_Param = aPTS;
 else {
  SMS_ListPushBack ( s_pHst, apPath ) -> m_Param = aPTS;
  if ( s_pHst -> m_Size > HIST_SIZE ) SMS_ListPop ( s_pHst );
 }  /* end else */

}  /* end SMS_HistoryAdd */

void SMS_HistorySave ( void ) {

 int  lFD;
 char lPath[ 128 ];

 if ( g_pBootDir[ 0 ] ) {   /* CWD boot -> save next to the ELF */
  strcpy ( lPath, g_pBootDir );
  strcat ( lPath, "SMS.hst" );
 } else {
  lPath[ 0 ] = 'm';
  lPath[ 1 ] = 'c';
  lPath[ 2 ] = '0' + g_MCSlot;
  lPath[ 3 ] = ':';
  lPath[ 4 ] = '/';
  strcpy ( &lPath[ 5 ], s_pHistory );
 }  /* end else */

 lFD = fioOpen ( lPath, O_CREAT | O_WRONLY );

 if ( lFD >= 0 ) {

  SMS_ListNode* lpNode = s_pHst -> m_pHead;

  while ( lpNode ) {
   unsigned short lLen = strlen (  _STR( lpNode )  );
   fioWrite (  lFD, &lLen,              2 );
   fioWrite (  lFD, _STR( lpNode ), lLen  );
   fioWrite (  lFD, &lpNode -> m_Param, 8 );
   lpNode = lpNode -> m_pNext;
  }  /* end while */

  fioClose ( lFD );

 }  /* end if */

}  /* end SMS_HistorySave */

int SMS_HistoryRemove ( const char* apPath ) {

 SMS_ListNode* lpNode;

 if (   SMS_HistoryLook (  apPath, ( void** )&lpNode  ) == -1   ) return 0;

 SMS_ListRemove ( s_pHst, lpNode );

 return 1;

}  /* end SMS_HistoryRemove */
