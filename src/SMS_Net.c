/* Sockets for the EE side of SMS, through the smsnet IOP module. See SMS_Net.h.
 * Licensed under the Academic Free License version 2.0. */
#include "SMS.h"
#include "SMS_Net.h"
#include "SMS_IOP.h"
#include "SMS_SIF.h"
#include "SMS_Locale.h"

#include "smsnet.h"

#include <kernel.h>
#include <sifrpc.h>
#include <string.h>

#ifdef BDM

static SifRpcClientData_t s_Client __attribute__(   (  aligned( 64 )  )   );
static unsigned char      s_Buf[ ( SMSNET_BUFSIZE + 63 ) & ~63 ] __attribute__(   (  aligned( 64 )  )   );

#define HDR_OUT (  ( SMSNetHdr* )s_Buf                  )
#define HDR_IN  (  ( SMSNetHdr* )UNCACHED_SEG( s_Buf )  )

/* The reply is written to memory behind the cache's back, so it is read uncached. */
static int _call ( int aCmd, int aSend, int aRecv ) {

 return SifCallRpc (
         &s_Client, aCmd, 0, s_Buf, sizeof ( SMSNetHdr ) + aSend,
         s_Buf, sizeof ( SMSNetHdr ) + aRecv, NULL, NULL
        ) >= 0;

}  /* end _call */

int SMS_NetStart ( void ) {

 int lRes = SMS_IOPStartInet ();

 if ( lRes <= 0 ) return lRes;

 if ( lRes == 2 ) s_Client.server = NULL;   /* a new module instance: forget the old binding */

 return SIF_BindRPC ( &s_Client, SMSNET_RPC_ID ) ? 1 : 0;

}  /* end SMS_NetStart */

static unsigned int _dotted ( const char* apStr ) {

 unsigned char lIP[ 4 ];
 unsigned int  retVal;
 int           i;

 for ( i = 0; i < 4; ++i ) {

  int lVal = 0, lDigits = 0;

  while ( *apStr >= '0' && *apStr <= '9' && lDigits++ < 3 ) lVal = lVal * 10 + ( *apStr++ - '0' );

  if (  !lDigits || lVal > 255 || *apStr != ( i < 3 ? '.' : '\x00' )  ) return 0;

  lIP[ i ] = ( unsigned char )lVal;
  ++apStr;

 }  /* end for */

 memcpy ( &retVal, lIP, 4 );

 return retVal;

}  /* end _dotted */

static unsigned int _ask ( unsigned int aServer, const char* apName ) {

 int lLen = strlen ( apName ) + 1;

 if ( !aServer || lLen > 128 ) return 0;

 HDR_OUT -> m_IP = aServer;
 memcpy ( s_Buf + sizeof ( SMSNetHdr ), apName, lLen );

 return _call ( SMSNET_RESOLVE, lLen, 0 ) ? HDR_IN -> m_IP : 0;

}  /* end _ask */

unsigned int SMS_NetResolve ( const char* apName ) {

 static const char* s_Public[ 2 ] = { "1.1.1.1", "8.8.8.8" };

 unsigned int retVal;
 int          lRound, i;

/* Two rounds: right after the adapter starts, the link can take a few seconds to come up,
 * and every question asked before that goes nowhere. */
 for ( lRound = 0; lRound < 2; ++lRound ) {

  if (   (  retVal = _ask ( _dotted ( g_pDefGW ), apName )  )   ) return retVal;

  for ( i = 0; i < 2; ++i ) if (   (  retVal = _ask ( _dotted ( s_Public[ i ] ), apName )  )   ) return retVal;

 }  /* end for */

 return 0;

}  /* end SMS_NetResolve */

int SMS_NetConnect ( unsigned int anIP, int aPort ) {

 HDR_OUT -> m_IP  = anIP;
 HDR_OUT -> m_Len = aPort;

 return _call ( SMSNET_CONNECT, 0, 0 ) ? HDR_IN -> m_Sock : -1;

}  /* end SMS_NetConnect */

int SMS_NetSend ( int aSock, const void* apData, int aLen ) {

 const unsigned char* lpData = ( const unsigned char* )apData;
 int                  lSent  = 0;

 while ( lSent < aLen ) {

  int lPart = aLen - lSent > SMSNET_CHUNK ? SMSNET_CHUNK : aLen - lSent;

  HDR_OUT -> m_Sock = aSock;
  HDR_OUT -> m_Len  = lPart;
  memcpy (  s_Buf + sizeof ( SMSNetHdr ), lpData + lSent, lPart  );

  if (  !_call ( SMSNET_SEND, lPart, 0 ) || HDR_IN -> m_Len != lPart  ) return -1;

  lSent += lPart;

 }  /* end while */

 return lSent;

}  /* end SMS_NetSend */

int SMS_NetRecv ( int aSock, void* apData, int aLen, int aTimeoutMs ) {

 int lGot;

 if ( aLen > SMSNET_CHUNK ) aLen = SMSNET_CHUNK;

 HDR_OUT -> m_Sock = aSock;
 HDR_OUT -> m_Len  = aLen;
 HDR_OUT -> m_Tmo  = aTimeoutMs;

 if (  !_call ( SMSNET_RECV, 0, aLen )  ) return -1;

 lGot = HDR_IN -> m_Len;

 if ( lGot > aLen ) return -1;

 if ( lGot > 0 ) memcpy (  apData, ( unsigned char* )UNCACHED_SEG( s_Buf ) + sizeof ( SMSNetHdr ), lGot  );

 return lGot;

}  /* end SMS_NetRecv */

void SMS_NetClose ( int aSock ) {

 HDR_OUT -> m_Sock = aSock;

 _call ( SMSNET_CLOSE, 0, 0 );

}  /* end SMS_NetClose */

#endif  /* BDM */
