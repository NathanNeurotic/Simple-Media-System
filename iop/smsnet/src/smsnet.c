/* smsnet: socket service for the EE side of SMS. See include/smsnet.h.
 * Licensed under the Academic Free License version 2.0. */
#include "types.h"
#include "defs.h"
#include "irx.h"
#include "loadcore.h"
#include "sifcmd.h"
#include "sysclib.h"
#include "thbase.h"
#include "ps2ip.h"

#include "smsnet.h"
#include "dns.h"

#define MODNAME "smsnet"

IRX_ID( MODNAME, 1, 0 );

static SifRpcDataQueue_t  s_Queue;
static SifRpcServerData_t s_Server;
static unsigned char      s_Buf[ SMSNET_BUFSIZE ] __attribute__(   (  aligned( 16 )  )   );
static unsigned int       s_QueryID = 0x5153;

/* 1 when the socket has data ( or was closed ) before the time runs out, 0 otherwise. */
static int _wait ( int aSock, int aMs ) {

 fd_set         lRead;
 struct timeval lTime;

 FD_ZERO( &lRead );
 FD_SET( aSock, &lRead );

 lTime.tv_sec  = aMs / 1000;
 lTime.tv_usec = ( aMs % 1000 ) * 1000;

 return lwip_select ( aSock + 1, &lRead, NULL, NULL, &lTime ) > 0;

}  /* end _wait */

static int _open ( int aType, int aProto, unsigned int anIP, int aPort ) {

 struct sockaddr_in lAddr;
 int                lSock = lwip_socket ( PF_INET, aType, aProto );

 if ( lSock < 0 ) return -1;

 if ( aType == SOCK_STREAM ) {

  int lOpt = 1;

  lwip_setsockopt (  lSock, IPPROTO_TCP, TCP_NODELAY, ( char* )&lOpt, sizeof ( lOpt )  );

 }  /* end if */

 memset (  &lAddr, 0, sizeof ( lAddr )  );
 lAddr.sin_addr.s_addr = anIP;
 lAddr.sin_family      = AF_INET;
 lAddr.sin_port        = htons ( aPort );

 if (  lwip_connect (  lSock, ( struct sockaddr* )&lAddr, sizeof ( lAddr )  ) < 0  ) {

  lwip_close ( lSock );

  return -2;

 }  /* end if */

 return lSock;

}  /* end _open */

static int _send ( int aSock, unsigned char* apData, int aLen ) {

 int lSent = 0;

 while ( lSent < aLen ) {

  int lRes = lwip_send ( aSock, apData + lSent, aLen - lSent, 0 );

  if ( lRes <= 0 ) return -1;

  lSent += lRes;

 }  /* end while */

 return lSent;

}  /* end _send */

/* Asks one name server, twice at most. Returns the address, 0 when there is no answer. */
static unsigned int _resolve ( unsigned int aServer, const char* apName, unsigned char* apWork ) {

 unsigned int  retVal = 0;
 unsigned char lIP[ 4 ];
 int           lTry;
 int           lSock = _open ( SOCK_DGRAM, IPPROTO_UDP, aServer, 53 );

 if ( lSock < 0 ) return 0;

 for ( lTry = 0; lTry < 2 && !retVal; ++lTry ) {

  unsigned int lID  = ++s_QueryID & 0xFFFF;
  int          lLen = smsdns_query ( apWork, 512, apName, lID );

  if (  !lLen || lwip_send ( lSock, apWork, lLen, 0 ) < 0  ) break;

  if (  !_wait ( lSock, 3000 )  ) continue;

  lLen = lwip_recv ( lSock, apWork, 512, 0 );

  if (  lLen > 0 && smsdns_answer ( apWork, lLen, lID, lIP )  ) memcpy ( &retVal, lIP, 4 );

 }  /* end for */

 lwip_close ( lSock );

 return retVal;

}  /* end _resolve */

static void* _rpc ( int aCmd, void* apData, int aSize ) {

 SMSNetHdr*     lpHdr  = ( SMSNetHdr* )apData;
 unsigned char* lpData = ( unsigned char* )( lpHdr + 1 );
 int            lLen   = lpHdr -> m_Len;

 switch ( aCmd ) {

  case SMSNET_CONNECT:
   lpHdr -> m_Sock = _open ( SOCK_STREAM, IPPROTO_TCP, lpHdr -> m_IP, lLen );
  break;

  case SMSNET_SEND:
   lpHdr -> m_Len = lLen < 0 || lLen > SMSNET_CHUNK ? -1 : _send ( lpHdr -> m_Sock, lpData, lLen );
  break;

  case SMSNET_RECV:

   if ( lLen < 1 || lLen > SMSNET_CHUNK ) lLen = SMSNET_CHUNK;

   lpHdr -> m_Len = _wait ( lpHdr -> m_Sock, lpHdr -> m_Tmo ) ? lwip_recv ( lpHdr -> m_Sock, lpData, lLen, 0 )
                                                             : SMSNET_TIMEOUT;
  break;

  case SMSNET_CLOSE:
   lwip_close ( lpHdr -> m_Sock );
  break;

  case SMSNET_RESOLVE: {

   static unsigned char s_Work[ 512 ];
   char                 lName[ 128 ];

   strncpy (  lName, ( char* )lpData, sizeof ( lName ) - 1  );
   lName[ sizeof ( lName ) - 1 ] = '\x00';

   lpHdr -> m_IP = _resolve ( lpHdr -> m_IP, lName, s_Work );

  } break;

 }  /* end switch */

 return apData;

}  /* end _rpc */

static void _thread ( void* apArg ) {

 sceSifInitRpc ( 0 );
 sceSifSetRpcQueue (  &s_Queue, GetThreadId ()  );
 sceSifRegisterRpc ( &s_Server, SMSNET_RPC_ID, _rpc, s_Buf, NULL, NULL, &s_Queue );
 sceSifRpcLoop ( &s_Queue );

}  /* end _thread */

int _start ( int argc, char* argv[] ) {

 iop_thread_t lThread;
 int          lID;

 lThread.attr      = TH_C;
 lThread.option    = 0;
 lThread.thread    = _thread;
 lThread.stacksize = 0x2000;
 lThread.priority  = 40;

 lID = CreateThread ( &lThread );

 if ( lID <= 0 ) return MODULE_NO_RESIDENT_END;

 StartThread ( lID, NULL );

 return MODULE_RESIDENT_END;

}  /* end _start */
