#include "SMS_Artwork.h"

#include <stdlib.h>
#include <string.h>

void SMS_ArtworkClear ( SMS_ArtworkCache* apCache ) {
 unsigned int i;
 for ( i = 0; i < SMS_ARTWORK_SLOTS; ++i ) free ( apCache -> m_Slots[ i ].m_pPixels );
 memset ( apCache, 0, sizeof ( *apCache ) );
}

void SMS_ArtworkPage ( SMS_ArtworkCache* apCache, const void* const* apKeys, unsigned int aCount ) {
 unsigned int i;
 if ( aCount > SMS_ARTWORK_SLOTS ) aCount = SMS_ARTWORK_SLOTS;
 if ( aCount == apCache -> m_Count ) {
  for ( i = 0; i < aCount && apCache -> m_Slots[ i ].m_pKey == apKeys[ i ]; ++i );
  if ( i == aCount ) return;
 }
 SMS_ArtworkClear ( apCache );
 apCache -> m_Count = aCount;
 for ( i = 0; i < aCount; ++i ) apCache -> m_Slots[ i ].m_pKey = apKeys[ i ];
}

unsigned char* SMS_ArtworkGet ( SMS_ArtworkCache* apCache, unsigned int anIndex,
 unsigned char* ( *aLoad ) ( const void*, void* ), void* apParam ) {
 SMS_ArtworkSlot* lpSlot;
 if ( anIndex >= apCache -> m_Count ) return NULL;
 lpSlot = &apCache -> m_Slots[ anIndex ];
 if ( !lpSlot -> m_Attempted ) {
  lpSlot -> m_Attempted = 1;
  lpSlot -> m_pPixels = aLoad ( lpSlot -> m_pKey, apParam );
 }
 return lpSlot -> m_pPixels;
}

int SMS_ArtworkName ( char* apOut, unsigned int aCapacity, const char* apMedia, int aJPEG ) {
 const char* lpDot;
 unsigned int lStem, lExt = aJPEG ? 5 : 4;
 if ( !apOut || !apMedia || !*apMedia || strchr ( apMedia, '/' ) || strchr ( apMedia, '\\' ) ) return 0;
 lpDot = strrchr ( apMedia, '.' );
 if ( !lpDot || lpDot == apMedia ) return 0;
 lStem = lpDot - apMedia;
 if ( lStem + lExt + 1 > aCapacity ) return 0;
 memcpy ( apOut, apMedia, lStem );
 strcpy ( apOut + lStem, aJPEG ? ".jpeg" : ".jpg" );
 return 1;
}

/* Resource preflight only. The JPEG decoder still validates the complete file.
 * Reject unsupported frames and oversized dimensions before its allocations. */
int SMS_ArtworkJPEGSize ( const unsigned char* apData, unsigned int aSize ) {
 unsigned int lPos = 2;
 if ( !apData || aSize < 4 || aSize > SMS_ARTWORK_MAX_FILE || apData[ 0 ] != 0xFF || apData[ 1 ] != 0xD8 ) return 0;
 while ( lPos < aSize ) {
  unsigned int lMarker, lLength, lWidth, lHeight;
  if ( apData[ lPos++ ] != 0xFF ) return 0;
  while ( lPos < aSize && apData[ lPos ] == 0xFF ) ++lPos;
  if ( lPos >= aSize ) return 0;
  lMarker = apData[ lPos++ ];
  if ( lMarker == 0xDA || lMarker == 0xD9 || !lMarker || lMarker == 0xD8 ) return 0;
  if ( lPos + 2 > aSize ) return 0;
  lLength = ( apData[ lPos ] << 8 ) | apData[ lPos + 1 ];
  if ( lLength < 2 || lLength > aSize - lPos ) return 0;
  if ( lMarker == 0xC0 ) {
   if ( lLength != 17 || apData[ lPos + 2 ] != 8 || apData[ lPos + 7 ] != 3 ) return 0;
   lHeight = ( apData[ lPos + 3 ] << 8 ) | apData[ lPos + 4 ];
   lWidth  = ( apData[ lPos + 5 ] << 8 ) | apData[ lPos + 6 ];
   return lWidth && lHeight && lWidth <= SMS_ARTWORK_MAX_SIDE && lHeight <= SMS_ARTWORK_MAX_SIDE;
  }
  if ( lMarker >= 0xC1 && lMarker <= 0xCF && lMarker != 0xC4 ) return 0;
  lPos += lLength;
 }
 return 0;
}

int SMS_ArtworkThumbnail ( unsigned char* apOut, const unsigned char* apRGB, unsigned int aWidth, unsigned int aHeight ) {
 unsigned int lWidth, lHeight, x, y;
 if ( !apOut || !apRGB || !aWidth || !aHeight || aWidth > SMS_ARTWORK_MAX_SIDE || aHeight > SMS_ARTWORK_MAX_SIDE ) return 0;
 if ( aWidth >= aHeight ) {
  lWidth = SMS_ARTWORK_SIDE;
  lHeight = aHeight * SMS_ARTWORK_SIDE / aWidth;
 } else {
  lHeight = SMS_ARTWORK_SIDE;
  lWidth = aWidth * SMS_ARTWORK_SIDE / aHeight;
 }
 if ( !lWidth ) lWidth = 1;
 if ( !lHeight ) lHeight = 1;
 memset ( apOut, 0, SMS_ARTWORK_BYTES );
 for ( y = 0; y < SMS_ARTWORK_SIDE; ++y )
  for ( x = 0; x < SMS_ARTWORK_SIDE; ++x ) apOut[ ( y * SMS_ARTWORK_SIDE + x ) * 4 + 3 ] = 0x80;
 for ( y = 0; y < lHeight; ++y ) {
  unsigned int lSY = y * aHeight / lHeight;
  for ( x = 0; x < lWidth; ++x ) {
   unsigned int lSX = x * aWidth / lWidth;
   unsigned int lDst = ( ( y + ( SMS_ARTWORK_SIDE - lHeight ) / 2 ) * SMS_ARTWORK_SIDE + x + ( SMS_ARTWORK_SIDE - lWidth ) / 2 ) * 4;
   memcpy ( apOut + lDst, apRGB + ( lSY * aWidth + lSX ) * 3, 3 );
  }
 }
 return 1;
}

unsigned int SMS_ArtworkMove ( unsigned int aCount, unsigned int anIndex, int aDelta ) {
 if ( !aCount ) return 0;
 if ( anIndex >= aCount ) anIndex = aCount - 1;
 if ( aDelta < 0 ) {
  unsigned int lDistance = ( unsigned int )( -( aDelta + 1 ) ) + 1;
  return lDistance > anIndex ? 0 : anIndex - lDistance;
 }
 return ( unsigned int )aDelta >= aCount - anIndex ? aCount - 1 : anIndex + aDelta;
}
