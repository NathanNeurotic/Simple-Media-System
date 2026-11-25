/* The console clock ( read, set, set from the Internet ) and Check for updates.
 *
 * The update is downloaded whole into memory over a verified HTTPS connection
 * ( SMS_TLS.c, mbedTLS ), checked against the SHA-256 the release publishes, and only then
 * written over the running program's file, in place. Nothing else on the device is touched.
 *
 * Built with -G0 and exporting functions only ( see the Makefile ).
 * Licensed under the Academic Free License version 2.0. */
#include "SMS.h"
#include "SMS_Update.h"
#include "SMS_UpdateCore.h"
#include "SMS_CDDA.h"
#include "SMS_DateTime.h"
#include "SMS_Config.h"

#include <string.h>

static int _bcd ( int aVal ) { return (  ( aVal / 10 ) << 4  ) | ( aVal % 10 ); }
static int _dec ( int aVal ) { return ( aVal >> 4 ) * 10 + ( aVal & 15 );       }

/* Minutes to add to the console clock ( Japan time ) to get the user's local time. */
int SMS_ZoneOffset ( void ) {

 return UPD_ZoneOffset (  g_Config.m_ClockZone, SMS_TimeZone (), SMS_SummerTime ()  );

}  /* end SMS_ZoneOffset */

static int _zone ( void ) { return SMS_ZoneOffset (); }

/* The clock as the console keeps it: { year, month, day, hour, minute, second }, Japan time. */
static int _rtc_read ( int* apT ) {

 unsigned char lClk[ 8 ] __attribute__(   (  aligned( 8 )  )   );

 if (  !CDDA_ReadClock ( lClk ) || ( lClk[ 0 ] & 0x80 )  ) return 0;

 apT[ 0 ] = 2000 + _dec ( lClk[ 7 ] );
 apT[ 1 ] = _dec ( lClk[ 6 ] & 0x1F );
 apT[ 2 ] = _dec ( lClk[ 5 ] );
 apT[ 3 ] = _dec ( lClk[ 3 ] );
 apT[ 4 ] = _dec ( lClk[ 2 ] );
 apT[ 5 ] = _dec ( lClk[ 1 ] );

 return apT[ 1 ] >= 1 && apT[ 1 ] <= 12 && apT[ 2 ] >= 1 && apT[ 2 ] <= 31 && apT[ 3 ] < 24 && apT[ 4 ] < 60;

}  /* end _rtc_read */

static int _rtc_write ( const int* apT ) {

 unsigned char lClk[ 8 ] __attribute__(   (  aligned( 8 )  )   );

 lClk[ 0 ] = 0;
 lClk[ 1 ] = _bcd ( apT[ 5 ] );
 lClk[ 2 ] = _bcd ( apT[ 4 ] );
 lClk[ 3 ] = _bcd ( apT[ 3 ] );
 lClk[ 4 ] = 0;
 lClk[ 5 ] = _bcd ( apT[ 2 ] );
 lClk[ 6 ] = _bcd ( apT[ 1 ] );
 lClk[ 7 ] = _bcd ( apT[ 0 ] - 2000 );

 return CDDA_WriteClock ( lClk );

}  /* end _rtc_write */

int SMS_ClockGet ( int* apT ) {

 int lT[ 6 ];

 if (  !_rtc_read ( lT ) || !UPD_ClockShift ( lT, _zone () )  ) return 0;

 memcpy (  apT, lT, 5 * sizeof ( int )  );

 return 1;

}  /* end SMS_ClockGet */

int SMS_ClockSet ( const int* apT, int aSec ) {

 int lT[ 6 ];

 memcpy (  lT, apT, 5 * sizeof ( int )  );
 lT[ 5 ] = aSec;

 if (  lT[ 2 ] > UPD_DaysIn ( lT[ 0 ], lT[ 1 ] )  ) lT[ 2 ] = UPD_DaysIn ( lT[ 0 ], lT[ 1 ] );

 return UPD_ClockShift ( lT, -_zone () ) && _rtc_write ( lT );

}  /* end SMS_ClockSet */

