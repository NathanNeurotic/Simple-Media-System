/* Name lookup packets ( RFC 1035 ). No I/O here, so the host test can run this file as is.
 * Licensed under the Academic Free License version 2.0. */
#include "dns.h"

int smsdns_query ( unsigned char* apBuf, int aCap, const char* apName, unsigned int anID ) {

 int i, lPos = 12, lLen;

 if ( aCap < 18 ) return 0;

 for ( i = 0; i < 12; ++i ) apBuf[ i ] = 0;

 apBuf[ 0 ] = ( unsigned char )( anID >> 8 );
 apBuf[ 1 ] = ( unsigned char )anID;
 apBuf[ 2 ] = 1;   /* recursion desired */
 apBuf[ 5 ] = 1;   /* one question      */

 while ( *apName ) {

  for ( lLen = 0; apName[ lLen ] && apName[ lLen ] != '.'; ++lLen );

  if ( lLen < 1 || lLen > 63 || lPos + lLen + 6 > aCap ) return 0;

  apBuf[ lPos++ ] = ( unsigned char )lLen;

  for ( i = 0; i < lLen; ++i ) apBuf[ lPos++ ] = ( unsigned char )apName[ i ];

  apName += lLen;

  if ( *apName == '.' ) ++apName;

 }  /* end while */

 if ( lPos == 12 ) return 0;

 apBuf[ lPos++ ] = 0;
 apBuf[ lPos++ ] = 0; apBuf[ lPos++ ] = 1;   /* type A   */
 apBuf[ lPos++ ] = 0; apBuf[ lPos++ ] = 1;   /* class IN */

 return lPos;

}  /* end smsdns_query */

/* Steps over one name. Returns the position after it, or -1 when it runs off the packet. */
static int _skip_name ( const unsigned char* apBuf, int aLen, int aPos ) {

 while ( aPos < aLen ) {

  int lLen = apBuf[ aPos ];

  if ( !lLen ) return aPos + 1;

  if (  ( lLen & 0xC0 ) == 0xC0  ) return aPos + 2 <= aLen ? aPos + 2 : -1;

  if ( lLen & 0xC0 ) return -1;

  aPos += lLen + 1;

 }  /* end while */

 return -1;

}  /* end _skip_name */

int smsdns_answer ( const unsigned char* apBuf, int aLen, unsigned int anID, unsigned char* apIP ) {

 int lPos = 12, lQ, lA;

 if ( aLen < 12 ) return 0;

 if (  apBuf[ 0 ] != ( unsigned char )( anID >> 8 ) || apBuf[ 1 ] != ( unsigned char )anID  ) return 0;

 if (  !( apBuf[ 2 ] & 0x80 ) || ( apBuf[ 3 ] & 0x0F )  ) return 0;   /* not a reply, or an error */

 lQ = ( apBuf[ 4 ] << 8 ) | apBuf[ 5 ];
 lA = ( apBuf[ 6 ] << 8 ) | apBuf[ 7 ];

 while ( lQ-- ) {

  lPos = _skip_name ( apBuf, aLen, lPos );

  if ( lPos < 0 || lPos + 4 > aLen ) return 0;

  lPos += 4;

 }  /* end while */

 while ( lA-- ) {

  int lType, lClass, lSize;

  lPos = _skip_name ( apBuf, aLen, lPos );

  if ( lPos < 0 || lPos + 10 > aLen ) return 0;

  lType  = ( apBuf[ lPos     ] << 8 ) | apBuf[ lPos + 1 ];
  lClass = ( apBuf[ lPos + 2 ] << 8 ) | apBuf[ lPos + 3 ];
  lSize  = ( apBuf[ lPos + 8 ] << 8 ) | apBuf[ lPos + 9 ];
  lPos  += 10;

  if ( lPos + lSize > aLen ) return 0;

  if ( lType == 1 && lClass == 1 && lSize == 4 ) {

   apIP[ 0 ] = apBuf[ lPos     ];
   apIP[ 1 ] = apBuf[ lPos + 1 ];
   apIP[ 2 ] = apBuf[ lPos + 2 ];
   apIP[ 3 ] = apBuf[ lPos + 3 ];

   return 1;

  }  /* end if */

  lPos += lSize;

 }  /* end while */

 return 0;

}  /* end smsdns_answer */
