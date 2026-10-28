/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2006 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#include "SMS_RingBuffer.h"

#include <kernel.h>
#include <malloc.h>
#include <string.h>
#ifdef SMS_DIAG
#include "SMS_Timer.h"
#include <stdio.h>
#endif

SMS_RingBuffer* SMS_RingBufferInit ( void* apBuffer, int aSize ) {

 SMS_RingBuffer* retVal = ( SMS_RingBuffer* )malloc (  sizeof ( SMS_RingBuffer )  );
 ee_sema_t       lSema;

 retVal -> m_pInp     =
 retVal -> m_pOut     =
 retVal -> m_pBeg     = apBuffer;
 retVal -> m_pEnd     =
 retVal -> m_pJmp     = retVal -> m_pBeg + aSize;
 retVal -> m_Size     =
 retVal -> m_Capacity = aSize;
 retVal -> m_fWait    = 0;
 retVal -> WaitCB     = NULL;
 retVal -> BlockCB    = NULL;
 retVal -> m_nRef     = 1;
#ifdef SMS_DIAG
 retVal -> m_WaitEvents = retVal -> m_WaitMs = retVal -> m_MaxWaitMs = 0;
 retVal -> m_pDiagName = "unnamed";
#endif

 lSema.init_count = 0;
 retVal -> m_Sema[ 0 ] = CreateSema ( &lSema );
 retVal -> m_Sema[ 1 ] = CreateSema ( &lSema );
 lSema.init_count = 1;
 retVal -> m_Sema[ 2 ] = CreateSema ( &lSema );
 return retVal;

}  /* end SMS_RingBufferInit */

SMS_RingBuffer* SMS_RingBufferAddRef ( SMS_RingBuffer* apRB ) {

 WaitSema ( apRB -> m_Sema[ 2 ] );
  ++apRB -> m_nRef;
 SignalSema ( apRB -> m_Sema[ 2 ] );

 return apRB;

}  /* end SMS_RingBufferAddRef */

void SMS_RingBufferDestroy ( SMS_RingBuffer* apRB ) {

 if ( !--apRB -> m_nRef ) {
#ifdef SMS_DIAG
  /* Includes startup, pause and seek waits: compare like-for-like playback
   * runs. A wait alone is not proof of a storage underrun. */
  printf ( "[sms-queue] name=%s buffer=%p capacity=%d waits=%u wait_ms=%u max_wait_ms=%u\n",
   apRB -> m_pDiagName, apRB, apRB -> m_Capacity, apRB -> m_WaitEvents, apRB -> m_WaitMs, apRB -> m_MaxWaitMs );
#endif

  DeleteSema ( apRB -> m_Sema[ 0 ] );
  DeleteSema ( apRB -> m_Sema[ 1 ] );
  DeleteSema ( apRB -> m_Sema[ 2 ] );

  free ( apRB );

 }  /* end if */

}  /* end SMS_RingBufferDestroy */

void* SMS_RingBufferAlloc ( SMS_RingBuffer* apRB, int aSize ) {

 void* retVal  = NULL;
 int   lfAvail = 0;

 WaitSema ( apRB -> m_Sema[ 2 ] );

 aSize = ( aSize + 63 ) & 0xFFFFFFC0;

 if ( aSize <= apRB -> m_Capacity ) {

  do {

   while ( aSize > apRB -> m_Size ) {

    if ( apRB -> BlockCB ) apRB -> BlockCB ( apRB );

    apRB -> m_fWait = aSize;

    SignalSema ( apRB -> m_Sema[ 2 ] );
    WaitSema ( apRB -> m_Sema[ 0 ] );
    WaitSema ( apRB -> m_Sema[ 2 ] );

   }  /* end while */

   if ( apRB -> m_pInp + aSize > apRB -> m_pEnd ) {

    apRB -> m_pJmp  = apRB -> m_pInp;
    apRB -> m_pInp  = apRB -> m_pBeg;
    apRB -> m_Size -= apRB -> m_pEnd - apRB -> m_pJmp;

   } else lfAvail = 1;

  } while ( !lfAvail );

  retVal = apRB -> m_pPtr = apRB -> m_pInp;
  apRB -> m_pInp += aSize;
  apRB -> m_Size -= aSize;

 }  /* end if */

 SignalSema ( apRB -> m_Sema[ 2 ] );

 return retVal;

}  /* end SMS_RingBufferAlloc */

