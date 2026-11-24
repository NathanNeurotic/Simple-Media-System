/* Parsing and date arithmetic for the clock sync and the updater. See SMS_UpdateCore.h.
 * Licensed under the Academic Free License version 2.0. */
#include "SMS_UpdateCore.h"

#include <string.h>

int UPD_DaysIn ( int aYear, int aMonth ) {

 static const unsigned char s_Days[ 12 ] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

 return s_Days[ aMonth - 1 ] + (  aMonth == 2 && !( aYear & 3 )  );   /* 2000-2099: every fourth year is a leap year */

}  /* end UPD_DaysIn */

int UPD_ClockShift ( int* apT, int aMinutes ) {

 int lMin = apT[ 3 ] * 60 + apT[ 4 ] + aMinutes;

 if ( lMin < 0 ) {

  lMin += 1440;

  if ( --apT[ 2 ] < 1 ) {

   if ( --apT[ 1 ] < 1 ) {
    apT[ 1 ] = 12;
    --apT[ 0 ];
   }  /* end if */

   apT[ 2 ] = UPD_DaysIn ( apT[ 0 ], apT[ 1 ] );

  }  /* end if */

 } else if ( lMin >= 1440 ) {

  lMin -= 1440;

  if (  ++apT[ 2 ] > UPD_DaysIn ( apT[ 0 ], apT[ 1 ] )  ) {

   apT[ 2 ] = 1;

   if ( ++apT[ 1 ] > 12 ) {
    apT[ 1 ] = 1;
    ++apT[ 0 ];
   }  /* end if */

  }  /* end if */

 }  /* end if */

 apT[ 3 ] = lMin / 60;
 apT[ 4 ] = lMin % 60;

 return apT[ 0 ] >= 2000 && apT[ 0 ] <= 2099;

}  /* end UPD_ClockShift */

/* Reads exactly aCount digits. Returns the position after them, NULL when one is missing. */
static const char* _digits ( const char* apStr, int aCount, int* apVal ) {

 int lVal = 0;

 while ( aCount-- ) {

  if ( *apStr < '0' || *apStr > '9' ) return NULL;

  lVal = lVal * 10 + ( *apStr++ - '0' );

 }  /* end while */

 *apVal = lVal;

 return apStr;

}  /* end _digits */

int UPD_ParseDate ( const char* apHeaders, int* apT ) {

 static const char s_Months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

 const char* lpPos = apHeaders;
 int         i;

 for ( ;; ) {   /* the field starts a line; the status line is never it */

  lpPos = strchr ( lpPos, '\n' );

  if ( !lpPos ) return 0;

  ++lpPos;

  if (  ( lpPos[ 0 ] | 0x20 ) == 'd' && ( lpPos[ 1 ] | 0x20 ) == 'a' &&
        ( lpPos[ 2 ] | 0x20 ) == 't' && ( lpPos[ 3 ] | 0x20 ) == 'e' && lpPos[ 4 ] == ':'
  ) break;

 }  /* end for */

 lpPos += 5;

 while ( *lpPos == ' ' ) ++lpPos;
 while ( *lpPos && *lpPos != ',' && *lpPos != '\r' && *lpPos != '\n' ) ++lpPos;   /* the weekday */

 if ( lpPos[ 0 ] != ',' || lpPos[ 1 ] != ' ' ) return 0;

 if (  !(  lpPos = _digits ( lpPos + 2, 2, &apT[ 2 ] )  ) || *lpPos++ != ' '  ) return 0;

 for ( i = 0; i < 12; ++i ) if (  !strncmp ( lpPos, &s_Months[ i * 3 ], 3 )  ) break;

 if ( i == 12 || lpPos[ 3 ] != ' ' ) return 0;

 apT[ 1 ] = i + 1;

 if (  !(  lpPos = _digits ( lpPos + 4, 4, &apT[ 0 ] )  ) || *lpPos++ != ' '  ) return 0;
 if (  !(  lpPos = _digits ( lpPos,     2, &apT[ 3 ] )  ) || *lpPos++ != ':'  ) return 0;
 if (  !(  lpPos = _digits ( lpPos,     2, &apT[ 4 ] )  ) || *lpPos++ != ':'  ) return 0;
 if (  !(  lpPos = _digits ( lpPos,     2, &apT[ 5 ] )  )                     ) return 0;

 if (  strncmp ( lpPos, " GMT", 4 ) || ( lpPos[ 4 ] != '\r' && lpPos[ 4 ] != '\n' )  ) return 0;

 return apT[ 0 ] >= 2000 && apT[ 0 ] <= 2099 &&
        apT[ 2 ] >= 1    && apT[ 2 ] <= UPD_DaysIn ( apT[ 0 ], apT[ 1 ] ) &&
        apT[ 3 ] <  24   && apT[ 4 ] <  60 && apT[ 5 ] < 61;

}  /* end UPD_ParseDate */

