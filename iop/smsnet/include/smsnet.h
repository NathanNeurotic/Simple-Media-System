/* smsnet: a small socket service on the IOP for the EE side of SMS.
 *
 * SMS has no socket library on the EE. Its network stack ( SMSTCPIP ) lives on the IOP
 * and only IOP modules can reach it. This module forwards the few calls the clock sync
 * and the updater need: open a TCP connection, send, receive, close, and look a name up.
 *
 * One request at a time. Every call carries a 16 byte header followed by data.
 * Licensed under the Academic Free License version 2.0.
 */
#ifndef SMSNET_H
#define SMSNET_H

#define SMSNET_RPC_ID 0x534D4E01

#define SMSNET_CONNECT 1 /* in: ip, port in len        out: sock ( < 0 = failed )            */
#define SMSNET_SEND    2 /* in: sock, len, data        out: len sent ( < 0 = failed )        */
#define SMSNET_RECV    3 /* in: sock, len, tmo ( ms )  out: len, data ( 0 closed, -2 timeout ) */
#define SMSNET_CLOSE   4 /* in: sock                                                         */
#define SMSNET_RESOLVE 5 /* in: ip of the name server, name in data   out: ip ( 0 = failed ) */

#define SMSNET_CHUNK   4096
#define SMSNET_TIMEOUT ( -2 )

typedef struct SMSNetHdr {
 int          m_Sock;
 int          m_Len;
 int          m_Tmo;
 unsigned int m_IP;   /* network byte order */
} SMSNetHdr;

#define SMSNET_BUFSIZE (  sizeof ( SMSNetHdr ) + SMSNET_CHUNK  )

#endif
