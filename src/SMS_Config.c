/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2005 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#include "SMS.h"
#include "SMS_EE.h"
#include "SMS_Config.h"
#include "SMS_GS.h"
#include "SMS_MC.h"
#include "SMS_Locale.h"
#include "SMS_FileContext.h"
#include "SMS_IOP.h"

#include <malloc.h>
#include <fileio.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern void _check_dc_offset ( void );
extern unsigned char g_IconSMS[ 2020 ] __attribute__(   (  section( ".data" )  )   );

SMSConfig g_Config __attribute__(   (  section( ".data" )  )   );

static unsigned int s_DefPalette[ 16 ] __attribute__(   (  section( ".data" )  )   ) = {
 GS_SET_RGBAQ( 0x00, 0x00, 0x00, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x00, 0x00, 0x40, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x00, 0x40, 0x00, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x00, 0x40, 0x40, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x40, 0x00, 0x00, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x40, 0x00, 0x40, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x40, 0x40, 0x00, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x40, 0x40, 0x40, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x80, 0x80, 0x80, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x00, 0x00, 0xFF, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x00, 0xFF, 0x00, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0x00, 0xFF, 0xFF, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0xFF, 0x00, 0x00, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0xFF, 0x00, 0xFF, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0xFF, 0xFF, 0x00, 0x80, 0x00 ),
 GS_SET_RGBAQ( 0xFF, 0xFF, 0xFF, 0x80, 0x00 )
};

unsigned int g_Palette[ 16 ] __attribute__(   (  section( ".bss" )  )   );

char g_SMSPal[ 13 ] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "/SMS/SMS.pal";
char g_SMSSMB[ 128 ] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mc0:/SMS/SMS.smb";   /* server list; tracks the SMS.cfg directory ( CWD on FS boots ) via _derive_smb_path */