#ifdef BDM
#include "SMS_Net.h"
#include "SMS_TLS.h"
#include "SMS_IOP.h"
#include "SMS_GUI.h"
#include "SMS_Locale.h"
#include "SMS_Config.h"
#include "SMS_Timer.h"
#include "SMS_MC.h"

#include <kernel.h>
#include <fileio.h>
#include <malloc.h>
#include <stdio.h>
#include <time.h>

#include "mbedtls/platform_time.h"
#include "mbedtls/sha256.h"
#include "mbedtls/ssl.h"

#ifndef SMS_BUILD_TIME
# define SMS_BUILD_TIME 0   /* a local build: any published build is newer */
#endif  /* SMS_BUILD_TIME */

#define UPD_REPO     "/NathanNeurotic/Simple-Media-System/releases/"
#ifdef SMS_BUILD_STABLE
# define UPD_SUMS    "https://github.com" UPD_REPO "latest/download/SHA256SUMS.txt"
# define UPD_ASSETS  "https://github.com" UPD_REPO "latest/download/"
#else   /* the build that follows every change */
# define UPD_SUMS    "https://github.com" UPD_REPO "download/rolling/SHA256SUMS.txt"
# define UPD_ASSETS  "https://github.com" UPD_REPO "download/rolling/"
#endif  /* SMS_BUILD_STABLE */

#define UPD_BUF      16384               /* the request, then each piece of headers and body */
#define UPD_URL      2048                /* GitHub's signed download links run to about 900  */
#define UPD_MAX_ELF  ( 6 * 1024 * 1024 )
#define UPD_MAX_SUMS 4096
#define UPD_IDLE_MS  15000               /* a connection that says nothing for this long     */
#define UPD_MIN_YEAR 2025                /* an earlier clock was never set                   */
#define UPD_MAX_YEAR 2060                /* a later one is wrong: the roots are long expired */

/* What went wrong, beyond SMS_TLS.h's own codes. */
#define UPD_ERR_DNS     ( -20 )
#define UPD_ERR_CONNECT ( -21 )

extern unsigned char sectigo_e46_der[];  extern unsigned int size_sectigo_e46_der;
extern unsigned char sectigo_r46_der[];  extern unsigned int size_sectigo_r46_der;
extern unsigned char isrgrootx1_der[];   extern unsigned int size_isrgrootx1_der;
extern unsigned char isrg_root_x2_der[]; extern unsigned int size_isrg_root_x2_der;

static char s_BootPath[ 256 ];

void SMS_UpdateSetBootPath ( const char* apPath ) {

 strncpy (  s_BootPath, apPath, sizeof ( s_BootPath ) - 1  );

}  /* end SMS_UpdateSetBootPath */

/* -------------------------------------------------------------------------------------
 * What mbedTLS asks of the platform ( third_party/mbedtls-port/sms_mbedtls_config.h ).
 * ----------------------------------------------------------------------------------- */
mbedtls_ms_time_t mbedtls_ms_time ( void ) { return ( mbedtls_ms_time_t )g_Timer; }

struct tm* mbedtls_platform_gmtime_r ( const mbedtls_time_t* apTime, struct tm* apTM ) {

/* Howard Hinnant's civil_from_days. */
 long long lSec  = ( long long )*apTime;
 long long lDays = lSec / 86400;
 int       lRem  = ( int )( lSec % 86400 );
 long long lEra;
 unsigned  lDoe, lYoe, lDoy, lMp;
 int       lYear, lMonth;

 if ( lRem < 0 ) { lRem += 86400; --lDays; }

 lDays += 719468;
 lEra   = ( lDays >= 0 ? lDays : lDays - 146096 ) / 146097;
 lDoe   = ( unsigned )( lDays - lEra * 146097 );
 lYoe   = ( lDoe - lDoe / 1460 + lDoe / 36524 - lDoe / 146096 ) / 365;
 lDoy   = lDoe - ( 365 * lYoe + lYoe / 4 - lYoe / 100 );
 lMp    = ( 5 * lDoy + 2 ) / 153;
 lMonth = ( int )( lMp < 10 ? lMp + 3 : lMp - 9 );
 lYear  = ( int )( lYoe + lEra * 400 ) + ( lMonth <= 2 );