static int _hex ( char aChr ) {

 if ( aChr >= '0' && aChr <= '9' ) return aChr - '0';
 if ( aChr >= 'a' && aChr <= 'f' ) return aChr - 'a' + 10;
 if ( aChr >= 'A' && aChr <= 'F' ) return aChr - 'A' + 10;

 return -1;

}  /* end _hex */

int UPD_ParseSums ( const char* apText, const char* apName, unsigned char* apSHA,
                    unsigned int* apBuild, char* apID, int anIDSize ) {

 int         lfSHA = 0, lfBuild = 0;
 int         lNameLen = strlen ( apName );
 const char* lpLine = apText;

 while ( *lpLine ) {

  const char* lpEnd = strchr ( lpLine, '\n' );
  int         lLen  = lpEnd ? ( int )( lpEnd - lpLine ) : ( int )strlen ( lpLine );
  int         i;

  if ( lLen && lpLine[ lLen - 1 ] == '\r' ) --lLen;

  if (  lLen > 8 && !strncmp ( lpLine, "# build ", 8 )  ) {

   unsigned int lBuild = 0;
   int          lPos   = 8, lID = 0;

   while ( lPos < lLen && lpLine[ lPos ] >= '0' && lpLine[ lPos ] <= '9' && lPos < 18 )

    lBuild = lBuild * 10 + ( unsigned int )( lpLine[ lPos++ ] - '0' );

/* The first complete build line counts: a later one must not write over its ID. */
   if ( !lfBuild && lPos > 8 && lPos < lLen && lpLine[ lPos ] == ' ' ) {

    for ( ++lPos; lPos < lLen && lID < anIDSize - 1; ++lPos ) {

     char lChr = lpLine[ lPos ];

     if (  !(  ( lChr >= '0' && lChr <= '9' ) || ( lChr >= 'a' && lChr <= 'z' ) ||
               ( lChr >= 'A' && lChr <= 'Z' ) || lChr == '.' || lChr == '-'  )
     ) break;

     apID[ lID++ ] = lChr;

    }  /* end for */

    if ( lID && lPos == lLen ) {

     apID[ lID ] = '\x00';
     *apBuild    = lBuild;
     lfBuild     = 1;

    }  /* end if */

   }  /* end if */

  } else if ( lLen > 65 ) {

   unsigned char lSHA[ 32 ];
   const char*   lpName = lpLine + 64;

   for ( i = 0; i < 32; ++i ) {

    int lHi = _hex ( lpLine[ 2 * i ] ), lLo = _hex ( lpLine[ 2 * i + 1 ] );

    if ( lHi < 0 || lLo < 0 ) break;

    lSHA[ i ] = ( unsigned char )( lHi * 16 + lLo );

   }  /* end for */

   while (  lpName < lpLine + lLen && ( *lpName == ' ' || *lpName == '*' )  ) ++lpName;

   if (  i == 32 && lpName > lpLine + 64 && ( int )( lpLine + lLen - lpName ) == lNameLen &&
         !strncmp ( lpName, apName, lNameLen )
   ) {

    memcpy ( apSHA, lSHA, 32 );
    lfSHA = 1;

   }  /* end if */

  }  /* end if */

  if ( !lpEnd ) break;

  lpLine = lpEnd + 1;

 }  /* end while */

 return lfSHA && lfBuild;

}  /* end UPD_ParseSums */

const char* UPD_SplitURL ( const char* apURL, char* apHost, int aHostSize ) {

 const char* lpSlash;
 int         lLen;

 if (  strncmp ( apURL, "https://", 8 )  ) return NULL;

 apURL  += 8;
 lpSlash = strchr ( apURL, '/' );

 if ( !lpSlash || lpSlash == apURL || lpSlash - apURL >= aHostSize ) return NULL;

 lLen = ( int )( lpSlash - apURL );
 memcpy ( apHost, apURL, lLen );
 apHost[ lLen ] = '\x00';

 if (  strcmp ( apHost, "github.com"                           ) &&
       strcmp ( apHost, "objects.githubusercontent.com"        ) &&
       strcmp ( apHost, "release-assets.githubusercontent.com" )
 ) return NULL;   /* a redirect anywhere but GitHub is never followed */

 return lpSlash;

}  /* end UPD_SplitURL */

int UPD_IsELF ( const unsigned char* apData, int aSize ) {

 return aSize >= 52 && apData[ 0 ] == 0x7F && apData[ 1 ] == 'E' && apData[ 2 ] == 'L' && apData[ 3 ] == 'F' &&
        apData[  4 ] == 1 && apData[  5 ] == 1 &&
        apData[ 16 ] == 2 && apData[ 17 ] == 0 && apData[ 18 ] == 8 && apData[ 19 ] == 0;

}  /* end UPD_IsELF */

int UPD_IsPacked ( const unsigned char* apData, int aSize ) {

 return UPD_IsELF ( apData, aSize ) && apData[ 27 ] != 0;   /* entry point at or above 0x01000000 */

}  /* end UPD_IsPacked */