void SMS_RingBufferFree ( SMS_RingBuffer* apRB, int aSize ) {

 WaitSema ( apRB -> m_Sema[ 2 ] );

 aSize = ( aSize + 63 ) & 0xFFFFFFC0;

 if ( apRB -> m_pOut + aSize == apRB -> m_pJmp ) {

  apRB -> m_pOut  = apRB -> m_pBeg;
  apRB -> m_Size += apRB -> m_pEnd - apRB -> m_pJmp;
  apRB -> m_pJmp  = apRB -> m_pEnd;

 } else apRB -> m_pOut += aSize;

 apRB -> m_Size += aSize;

 if ( apRB -> m_fWait ) {
  apRB -> m_fWait = 0;
  SignalSema ( apRB -> m_Sema[ 0 ] );
 }  /* end if */

 SignalSema ( apRB -> m_Sema[ 2 ] );

}  /* end SMS_RingBufferFree */

void SMS_RingBufferReset ( SMS_RingBuffer* apRB ) {

 WaitSema ( apRB -> m_Sema[ 2 ] );

 apRB -> m_pInp  =
 apRB -> m_pOut  = apRB -> m_pBeg;
 apRB -> m_pJmp  = apRB -> m_pEnd;
 apRB -> m_Size  = apRB -> m_pEnd - apRB -> m_pBeg;
 apRB -> m_fWait = 0;
 apRB -> BlockCB = NULL;

 while (  PollSema ( apRB -> m_Sema[ 0 ] ) >= 0 );
 while (  PollSema ( apRB -> m_Sema[ 1 ] ) >= 0 );

 SignalSema ( apRB -> m_Sema[ 2 ] );

}  /* end SMS_RingBufferReset */

void SMS_RingBufferUnalloc ( SMS_RingBuffer* apRB, int aSize ) {

 WaitSema ( apRB -> m_Sema[ 2 ] );

 aSize = ( aSize + 63 ) & 0xFFFFFFC0;
 apRB -> m_pInp  = apRB -> m_pPtr;
 apRB -> m_Size += aSize;

 SignalSema ( apRB -> m_Sema[ 2 ] );

}  /* end SMS_RingBufferUnalloc */

void* SMS_RingBufferWait ( SMS_RingBuffer* apRB ) {
#ifdef SMS_DIAG
 unsigned int lStart, lMs;
#endif

 if ( apRB -> WaitCB ) apRB -> WaitCB ( apRB );

#ifdef SMS_DIAG
 lStart = ( unsigned int )*( volatile u64* )&g_Timer;
#endif
 WaitSema ( apRB -> m_Sema[ 1 ] );
#ifdef SMS_DIAG
 lMs = ( unsigned int )*( volatile u64* )&g_Timer - lStart;
 if ( lMs ) {
  ++apRB -> m_WaitEvents;
  apRB -> m_WaitMs += lMs;
  if ( lMs > apRB -> m_MaxWaitMs ) apRB -> m_MaxWaitMs = lMs;
 }
#endif
 WaitSema ( apRB -> m_Sema[ 2 ] );

 if ( apRB -> m_pOut == apRB -> m_pJmp ) {
  apRB -> m_pOut  = apRB -> m_pBeg;
  apRB -> m_Size += apRB -> m_pEnd - apRB -> m_pJmp;
  apRB -> m_pJmp  = apRB -> m_pEnd;
 }  /* end if */

 SignalSema ( apRB -> m_Sema[ 2 ] );

 return apRB -> m_pOut;

}  /* end SMS_RingBufferWait */

int SMS_RingBufferCount ( SMS_RingBuffer* apRB ) {

 ee_sema_t lSema;

 ReferSemaStatus ( apRB -> m_Sema[ 1 ], &lSema );

 return lSema.count;

}  /* end SMS_RingBufferCount */