 memset (  apTM, 0, sizeof ( *apTM )  );
 apTM -> tm_year = lYear - 1900;
 apTM -> tm_mon  = lMonth - 1;
 apTM -> tm_mday = ( int )( lDoy - ( 153 * lMp + 2 ) / 5 + 1 );
 apTM -> tm_hour = lRem / 3600;
 apTM -> tm_min  = lRem / 60 % 60;
 apTM -> tm_sec  = lRem % 60;

 return apTM;

}  /* end mbedtls_platform_gmtime_r */

static unsigned int _count ( void ) {

 unsigned int retVal;

 __asm__ volatile ( "mfc0 %0, $9" : "=r"( retVal ) );

 return retVal;

}  /* end _count */

/* The PS2 has no hardware random source. Each output byte folds 8 samples of the CPU's cycle
 * counter taken across a thread switch ( interrupt and DMA timing make its low bits jitter );
 * the clock and the millisecond timer are mixed in once per call. The same source as the
 * RiptOPL and POPSLoader updaters. */
int mbedtls_hardware_poll ( void* apData, unsigned char* apOut, size_t aLen, size_t* apLen ) {

 unsigned char lClk[ 8 ] __attribute__(   (  aligned( 8 )  )   );
 size_t        i;
 int           j;

 for ( i = 0; i < aLen; ++i ) {

  unsigned char lByte = 0;

  for ( j = 0; j < 8; ++j ) {

   unsigned int lBefore = _count ();

   SMS_TimerWait ( 1 );

   lByte = ( unsigned char )(  ( lByte << 1 ) | ( lByte >> 7 )  ) ^ ( unsigned char )( _count () - lBefore );

  }  /* end for */

  apOut[ i ] = lByte ^ ( unsigned char )g_Timer;

 }  /* end for */

 if (  aLen >= 8 && CDDA_ReadClock ( lClk )  ) for ( i = 0; i < 8; ++i ) apOut[ i ] ^= lClk[ i ];

 *apLen = aLen;

 return 0;

}  /* end mbedtls_hardware_poll */

static long long _now_utc ( void ) {

 int lT[ 6 ];

 if (  !_rtc_read ( lT ) || lT[ 0 ] < UPD_MIN_YEAR || lT[ 0 ] > UPD_MAX_YEAR  ) return -1;

 return raTlsRtcToUtc ( lT[ 0 ], lT[ 1 ], lT[ 2 ], lT[ 3 ], lT[ 4 ], lT[ 5 ] );

}  /* end _now_utc */

static int _bio_send ( void* apCtx, const unsigned char* apBuf, size_t aLen ) {

 int lRes = SMS_NetSend (  *( int* )apCtx, apBuf, ( int )aLen  );

 return lRes < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : lRes;

}  /* end _bio_send */

static int _bio_recv ( void* apCtx, unsigned char* apBuf, size_t aLen ) {

 int lRes = SMS_NetRecv (  *( int* )apCtx, apBuf, ( int )aLen, UPD_IDLE_MS  );

 if ( lRes == SMS_NET_TIMEOUT ) return MBEDTLS_ERR_SSL_TIMEOUT;

 return lRes < 0 ? MBEDTLS_ERR_SSL_INTERNAL_ERROR : lRes;

}  /* end _bio_recv */

/* -------------------------------------------------------------------------------------
 * The network, on demand.
 * ----------------------------------------------------------------------------------- */
static char* _str ( SMString* apStr ) { return apStr -> m_pStr; }

/* Brings the network up whatever state it is in. When UDPFS holds the adapter the user is
 * asked first, then the adapter is taken from it with the same in-place IOP reset the
 * network mode switch uses. 1 = ready; 0 = not, and the user has been told why. */
