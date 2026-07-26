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

static SMS_List* s_pHst;

static char s_pHistory[] __attribute__(   (  aligned( 1 ), section( ".data" )  )   ) = "SMS/SMS.hst";

void SMS_HistoryLoad ( void ) {

 char lP[ 128 ];
 int  lFD;

 s_pHst = SMS_ListInit ();

 if (  SMS_ConfigAssetPath ( lP, sizeof ( lP ), "SMS.hst" )  ) {   /* CWD copy next to the ELF */

  lFD = fioOpen ( lP, O_RDONLY );

  if ( lFD >= 0 ) {

   while ( 1 ) {

    unsigned short lSize;
    SMS_ListNode*  lpNode;

    if (  fioRead ( lFD, &lSize, 2 ) != 2  ) break;

    lpNode = SMS_ListPushBackBuf ( s_pHst, lSize + 1 );
    fioRead ( lFD, _STR( lpNode ),     lSize );
    fioRead ( lFD, &lpNode -> m_Param, 8     );

   }  /* end while */

   fioClose ( lFD );
   return;

  }  /* end if */

 }  /* end if */

 /* memory-card fallback */
#ifdef BDM
/* fio, NOT libmc ( same fix as config / palette / locale / skin ): historical
 * SMS.hst files may have been created through fio, so read the same path through
 * fio for compatibility. History is no longer persisted by this build. */
 {
  char lMC[ 5 + sizeof ( s_pHistory ) ];

  lMC[ 0 ] = 'm'; lMC[ 1 ] = 'c'; lMC[ 2 ] = ( char )( '0' + g_MCSlot ); lMC[ 3 ] = ':'; lMC[ 4 ] = '/';
  strcpy ( &lMC[ 5 ], s_pHistory );   /* "mc0:/" + "SMS/SMS.hst" */

  lFD = fioOpen ( lMC, O_RDONLY );
 }

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
#else
 lFD = MC_OpenS ( g_MCSlot, 0, s_pHistory, O_RDONLY );   /* non-BDM: synchronous wrappers, real fd */

 if ( lFD < 0 ) return;

 while ( 1 ) {

  unsigned short lSize;
  SMS_ListNode*  lpNode;

  if (  MC_ReadS ( lFD, &lSize, 2 ) == 2  ) {
   lpNode = SMS_ListPushBackBuf ( s_pHst, lSize + 1 );
   MC_ReadS (  lFD, _STR( lpNode ), lSize  );
   MC_ReadS (  lFD, &lpNode -> m_Param, 8  );
  } else break;

 }  /* end while */

 MC_CloseS ( lFD );
#endif

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
 /* Do not persist resume history. On filesystem boots this used to create SMS.hst
  * next to the ELF, including on MMCE devices, where SMS must not write history. */

}  /* end SMS_HistorySave */

int SMS_HistoryRemove ( const char* apPath ) {

 SMS_ListNode* lpNode;

 if (   SMS_HistoryLook (  apPath, ( void** )&lpNode  ) == -1   ) return 0;

 SMS_ListRemove ( s_pHst, lpNode );

 return 1;

}  /* end SMS_HistoryRemove */