static char s_pASCII [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mc0:SMS/ascii.mtf";
static char s_pLatin2[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mc0:SMS/latin2.mtf";
static char s_pCyrill[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mc0:SMS/cyrillic.mtf";
static char s_pLatin1[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mc0:SMS/latin1.mtf";
static char s_pGreek [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mc0:SMS/greek.mtf";
static char s_pSMS   [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "/SMS";
static char s_pSMSCfg[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "/SMS/SMS.cfg";
static char s_pIcoSys[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mc0:SMS/icon.sys";
static char s_pSMSIcn[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mc0:/SMS/SMS.icn";
static char s_pMC0SMC[ 128 ] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mc0:/SMS/SMS.cfg";
static int  s_CfgOnFS __attribute__(   (  section( ".data" )  )   );   /* 1 = config is a plain filesystem file ( non-mc boot -> CWD ), not a memory-card save */
static int  s_CfgFallback __attribute__(   (  section( ".data" )  )   );   /* 1 = non-re-mountable boot ( smb / host / cdrom ): resolve an attached FS device in SMS_IOPInit; mc0: only if none is found */
char g_pBootDir[ 128 ] __attribute__(   (  section( ".bss" )  )   );   /* "<dev>/path/" of the ELF when launched from a non-mc device ( for CWD skins ); empty on mc boots */
static char s_pPS2D  [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "PS2D";
static char s_pSMSICN[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "SMS.icn";

static const char* s_pFontNames[ 5 ] __attribute__(   (  section( ".data" )  )   ) = {
 s_pASCII, s_pLatin2, s_pCyrill, s_pLatin1, s_pGreek
};

void SMS_SetPalette ( const unsigned int* apPal ) {

 if ( !apPal ) apPal = s_DefPalette;

 memcpy ( g_Palette, apPal, 64 );

}  /* end SMS_SetPalette */

void SMS_SetMCSlot ( char aSlot ) {

 s_pASCII [ 2 ] = aSlot;
 s_pLatin2[ 2 ] = aSlot;
 s_pCyrill[ 2 ] = aSlot;
 s_pLatin1[ 2 ] = aSlot;
 s_pGreek [ 2 ] = aSlot;
 s_pIcoSys[ 2 ] = aSlot;
 g_pMC0SMS[ 2 ] = aSlot;
 s_pSMSIcn[ 2 ] = aSlot;
 s_pMC0SMC[ 2 ] = aSlot;
 g_SMSSMB [ 2 ] = aSlot;

}  /* end SMS_SetMCSlot */

/* Keep the SMB server list ( g_SMSSMB ) in the SAME directory as SMS.cfg
 * ( s_pMC0SMC ), so everything-but-IPCONFIG lands together: CWD on a filesystem
 * boot, the resolved fallback device on a card-less SMB boot, mc?:/SMS otherwise.
 * Only the directory is copied across; the file name is always SMS.smb. */
static void _derive_smb_path ( void ) {

 int i, lLast = -1;

 for ( i = 0; s_pMC0SMC[ i ] && i < ( int )sizeof ( g_SMSSMB ) - 9; ++i )
  if ( s_pMC0SMC[ i ] == '/' || s_pMC0SMC[ i ] == ':' ) lLast = i;

 for ( i = 0; i <= lLast; ++i ) g_SMSSMB[ i ] = s_pMC0SMC[ i ];
 strcpy ( &g_SMSSMB[ lLast + 1 ], "SMS.smb" );

}  /* end _derive_smb_path */

void SMS_ConfigSetCWD ( const char* apELFPath ) {
/* Launched from a non-memory-card device ( USB / HDD / MX4SIO / etc., where mc0:
 * may be dead ): keep settings next to the ELF -- <boot dir>SMS.cfg -- instead of
 * on the memory card. The mc-save icon path is skipped for this case ( s_CfgOnFS ). */
 int i, lLast = -1;

/* CWD config only works for a boot device SMS can RE-MOUNT at config time ( the
 * lazy-load in SMS_IOPInit: "mass" = USB / MX4SIO / iLink, "mmce", "pfs" / "hdd"
 * = internal HDD ). A device SMS cannot re-establish after its IOP reset -- SMB
 * ( no stored share credentials; the share is usually read-only ), host:, cdrom
 * -- CANNOT hold config next to the ELF ( every SMS.cfg save/load there fails --
 * the "Error" on Save when running from OPL-over-SMB ). Rather than fall back to
 * the memory card, flag those for the FS fallback: SMS_IOPInit resolves an
 * attached, writable USB / MMCE device and keeps SMS.cfg on THAT ( mc0: only when
 * nothing else is attached ). Keeps config off the card, matching the CWD intent.
 * ( The SMB server list SMS.smb + IPCONFIG.DAT are still mc0:-pinned separately. ) */
 if (  strncmp ( apELFPath, "mass", 4 ) != 0 && strncmp ( apELFPath, "mmce", 4 ) != 0 &&
       strncmp ( apELFPath, "pfs",  3 ) != 0 && strncmp ( apELFPath, "hdd",  3 ) != 0  ) { s_CfgFallback = 1; return; }

 for ( i = 0; apELFPath[ i ] && i < ( int )sizeof ( s_pMC0SMC ) - 9; ++i )
  if ( apELFPath[ i ] == '/' || apELFPath[ i ] == ':' || apELFPath[ i ] == '\\' ) lLast = i;

 if ( lLast < 0 ) return;   /* no path separator -> leave the mc0: default */

 for ( i = 0; i <= lLast; ++i ) s_pMC0SMC[ i ] = apELFPath[ i ];
 strcpy ( &s_pMC0SMC[ lLast + 1 ], "SMS.cfg" );

 for ( i = 0; i <= lLast; ++i ) g_pBootDir[ i ] = apELFPath[ i ];   /* keep the boot dir for CWD skin loading */
 g_pBootDir[ lLast + 1 ] = '\x00';

 s_CfgOnFS = 1;

 _derive_smb_path ();   /* SMB server list -> CWD too */

}  /* end SMS_ConfigSetCWD */

/* CWD-first asset probe. On a re-mountable FS boot ( g_pBootDir set ) where
 * "<bootdir><apName>" exists, copy that fio path into apBuf and return 1. Return 0
 * ( mc / SMB boot, or the CWD copy is absent ) -> the caller falls back to its
 * existing memory-card read, so no on-card asset is ever lost. The probe is a bare
 * fioOpen ( only errors, never hangs, on an unmounted device ) -> boot-safe once
 * past GUI_Initialize. Assets sit FLAT next to the ELF, same as SMS.cfg / SMS.smb. */
int SMS_ConfigAssetPath ( char* apBuf, int aSize, const char* apName ) {

 int lLen = strlen ( g_pBootDir );
 int lFD;

 if ( !g_pBootDir[ 0 ] ) return 0;                             /* mc / SMB boot -> mc read */
 if (  lLen + ( int )strlen ( apName ) >= aSize  ) return 0;

 strcpy ( apBuf, g_pBootDir );                                 /* "<dev>/path/" ( trailing sep ) */
 strcpy ( apBuf + lLen, apName );

 lFD = fioOpen ( apBuf, O_RDONLY );
 if ( lFD < 0 ) return 0;                                      /* CWD miss -> mc fallback */
 fioClose ( lFD );

 return 1;

}  /* end SMS_ConfigAssetPath */

static void _load_font ( unsigned int anIndex ) {

 char        lP[ 128 ];
 const char* lpName = s_pFontNames[ anIndex ];
 const char* lpBase = strrchr ( lpName, '/' );
 int         lFD;

 lpBase = lpBase ? lpBase + 1 : lpName;   /* bare "<name>.mtf" for the CWD probe */
 lFD    = fioOpen (  SMS_ConfigAssetPath ( lP, sizeof ( lP ), lpBase ) ? lP : lpName, O_RDONLY  );

 if ( lFD >= 0 ) {

  unsigned int lFontSize;
  s64          lSize  = fioLseek ( lFD, 0, SEEK_END );
  void*        lpBuff = GSFont_Get ( anIndex, &lFontSize );

  if ( lSize > 0 ) {

   if ( lSize > lFontSize ) {
    void* lpNew = malloc ( lSize );
    if ( !lpNew ) { fioClose ( lFD ); return; }   /* oversized font + OOM -> keep the built-in, never fioRead into NULL */
    lpBuff = lpNew;
   }  /* end if */

   fioLseek ( lFD, 0, SEEK_SET );
   fioRead ( lFD, lpBuff, lSize );

   GSFont_Set ( anIndex, lpBuff );

  }  /* end if */

  fioClose ( lFD );

 }  /* end if */

}  /* end _load_font */

void SMS_LoadSMBInfo ( void ) {

 FileContext* lpFileCtx;
 SMS_List*    lpList;

 if ( g_Config.m_pSMBList )
  SMS_ListDestroy ( lpList = g_Config.m_pSMBList, 0 );
 else g_Config.m_pSMBList = lpList = SMS_ListInit ();

 if (   (  lpFileCtx = STIO_InitFileContext ( g_SMSSMB, NULL )  )   ) {

  while ( 1 ) {

   SMBLoginInfo lInfo;
   char         lDescr[ 64 ];
   char         lPort [ 8 ];

   memset (  &lInfo, 0, sizeof ( lInfo )  );

   File_GetString (  lpFileCtx, lInfo.m_ServerIP,   sizeof ( lInfo.m_ServerIP   )  );
   File_GetString (  lpFileCtx, lInfo.m_ServerName, sizeof ( lInfo.m_ServerName )  );
   File_GetString (  lpFileCtx, lInfo.m_ClientName, sizeof ( lInfo.m_ClientName )  );
   File_GetString (  lpFileCtx, lInfo.m_UserName,   sizeof ( lInfo.m_UserName   )  );
   File_GetString (  lpFileCtx, lInfo.m_Password,   sizeof ( lInfo.m_Password   )  );
   File_GetString (  lpFileCtx, lDescr,             sizeof ( lDescr             )  );

   lPort[ 0 ] = '\x00';
   /* Port/Share exist only in the new format. A legacy 6-line record ends right
    * after the description; reading further would steal the next record's lines,
    * so only consume them when the record isn't already exhausted. */
   if (  !FILE_EOF( lpFileCtx )  ) {
    File_GetString (  lpFileCtx, lPort,         sizeof ( lPort         )  );
    File_GetString (  lpFileCtx, lInfo.m_Share, sizeof ( lInfo.m_Share )  );
   }  /* end if */

   {  /* Only an all-digit token is a port; a misaligned IP/name line is not. */
    char* lpDig = lPort;
    int   lfDig = ( *lpDig != '\x00' );
    for ( ; *lpDig; ++lpDig ) if ( *lpDig < '0' || *lpDig > '9' ) { lfDig = 0; break; }
    lInfo.m_Port = lfDig ? atoi ( lPort ) : 0;
   }
   if ( lInfo.m_Port <= 0 || lInfo.m_Port > 65535 ) lInfo.m_Port = 1445;

   if ( lInfo.m_ServerIP  [ 0 ] &&
        lInfo.m_ServerName[ 0 ] &&
        lInfo.m_ClientName[ 0 ] &&
        strlen ( lInfo.m_ServerName ) < 14
   ) {
    SMBLoginInfo* lpInfo = ( SMBLoginInfo* )malloc (  sizeof ( SMBLoginInfo )  );
    g_IOPFlags |= SMS_IOPF_SMBINFO;
    memcpy (  lpInfo, &lInfo, sizeof ( SMBLoginInfo )  );
    strupr ( lpInfo -> m_ServerName );
    SMS_ListPushBack ( lpList, lDescr[ 0 ] ? lDescr : lInfo.m_ServerName ) -> m_Param = ( unsigned int )lpInfo;
    lpInfo -> m_fAsync = 1;
   }  /* end if */

   if (  FILE_EOF( lpFileCtx )  ) break;

  }  /* end while */

  lpFileCtx -> Destroy ( lpFileCtx );

 }  /* end if */

}  /* end SMS_LoadSMBInfo */

static void _smb_put_line ( int aFD, const char* apStr ) {

 if ( apStr ) fioWrite (  aFD, ( void* )apStr, strlen ( apStr )  );

 fioWrite ( aFD, "\n", 1 );

}  /* end _smb_put_line */

void SMS_SaveSMBInfo ( void ) {

 SMS_ListNode* lpNode;
 int           lFD;

 if ( !g_Config.m_pSMBList ) return;

 if ( !s_CfgOnFS ) fioMkdir ( g_pMC0SMS );   /* mc target -> ensure mc?:/SMS; on FS the CWD / device-root dir already exists */

 lFD = fioOpen ( g_SMSSMB, O_CREAT | O_WRONLY | O_TRUNC );

 if ( lFD < 0 ) return;

 lpNode = g_Config.m_pSMBList -> m_pHead;

 while ( lpNode ) {

  SMBLoginInfo* lpInfo = ( SMBLoginInfo* )( unsigned int )lpNode -> m_Param;
  char          lPort[ 8 ];

  sprintf (  lPort, "%d", lpInfo -> m_Port ? lpInfo -> m_Port : 1445  );

  _smb_put_line ( lFD, lpInfo -> m_ServerIP   );
  _smb_put_line ( lFD, lpInfo -> m_ServerName );
  _smb_put_line ( lFD, lpInfo -> m_ClientName );
  _smb_put_line ( lFD, lpInfo -> m_UserName   );
  _smb_put_line ( lFD, lpInfo -> m_Password   );
  _smb_put_line (  lFD, _STR( lpNode )  );
  /* Appended smbman fields ( legacy readers stop after the description ). */
  _smb_put_line ( lFD, lPort                  );
  _smb_put_line ( lFD, lpInfo -> m_Share      );

  lpNode = lpNode -> m_pNext;

 }  /* end while */

 fioClose ( lFD );

}  /* end SMS_SaveSMBInfo */

void SMS_LoadPalette ( void ) {

 int  i, lLoaded = 0;
 char lP[ 128 ];

 if (  SMS_ConfigAssetPath ( lP, sizeof ( lP ), "SMS.pal" )  ) {   /* CWD copy next to the ELF */
  int lF = fioOpen ( lP, O_RDONLY );
  if ( lF >= 0 ) { fioRead ( lF, s_DefPalette, 64 ); fioClose ( lF ); lLoaded = 1; }
 }  /* end if */

 if ( !lLoaded ) {   /* memory-card copy -- via fio ( libmc cannot see a fio-written SMS.pal, so an in-app "Update palette file" never applied on an mc boot ) */
  int lF;
  lP[ 0 ] = 'm';
  lP[ 1 ] = 'c';
  lP[ 2 ] = '0' + g_MCSlot;
  lP[ 3 ] = ':';
  strcpy ( &lP[ 4 ], g_SMSPal );   /* g_SMSPal = "/SMS/SMS.pal" -> "mc<slot>:/SMS/SMS.pal" */
  lF = fioOpen ( lP, O_RDONLY );
  if ( lF >= 0 ) { fioRead ( lF, s_DefPalette, 64 ); fioClose ( lF ); }
 }  /* end if */

 for ( i = 0; i < 16; ++i ) s_DefPalette[ i ] = ( s_DefPalette[ i ] & 0x00FFFFFF ) | 0x60000000;

}  /* end SMS_LoadPalette */

#ifdef BDM
/* libmc's mcGetInfo reports a transient "card changed" status (a negative value)
 * on the first call after init; a second call returns the stable status. The
 * legacy custom MC driver tolerated a single query, but on libmc a single query
 * intermittently looked like "no card" and skipped config save/load. Retry until
 * the status stabilises so settings reliably persist. Returns 0 when the card is
 * present and usable. */
static int _mc_get_info ( void ) {

 int lRes = -1, i;

 for ( i = 0; i < 8; ++i ) {
  MC_GetInfo ( g_MCSlot, 0, &lRes, &lRes, &lRes );
  MC_Sync ( &lRes );
  if ( lRes >= 0 ) break;
 }  /* end for */

 return lRes;

}  /* end _mc_get_info */

int SMS_MCPresent ( void ) { return _mc_get_info () > -2; }   /* memory card in the configured slot present + usable? */
#endif  /* BDM */

int SMS_ConfigOnFS ( void ) { return s_CfgOnFS; }   /* 1 = booted from a filesystem device ( config is a plain file on the boot drive, not an mc save ) */

const char* SMS_ConfigPath ( void ) { return s_pMC0SMC; }   /* the SMS.cfg path derived from argv[0]; its device prefix ( mmce/mass/pfs ) tells us what to mount for the config */

int SMS_ConfigFallback ( void ) { return s_CfgFallback; }   /* 1 = SMB/host/cdrom boot: SMS_IOPInit must resolve an attached FS device for config */

/* The FS-fallback resolver ( SMS_IOPInit, non-re-mountable boot ) found a writable
 * device -- pin SMS.cfg to it and switch to the FS save/load path. apPath is a
 * short device-root path ( e.g. "mass0:/SMS.cfg", <= 15 bytes ) that fits s_pMC0SMC. */
void SMS_ConfigUseFSPath ( const char* apPath ) { strcpy ( s_pMC0SMC, apPath ); s_CfgOnFS = 1; _derive_smb_path (); }

int SMS_LoadConfig ( void  ) {

 int retVal = 0;
 int lRes;

 g_Config.m_BrowserABCIdx    = 10;  /* active accent: blue (match inactive; graphics wants the file-view frame to stay blue when browsing, not flip to white) */
 g_Config.m_BrowserIBCIdx    = 10;  /* inactive panels / info bar / spectrum: blue  ( idx 10 -> g_Palette[9]  = 0,0,FF )  ( was red 13 ) -> jellyfish theme */
 g_Config.m_BrowserTxtIdx    = 16;  /* browser text: white ( was yellow 15 ) */
 g_Config.m_NetworkFlags     = 0;  /* no-settings default: auto-start NO device (clean boot); user enables HDD/USB/MX4SIO/net from the menu, persisted once saved */
 g_Config.m_PlayerVolume     = 12;
 g_Config.m_PlayerAC3RL      =  6;
 g_Config.m_DisplayMode      = GSVideoMode_Default;
 g_Config.m_DisplayCharset   = GSCodePage_WinLatin1;
 g_Config.m_PlayerFlags      = SMS_PF_SUBS | SMS_PF_ANIM | SMS_PF_TIME;
 g_Config.m_PlayerSCNIdx     = 15;
 g_Config.m_PlayerSCBIdx     = 16;
 g_Config.m_PlayerSCIIdx     = 11;
 g_Config.m_PlayerSCUIdx     = 10;
 g_Config.m_BrowserSCIdx     = 12;  /* selection highlight: cyan ( idx 12 -> g_Palette[11] = 0,FF,FF ) bright, pops ( was white 16 ) -> jellyfish theme */
 g_Config.m_BrowserSBCIdx    = 16;  /* text secondary / shadow: white */
 g_Config.m_PlayerSubOffset  = 32;
 g_Config.m_PlayerVBCIdx     = 11;
 g_Config.m_PlayerSBCIdx     = 11;
 g_Config.m_ScrollBarNum     = 32;
 g_Config.m_ScrollBarPos     = SMScrollBarPos_Bottom;
 g_Config.m_PlayerBrightness = 12;

 *(  ( unsigned int* )&g_Config.m_PAR[ 0 ]  ) = 0x3F6EEEEF;
 *(  ( unsigned int* )&g_Config.m_PAR[ 1 ]  ) = 0x3F888889;

 strcpy ( g_Config.m_Language, g_pDefStr );

 g_Config.m_DispWH[ 0 ][ 0 ] =  640;  /* NTSC     */
 g_Config.m_DispWH[ 0 ][ 1 ] =  448;
 g_Config.m_DispWH[ 1 ][ 0 ] =  640;  /* PAL      */
 g_Config.m_DispWH[ 1 ][ 1 ] =  512;
 g_Config.m_DispWH[ 2 ][ 0 ] =  640;  /* DTV480p  */
 g_Config.m_DispWH[ 2 ][ 1 ] =  512;
 g_Config.m_DispWH[ 3 ][ 0 ] =  640;  /* DTV576p  */
 g_Config.m_DispWH[ 3 ][ 1 ] =  512;
 g_Config.m_DispWH[ 4 ][ 0 ] = 1216;  /* DTV720p  */
 g_Config.m_DispWH[ 4 ][ 1 ] =  676;
 g_Config.m_DispWH[ 5 ][ 0 ] = 1820;  /* DTV1080i */
 g_Config.m_DispWH[ 5 ][ 1 ] = 1018;
 g_Config.m_DispWH[ 6 ][ 0 ] =  640;  /* VESA60Hz */
 g_Config.m_DispWH[ 6 ][ 1 ] =  480;
 g_Config.m_DispWH[ 7 ][ 0 ] =  640;  /* VESA75Hz */
 g_Config.m_DispWH[ 7 ][ 1 ] =  480;

 g_Config.m_SyncPar[ 0 ][ 0 ] =   0;  /* NTSC     */
 g_Config.m_SyncPar[ 0 ][ 1 ] = 248;
 g_Config.m_SyncPar[ 0 ][ 2 ] = 248;
 g_Config.m_SyncPar[ 1 ][ 0 ] =   0;  /* PAL      */
 g_Config.m_SyncPar[ 1 ][ 1 ] = 304;
 g_Config.m_SyncPar[ 1 ][ 2 ] = 304;
 g_Config.m_SyncPar[ 2 ][ 0 ] =   0;  /* DTV480p  */
 g_Config.m_SyncPar[ 2 ][ 1 ] = 464;
 g_Config.m_SyncPar[ 2 ][ 2 ] = 480;
 g_Config.m_SyncPar[ 3 ][ 0 ] =   0;  /* DTV576p  */
 g_Config.m_SyncPar[ 3 ][ 1 ] = 464;
 g_Config.m_SyncPar[ 3 ][ 2 ] = 480;
 g_Config.m_SyncPar[ 4 ][ 0 ] =   0;  /* DTV720p  */
 g_Config.m_SyncPar[ 4 ][ 1 ] = 660;
 g_Config.m_SyncPar[ 4 ][ 2 ] = 648;
 g_Config.m_SyncPar[ 5 ][ 0 ] =   0;  /* DTV1080i */
 g_Config.m_SyncPar[ 5 ][ 1 ] = 412;
 g_Config.m_SyncPar[ 5 ][ 2 ] = 480;
 g_Config.m_SyncPar[ 6 ][ 0 ] =   0;  /* VESA60Hz */
 g_Config.m_SyncPar[ 6 ][ 1 ] = 448;
 g_Config.m_SyncPar[ 6 ][ 2 ] = 480;
 g_Config.m_SyncPar[ 7 ][ 0 ] =   0;  /* VESA75Hz */
 g_Config.m_SyncPar[ 7 ][ 1 ] = 448;
 g_Config.m_SyncPar[ 7 ][ 2 ] = 460;

 g_Config.m_MP3AutoPar = 5;
 g_Config.m_CDVDSpeed  = 1;
 g_Config.m_ColorDepth = 0;

 SMS_ListPushBack (  g_Config.m_pSkinList = SMS_ListInit (), g_EmptyStr  );
 SMS_ListPushBack (  g_Config.m_pMBFList  = SMS_ListInit (), g_EmptyStr  );

#ifdef BDM
 lRes = s_CfgOnFS ? 0 : _mc_get_info ();
#else
 if ( s_CfgOnFS ) lRes = 0;
 else { MC_GetInfo ( g_MCSlot, 0, &lRes, &lRes, &lRes ); MC_Sync ( &lRes ); }
#endif

 if ( lRes > -2 ) {

  /* Read the config the SAME way SMS_SaveConfig writes it -- via fio on the
   * mc0: path. The legacy split (save = fio, load = libmc) is incoherent on the
   * modern iomanX + mcman stack: a fio-created dir/file is not seen by libmc's
   * MC_GetDir/MC_OpenS, so settings appeared to save but never loaded back. */
  int lFD = fioOpen ( s_pMC0SMC, O_RDONLY );

  if ( lFD >= 0 ) {

   int lLen = fioRead ( lFD, &g_Config, 4 );

   if ( lLen == 4 && g_Config.m_Version == 14 ) {

    lLen = fioRead ( lFD, &g_Config.m_DisplayMode, 892 );

    if ( lLen == 892 ) retVal = 1;

   }  /* end if */

   fioClose ( lFD );

  }  /* end if */

  SMS_LoadPalette ();

  for ( lRes = 0; lRes < 5; ++lRes ) _load_font ( lRes );

  SMS_EEScanDir ( g_pSMSSkn, g_pExtSMI, g_Config.m_pSkinList );

  if ( g_pBootDir[ 0 ] ) {   /* also list CWD skins ( "<boot dir>Skins/" ); SMS_EEScanDir dedups */
   char lSkinDir[ 128 ];
   strcpy ( lSkinDir, g_pBootDir );
   strcat ( lSkinDir, "Skins" );
   SMS_EEScanDir ( lSkinDir, g_pExtSMI, g_Config.m_pSkinList );
  }  /* end if */

 }  /* end if */

 g_Config.m_Version       = 14;
 g_Config.m_BrowserFlags &= ~SMS_BF_UDFL;
 g_Config.m_SkinName[ sizeof( g_Config.m_SkinName ) - 1 ] = '\x00';   /* guard: a corrupt config field must stay in-bounds */

 if ( g_Config.m_PlayerAC3RL < 1 ) g_Config.m_PlayerAC3RL = 6;

 SMS_LoadSMBInfo   ();
 SMS_SetDirButtons ();

 if (  !( g_IOPFlags & SMS_IOPF_SMBINFO )  ) g_Config.m_NetworkFlags &= ~SMS_DF_SMB;
#ifdef EMBEDDED
 g_Config.m_BrowserFlags &= 0x0FFFFFFF;
#endif  /* EMBEDDED */
 SMS_SetPalette ( NULL );

 return retVal;

}  /* end SMS_LoadCondig */

int SMS_SaveConfig ( void ) {

 int retVal = 0;
 int lRes;

 if ( s_CfgOnFS ) {   /* non-mc boot: write settings next to the ELF, no memory-card save/icon */

  int lFD = fioOpen ( s_pMC0SMC, O_WRONLY | O_CREAT );

/* A USB replug re-enumerates the bus, so the boot drive can renumber ( massN
 * shifts to a different unit ), leaving the config path -- pinned to the boot
 * unit from argv[0] -- aimed at the wrong / absent drive, so the open above fails
 * ( "saved once, then save errors" ). ONLY on that failure, and only for a
 * "massN:" path, scan the other units for the one that still holds our SMS.cfg
 * ( written on an earlier save ) and retry there, updating the unit so later
 * saves self-heal too. The probe is READ-ONLY, so we never create a stray
 * SMS.cfg on the wrong drive. */
  if (  lFD < 0 && strncmp ( s_pMC0SMC, "mass", 4 ) == 0 && s_pMC0SMC[ 4 ] >= '0' && s_pMC0SMC[ 4 ] <= '9'  ) {

   char lUnit, lOrig = s_pMC0SMC[ 4 ];

   for ( lUnit = '0'; lUnit <= '3'; ++lUnit ) {

    int lProbe;

    if ( lUnit == lOrig ) continue;

    s_pMC0SMC[ 4 ] = lUnit;
    lProbe = fioOpen ( s_pMC0SMC, O_RDONLY );   /* does our cfg live on this unit now? */

    if ( lProbe >= 0 ) {

     fioClose ( lProbe );
     lFD = fioOpen ( s_pMC0SMC, O_WRONLY | O_CREAT );   /* yes -> save here */
     if ( lFD >= 0 ) _derive_smb_path ();   /* keep the SMB list on the same self-healed unit */
     break;

    }  /* end if */

   }  /* end for */

   if ( lFD < 0 ) s_pMC0SMC[ 4 ] = lOrig;   /* boot drive not found -> leave the path unchanged */

  }  /* end if */

  if ( lFD >= 0 ) {
   if (  fioWrite (  lFD, &g_Config, sizeof ( g_Config )  ) == sizeof ( g_Config )  ) retVal = 1;
   fioClose ( lFD );
  }  /* end if */

  return retVal;

 }  /* end if */

#ifdef BDM
 lRes = _mc_get_info ();
#else
 MC_GetInfo ( g_MCSlot, 0, &lRes, &lRes, &lRes );
 MC_Sync ( &lRes );
#endif

 if ( lRes > -2 ) {

/* libmc's MC_GetDir does NOT see the fio-created mc?:/SMS dir on the modern
 * iomanX + mcman stack, so the old MC_GetDir precheck read "dir absent" on EVERY
 * save after the first, and `!fioMkdir(existing)` == !(-4 EEXIST) == 0 -> the gate
 * went false and the config write was silently skipped ( "saves once, then Error"
 * -- the mc / SMB-with-card save bug ). Mirror SMS_SaveSMBInfo + the IP-config save:
 * create the dir via fio and accept 0 ( created ) or -4 ( already exists ). Coherent
 * with the fio SMS_LoadConfig; the real success test stays the fioWrite below. */
  lRes = fioMkdir ( g_pMC0SMS );

  if (  lRes == 0 || lRes == -4  ) {

   int lFD = fioOpen ( s_pIcoSys, O_RDONLY );

   if ( lFD < 0 ) {

    static int lBgClr[ 4 ][ 4 ] __attribute__(   (  section( ".data" )  )    ) = {
     {  68,  23, 116,  0 },
     { 255, 255, 255,  0 },
     { 255, 255, 255,  0 },
     {  68,  23, 116,  0 }
	};
    static float lLightDir[ 3 ][ 4 ] __attribute__(   (  section( ".data" )  )    ) = {
     {  0.5F,  0.5F,  0.5F, 0.0F },
     {  0.0F, -0.4F, -0.1F, 0.0F },
     { -0.5F, -0.5F,  0.5F, 0.0F }
	};
	static float lLightCol[ 3 ][ 4 ] __attribute__(   (  section( ".data" )  )    ) = {
     { 0.3F, 0.3F, 0.3F, 0.0F },
     { 0.4F, 0.4F, 0.4F, 0.0F },
     { 0.5F, 0.5F, 0.5F, 0.0F }
	};
	static float lAmb[ 4 ] __attribute__(   (  section( ".data" )  )    ) = { 0.5F, 0.5F, 0.5F, 0.0F };

    SMS_MCIcon lIcon; memset ( &lIcon, 0, sizeof ( SMS_MCIcon )  );

	strcpy ( lIcon.m_Header, s_pPS2D );
	strcpy_sjis (  ( short* )&lIcon.m_Title, s_pSMS + 1  );

	lIcon.m_Offset =   16;
	lIcon.m_Trans  = 0x60;

    memcpy (  lIcon.m_ClrBg,    lBgClr,    sizeof ( lBgClr    )  );
    memcpy (  lIcon.m_LightDir, lLightDir, sizeof ( lLightDir )  );
    memcpy (  lIcon.m_LightCol, lLightCol, sizeof ( lLightCol )  );
    memcpy (  lIcon.m_LightAmb, lAmb,      sizeof ( lAmb      )  );

    strcpy ( lIcon.m_View, s_pSMSICN );
    strcpy ( lIcon.m_Copy, s_pSMSICN );
    strcpy ( lIcon.m_Del,  s_pSMSICN );

 	lFD = fioOpen ( s_pIcoSys, O_WRONLY | O_CREAT );

	if ( lFD >= 0 ) {

     fioWrite (  lFD, &lIcon, sizeof ( lIcon )  );
     fioClose ( lFD );

     lFD = fioOpen ( s_pSMSIcn, O_WRONLY | O_CREAT );

     if ( lFD >= 0 ) {

      fioWrite (  lFD, g_IconSMS, sizeof ( g_IconSMS )  );
      fioClose ( lFD );

     }  /* end if */

    }  /* end if */

   } else fioClose ( lFD );

   lFD = fioOpen ( s_pMC0SMC, O_WRONLY | O_CREAT );

   if ( lFD >= 0 ) {

    if (  fioWrite (  lFD, &g_Config, sizeof ( g_Config )  ) == sizeof ( g_Config )  ) retVal = 1;

    fioClose ( lFD );

   }  /* end if */

  }  /* end if */

 }  /* end if */

 return retVal;

}  /* end SMS_SaveConfig */

void SMS_LoadXLT ( void ) {

 int   i, lFD;
 char* lppFonts[ 4 ] = {
  s_pLatin2, s_pCyrill, s_pLatin1, s_pGreek
 };
 char  lP[ 128 ];

 for ( i = 0; i < 4; ++i ) {

  const char* lpBase;

  lFD = strlen ( lppFonts[ i ] );
  lppFonts[ i ][ lFD - 1 ] = 'x';   /* .mtf -> .mtx */

  lpBase = strrchr ( lppFonts[ i ], '/' );
  lpBase = lpBase ? lpBase + 1 : lppFonts[ i ];

  lFD = fioOpen (  SMS_ConfigAssetPath ( lP, sizeof ( lP ), lpBase ) ? lP : lppFonts[ i ], O_RDONLY  );

  if ( lFD >= 0 ) {

   fioRead ( lFD, g_XLT[ i ], 256 );
   fioClose ( lFD );

  }  /* end if */

 }  /* end for */

}  /* end SMS_LoadXLT */