static int _net_up ( void ) {

 int lRes;

 GUI_Status (  _str ( &STR_INITIALIZING_NETWORK )  );

 lRes = SMS_NetStart ();

 if ( lRes < 0 ) {

  if (  !GUI_Question (  _str ( &STR_NET_UDPFS_ASK )  )   ) return 0;

  SMS_IOPNetSwitch ( SMS_NETMODE_OFF );
  GUI_Redraw ( GUIRedrawMethod_Redraw );
  GUI_Status (  _str ( &STR_INITIALIZING_NETWORK )  );

  lRes = SMS_NetStart ();

 }  /* end if */

 if ( lRes <= 0 ) GUI_Error (  _str ( &STR_NET_NA )  );

 return lRes > 0;

}  /* end _net_up */

static int _connect ( const char* apHost, int aPort ) {

 unsigned int lIP = SMS_NetResolve ( apHost );
 int          lSock;

 if ( !lIP ) return UPD_ERR_DNS;

 lSock = SMS_NetConnect ( lIP, aPort );

 return lSock < 0 ? UPD_ERR_CONNECT : lSock;

}  /* end _connect */

static void _report ( int aRes ) {

 SMString* lpStr;

 switch ( aRes ) {

  case UPD_ERR_DNS      : lpStr = &STR_NET_DNS;     break;
  case RATLS_ERR_NET    :
  case UPD_ERR_CONNECT  : lpStr = &STR_NET_CONNECT; break;
  case RATLS_ERR_TIMEOUT: lpStr = &STR_NET_TIMEOUT; break;
  case RATLS_ERR_CLOCK  : lpStr = &STR_UPD_CLOCK;   break;
  case RATLS_ERR_TLS    :
  case RATLS_ERR_CERT   : lpStr = &STR_UPD_TLS;     break;
  case RATLS_ERR_MEM    : lpStr = &STR_UPD_MEM;     break;
  default               : lpStr = &STR_UPD_HTTP;    break;

 }  /* end switch */

 GUI_Error (  _str ( lpStr )  );

}  /* end _report */

/* -------------------------------------------------------------------------------------
 * Set the clock from the Internet: the Date line of a plain HTTP reply. Nothing secret or
 * trusted rides on it, and it has to work while the clock is still wrong, which HTTPS
 * cannot.
 * ----------------------------------------------------------------------------------- */
static int _http_date ( const char* apHost, int* apT ) {

 static const char s_Fmt[] = "HEAD / HTTP/1.1\r\nHost: %s\r\nUser-Agent: SMS\r\nConnection: close\r\n\r\n";

 char lBuf[ 1024 ];
 int  lSock = _connect ( apHost, 80 );
 int  lLen, lGot, retVal = RATLS_ERR_HTTP;

 if ( lSock < 0 ) return lSock;

 lLen = sprintf ( lBuf, s_Fmt, apHost );

 if (  SMS_NetSend ( lSock, lBuf, lLen ) != lLen  )

  retVal = RATLS_ERR_NET;

 else for ( lLen = 0; lLen < ( int )sizeof ( lBuf ) - 1; ) {

  lGot = SMS_NetRecv (  lSock, lBuf + lLen, sizeof ( lBuf ) - 1 - lLen, 8000  );

  if ( lGot <= 0 ) {
   if ( lGot == SMS_NET_TIMEOUT ) retVal = RATLS_ERR_TIMEOUT;
   break;
  }  /* end if */

  lLen        += lGot;
  lBuf[ lLen ] = '\x00';

  if (  UPD_ParseDate ( lBuf, apT )  ) { retVal = 0; break; }

 }  /* end for */

 SMS_NetClose ( lSock );

 return retVal;

}  /* end _http_date */

