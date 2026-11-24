/* Sockets for the EE side of SMS, through the smsnet IOP module ( iop/smsnet ).
 * BDM builds only. One caller at a time ( the menu thread ).
 * Licensed under the Academic Free License version 2.0. */
#ifndef __SMS_Net_H
#define __SMS_Net_H

#define SMS_NET_TIMEOUT ( -2 )

/* Brings the network up on demand: adapter, TCP/IP stack and the socket service.
 * 1 = ready, 0 = no adapter or a module failed, -1 = UDPFS holds the adapter. */
int          SMS_NetStart   ( void );
/* Looks a name up: the gateway first, then two public name servers. 0 = not found. */
unsigned int SMS_NetResolve ( const char* apName );
int          SMS_NetConnect ( unsigned int anIP, int aPort );                  /* socket, < 0 = failed */
int          SMS_NetSend    ( int aSock, const void* apData, int aLen );       /* aLen, < 0 = failed   */
/* > 0 bytes, 0 = closed by the server, SMS_NET_TIMEOUT, other < 0 = failed. */
int          SMS_NetRecv    ( int aSock, void* apData, int aLen, int aTimeoutMs );
void         SMS_NetClose   ( int aSock );

#endif  /* __SMS_Net_H */