int SMS_ClockFromInternet ( void ) {

 static const char* s_Hosts[ 2 ] = { "github.com", "www.google.com" };

 int lT[ 6 ], lRes = 0, i;

 if (  !_net_up ()  ) return 0;

 GUI_Status (  _str ( &STR_ASKING_TIME )  );

/* A second server only when the first one was found and failed: a name lookup that fails
 * does so for every name, after a long wait. */
 for ( i = 0; i < 2; ++i ) if (   !(  lRes = _http_date ( s_Hosts[ i ], lT )  ) || lRes == UPD_ERR_DNS   ) break;

 if ( lRes ) { _report ( lRes ); return 0; }

/* The answer is not authenticated: a year outside the window the secure connection accepts
 * is not written. UTC to the Japan time the console keeps. */
 if (  lT[ 0 ] < UPD_MIN_YEAR || lT[ 0 ] > UPD_MAX_YEAR || !UPD_ClockShift ( lT, 540 ) || !_rtc_write ( lT )  ) {
  GUI_Error (  _str ( &STR_CLOCK_REFUSED )  );
  return 0;
 }  /* end if */

 GUI_Status (  _str ( &STR_CLOCK_SET )  );

 return 1;

}  /* end SMS_ClockFromInternet */

/* -------------------------------------------------------------------------------------
 * Check for updates.
 * ----------------------------------------------------------------------------------- */
typedef struct UpdBody {

 unsigned char* m_pData;
 int            m_Size;   /* the room in m_pData; 0 = allocate what the server declares */
 int            m_Len;
 int            m_fShow;

} UpdBody;

static int _sink ( void* apCtx, const char* apData, int aLen ) {

 UpdBody* lpBody = ( UpdBody* )apCtx;

 if ( !lpBody -> m_Size ) {

  lpBody -> m_Size  = raTlsBodyLength;
  lpBody -> m_pData = ( unsigned char* )memalign ( 64, lpBody -> m_Size );

 }  /* end if */

 if ( !lpBody -> m_pData || lpBody -> m_Len + aLen > lpBody -> m_Size ) return 1;

 memcpy ( lpBody -> m_pData + lpBody -> m_Len, apData, aLen );
 lpBody -> m_Len += aLen;

 if ( lpBody -> m_fShow ) GUI_Progress (
  _str ( &STR_UPD_DOWNLOADING ), ( int )(  ( long long )lpBody -> m_Len * 100 / raTlsBodyLength  ), 0
 );

 return 0;

}  /* end _sink */

/* One verified download, following up to three redirects. The body length or an error. */
static int _fetch ( const char* apURL, int aMaxBody, UpdBody* apBody ) {

 static const ratls_io_t s_IO = { _bio_send, _bio_recv, NULL, _now_utc };

 ratls_anchor_t lRoots[ 4 ];
 ratls_io_t     lIO   = s_IO;
 char*          lpBuf = ( char* )malloc ( UPD_BUF + 2 * UPD_URL );
 const char*    lpPath;
 char           lHost[ 64 ];
 int            lSock, lHop, retVal = RATLS_ERR_HTTP;

 if ( !lpBuf ) return RATLS_ERR_MEM;

 lRoots[ 0 ].der = sectigo_e46_der;  lRoots[ 0 ].len = size_sectigo_e46_der;    /* github.com */
 lRoots[ 1 ].der = sectigo_r46_der;  lRoots[ 1 ].len = size_sectigo_r46_der;
 lRoots[ 2 ].der = isrgrootx1_der;   lRoots[ 2 ].len = size_isrgrootx1_der;     /* the download host */
 lRoots[ 3 ].der = isrg_root_x2_der; lRoots[ 3 ].len = size_isrg_root_x2_der;

 lIO.ctx = &lSock;

 for ( lHop = 0; lHop < 4; ++lHop ) {

  char* lpNext = lpBuf + UPD_BUF + ( lHop & 1 ) * UPD_URL;   /* the path may point into the other one */

  retVal = RATLS_ERR_HTTP;

  if (   !(  lpPath = UPD_SplitURL (  apURL, lHost, sizeof ( lHost )  )   )   ) break;

  if (  !raTlsClockOk ( _now_utc )  ) { retVal = RATLS_ERR_CLOCK; break; }   /* before anything is sent */

  if (   (  lSock = _connect ( lHost, 443 )  ) < 0   ) { retVal = lSock; break; }

  retVal = raTlsDownload (
   &lIO, lHost, lpPath, "SMS-updater", lRoots, 4, lpBuf, UPD_BUF, aMaxBody, _sink, apBody, lpNext, UPD_URL
  );

  SMS_NetClose ( lSock );

  if ( retVal != RATLS_ERR_REDIRECT ) break;

  if ( apBody -> m_Len ) { retVal = RATLS_ERR_HTTP; break; }

  apURL  = lpNext;
  retVal = RATLS_ERR_HTTP;   /* a fourth redirect is refused */

 }  /* end for */

 free ( lpBuf );

 return retVal;

}  /* end _fetch */

/* The device the running program sits on, when it is one an update can be written to:
 * a memory card, or a FAT drive ( USB, MX4SIO, i.LINK, MMCE ). */
static int _boot_ok ( void ) {

 return !strncmp ( s_BootPath, "mc", 2 ) || !strncmp ( s_BootPath, "mass", 4 ) || !strncmp ( s_BootPath, "mmce", 4 );

}  /* end _boot_ok */

/* The running program's file: its size, and whether it is the self-extracting build. -1 = unreadable. */
static int _boot_file ( int* apfPacked ) {

 unsigned char lHdr[ 64 ];
 int           lFD = fioOpen ( s_BootPath, O_RDONLY );
 int           retVal = -1;

 if ( lFD < 0 ) return -1;

 if (   fioRead (  lFD, lHdr, sizeof ( lHdr )  ) == ( int )sizeof ( lHdr ) && UPD_IsELF (  lHdr, sizeof ( lHdr )  )   ) {

  *apfPacked = UPD_IsPacked (  lHdr, sizeof ( lHdr )  );
  retVal     = fioLseek ( lFD, 0, SEEK_END );

 }  /* end if */

 fioClose ( lFD );

 return retVal;

}  /* end _boot_file */

/* Free space on the memory card the program runs from, in bytes; -1 when the card does not say. */
static int _mc_free ( void ) {

 int lType = 0, lFree = -1, lFmt = 0, lRes = -1, i;

 for ( i = 0; i < 8; ++i ) {
  if (  mcGetInfo ( s_BootPath[ 2 ] == '1', 0, &lType, &lFree, &lFmt ) < 0  ) return -1;
  mcSync ( 0, NULL, &lRes );
  if ( lRes >= 0 ) break;
 }  /* end for */

 return lRes == 0 && lType == 2 && lFree >= 0 ? lFree * 1024 : -1;

}  /* end _mc_free */

/* Overwrites the program in place. 0, or the step that failed with its error in *apErr. */
static int _put_file ( const unsigned char* apData, int aSize, int* apErr ) {

 int lFD = fioOpen ( s_BootPath, O_CREAT | O_WRONLY | O_TRUNC );
 int lPos, lRes = 0;

 if ( lFD < 0 ) { *apErr = lFD; return 1; }

 for ( lPos = 0; lPos < aSize; lPos += lRes ) {

  int lPart = aSize - lPos > 16384 ? 16384 : aSize - lPos;

  lRes = fioWrite (  lFD, ( void* )( apData + lPos ), lPart  );

  if ( lRes != lPart ) { *apErr = lRes; fioClose ( lFD ); return 2; }

  GUI_Progress (   _str ( &STR_UPD_WRITING ), ( int )(  ( long long )( lPos + lRes ) * 100 / aSize  ), 0   );

 }  /* end for */

 if (   (  lRes = fioClose ( lFD )  ) < 0   ) { *apErr = lRes; return 3; }

 return 0;

}  /* end _put_file */

void SMS_UpdateCheck ( void ) {

#ifdef SMS_DIAG
 static const char s_Plain [] = "Simple-Media-System-DIAG.elf";
 static const char s_Packed[] = "Simple-Media-System-DIAG.elf";
#else
 static const char s_Plain [] = "Simple-Media-System.elf";
 static const char s_Packed[] = "Simple-Media-System-packed.elf";
#endif  /* SMS_DIAG */

 UpdBody       lSums, lELF;
 unsigned char lWant[ 32 ], lHave[ 32 ];
 unsigned int  lBuild = 0;
 char          lID[ 16 ], lMsg[ 160 ], lURL[ 160 ];
 const char*   lpName;
 int           lfPacked = 0, lOld, lRes, lErr = 0;

 if (  !_boot_ok ()  ) { GUI_Error (  _str ( &STR_UPD_DEVICE )  ); return; }

 if (  !_net_up ()  ) return;

/* After the network is up: taking the adapter from UDPFS restarts the device drivers, and
 * the file has to be reachable again before anything is promised. */
 if (   (  lOld = _boot_file ( &lfPacked )  ) < 0   ) { GUI_Error (  _str ( &STR_UPD_DEVICE )  ); return; }

 GUI_Status (  _str ( &STR_UPD_CHECKING )  );

 memset (  &lSums, 0, sizeof ( lSums )  );
 memset (  &lELF,  0, sizeof ( lELF  )  );

 lSums.m_Size  = UPD_MAX_SUMS;
 lSums.m_pData = ( unsigned char* )malloc ( UPD_MAX_SUMS + 1 );

 if ( !lSums.m_pData ) { _report ( RATLS_ERR_MEM ); return; }

 lRes   = _fetch ( UPD_SUMS, UPD_MAX_SUMS, &lSums );
 lpName = lfPacked ? s_Packed : s_Plain;

 if ( lRes > 0 ) {

  lSums.m_pData[ lSums.m_Len ] = '\x00';

  if (   !UPD_ParseSums (  ( char* )lSums.m_pData, lpName, lWant, &lBuild, lID, sizeof ( lID )  )   ) lRes = RATLS_ERR_HTTP;

 }  /* end if */

 free ( lSums.m_pData );

 if ( lRes <= 0 ) { _report ( lRes ); return; }

 if ( lBuild <= ( unsigned int )SMS_BUILD_TIME ) { GUI_Status (  _str ( &STR_UPD_NONE )  ); return; }

 snprintf (  lMsg, sizeof ( lMsg ), _str ( &STR_UPD_ASK ), lID  );

 if (  !GUI_Question ( lMsg )  ) return;

 strcpy ( lURL, UPD_ASSETS );
 strcat ( lURL, lpName     );

 lELF.m_fShow = 1;
 GUI_Progress (  _str ( &STR_UPD_DOWNLOADING ), 0, 1  );

 lRes = _fetch ( lURL, UPD_MAX_ELF, &lELF );

 if ( lRes <= 0 ) {

  _report ( lELF.m_Size && !lELF.m_pData ? RATLS_ERR_MEM : lRes );

 } else {

  mbedtls_sha256 ( lELF.m_pData, lELF.m_Len, lHave, 0 );

  if (  lELF.m_Len != lRes || memcmp ( lWant, lHave, 32 ) || !UPD_IsELF ( lELF.m_pData, lELF.m_Len )  )

   GUI_Error (  _str ( &STR_UPD_BAD )  );

/* A memory card that says how much room it has: the new file takes the old one's place. */
  else if (  s_BootPath[ 1 ] == 'c' && ( lErr = _mc_free () ) >= 0 && lErr + lOld < lELF.m_Len + 2048  )

   GUI_Error (  _str ( &STR_UPD_MCFULL )  );

  else for ( ; ; ) {

   GUI_Progress (  _str ( &STR_UPD_WRITING ), 0, 1  );

   if (   !(  lRes = _put_file ( lELF.m_pData, lELF.m_Len, &lErr )  )   ) {
    GUI_Status (  _str ( &STR_UPD_DONE )  );
    break;
   }  /* end if */

/* The file is incomplete now: offer to write it again from the copy still in memory. */
   snprintf (  lMsg, sizeof ( lMsg ), _str ( &STR_UPD_WRITE_ERR ), lRes, lErr  );

   if (  !GUI_Question ( lMsg )  ) break;

  }  /* end for */

 }  /* end else */

 if ( lELF.m_pData ) free ( lELF.m_pData );

}  /* end SMS_UpdateCheck */
#endif  /* BDM */
