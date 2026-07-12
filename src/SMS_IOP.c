/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2006-2008 Eugene Plotnikov <e-plotnikov@operamail.com>
# (c) 2005 USB support by weltall
# (c) 2005 HOST support by Ronald Andersson (AKA: dlanor)
# Special thanks to 'bigboss'/PS2Reality for valuable information
# about SifAddCmdHandler function
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#define NO_DEBUG 1
#include "SMS.h"
#include "SMS_IOP.h"
#include "SMS_Data.h"
#include "SMS_Config.h"
#include "SMS_GUI.h"
#include "SMS_PAD.h"
#include "SMS_Locale.h"
#include "SMS_SIF.h"
#include "SMS_SPU.h"
#include "SMS_Sounds.h"
#include "SMS_RC.h"
#include "SMS_SMB.h"
#include "SMS_FileDir.h"
#include "SMS_ioctl.h"
#include "SMS_Timer.h"

#include <kernel.h>
#include <sifrpc.h>
#include <iopheap.h>
#include <iopcontrol.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>

#include <lzma2.h>

#define SMSUTILS_RPC_ID 0x6D737573

extern void* _gp;

unsigned int g_IOPFlags;

#ifdef BDM
unsigned int g_Mx4sioMask;
unsigned int g_AtaMask;
unsigned int g_IlinkMask;
unsigned int g_MmceFlags;   /* pending MMCE connect events: bit0 = mmce0:, bit1 = mmce1: */
#endif

#ifdef BDM
#include <fileXio_rpc.h>
extern unsigned char filexio_irx [];
extern unsigned int size_filexio_irx;

extern unsigned char bdm_irx        [];
extern unsigned char bdmfs_fatfs_irx[];
extern unsigned char usbd_irx       [];
extern unsigned char usbmass_bd_irx [];
extern unsigned char mx4sio_bd_irx  [];
extern unsigned char ata_bd_irx     [];
extern unsigned char iLinkman_irx   [];
extern unsigned char IEEE1394_bd_irx[];
extern unsigned char mmceman_irx    [];

extern unsigned char sio2man_irx [];
extern unsigned char iomanx_irx  [];
extern unsigned char mcman_irx   [];
extern unsigned char mcserv_irx  [];
extern unsigned char padman_irx  [];
extern unsigned char smbman_irx  [];

extern unsigned int size_bdm_irx;
extern unsigned int size_bdmfs_fatfs_irx;
extern unsigned int size_usbd_irx;
extern unsigned int size_usbmass_bd_irx;
extern unsigned int size_mx4sio_bd_irx;
extern unsigned int size_ata_bd_irx;
extern unsigned int size_iLinkman_irx;
extern unsigned int size_IEEE1394_bd_irx;
extern unsigned int size_mmceman_irx;

extern unsigned int size_sio2man_irx;
extern unsigned int size_iomanx_irx;
extern unsigned int size_mcman_irx;
extern unsigned int size_mcserv_irx;
extern unsigned int size_padman_irx;
extern unsigned int size_smbman_irx;

extern unsigned int g_MassFlags;
/* Units ( 0..3 ) whose "connected" event has already been emitted into
 * g_MassFlags ( i.e. already present in the device strip ). g_MassFlags is a
 * one-shot event flag: the GUI loop consumes each set bit, ADDS the device and
 * clears the bit -- with no duplicate check. So re-setting a bit for a unit that
 * is already in the strip appends a second copy. s_MassAdded lets refresh emit a
 * connect event only for units that are NOT already added. */
static unsigned int s_MassAdded;
/* Which subsystem owns the internal ATA bus: 0 = free, 1 = SMS PFS HDD ( ps2atad ),
 * 2 = ATA-as-BDM ( ata_bd, which bundles its own atad ). Both register the same atad
 * IOP library, so only the first to start may own the bus -- the other backs off. */
static int s_AtaBusOwner;
/* MMCE units ( bit0=mmce0:, bit1=mmce1: ) already announced to the device strip,
 * so a re-probe doesn't append a duplicate ( same idea as s_MassAdded ). */
static unsigned int s_MmceAdded;
#else
static char s_pSIO2MAN[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "rom0:SIO2MAN";
static char s_pPADMAN [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "rom0:PADMAN";
static char s_pMCMAN  [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "rom0:MCMAN";
static char s_pMCSERV [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "rom0:MCSERV";
static char s_pUSBD   [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "USBD.IRX";
#endif

static char s_HDDArgs[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = {
 '-', 'o', '\x00', '2',      '\x00',
 '-', 'n', '\x00', '2', '0', '\x00'
};

static char s_PFSArgs[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = {
 '-', 'm', '\x00', '2',      '\x00',
 '-', 'o', '\x00', '2', '0', '\x00',
 '-', 'n', '\x00', '4', '0', '\x00'
};

static char s_pAudSrv [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "AUDSRV";
static char s_pPS2Dev9[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "PS2DEV9";
static char s_pPS2ATAD[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "PS2ATAD";
static char s_pPS2HDD [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "PS2HDD";
static char s_pPS2FS  [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "PS2FS";
static char s_pPS2POff[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "POWEROFF";
static char s_pUDNL   [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "rom0:UDNL rom0:EELOADCNF";
static char s_pLIBSD  [] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "rom0:LIBSD";

struct {

 const char* m_pName;
 void*       m_pBuffer;
 int         m_BufSize;
 int         m_nArgs;
 void*       m_pArgs;
 int         m_fComp;   /* 1 = XZ-compressed ( load via SifExecDecompModuleBuffer ), 0 = raw */

} s_LoadParams[ 6 ] __attribute__(   (  section( ".data" )  )   ) = {
 { s_pAudSrv,  &g_DataBuffer[ SMS_AUDSRV_OFFSET   ], SMS_AUDSRV_SIZE,   0,                    NULL,      0 },
 { s_pPS2Dev9, &g_DataBuffer[ SMS_PS2DEV9_OFFSET  ], SMS_PS2DEV9_SIZE,  0,                    NULL,      0 },
 { s_pPS2POff, &g_DataBuffer[ SMS_POWEROFF_OFFSET ], SMS_POWEROFF_SIZE, 0,                    NULL,      0 },
 { s_pPS2ATAD, &g_DataBuffer[ SMS_PS2ATAD_OFFSET  ], SMS_PS2ATAD_SIZE,  0,                    NULL,      1 },
 { s_pPS2HDD,  &g_DataBuffer[ SMS_PS2HDD_OFFSET   ], SMS_PS2HDD_SIZE,   sizeof ( s_HDDArgs ), s_HDDArgs, 1 },
 { s_pPS2FS,   &g_DataBuffer[ SMS_PS2FS_OFFSET    ], SMS_PS2FS_SIZE,    sizeof ( s_PFSArgs ), s_PFSArgs, 1 }
};

static SifRpcClientData_t s_SMSUClt __attribute__ (   (  aligned( 64 ), section( ".bss" )  )   );

static void ( *s_SMS_SIFCmdHandler[ 6 ] ) ( void* ) __attribute__(   (  section( ".data" )  )   );

void SMS_IOPSetSifCmdHandler (  void ( *apFunc ) ( void* ), int aCmd  ) {

 s_SMS_SIFCmdHandler[ aCmd ] = apFunc;

}  /* end SMS_SetSifCmdHandler */

static void _sif_cmd_handler ( void* apPkt, void* apArg ) {

 s_SMS_SIFCmdHandler[ (  ( SifCmdHeader_t* )apPkt  ) -> opt ] ( apPkt );

}  /* end _sif_cmd_handler */

int SifExecDecompModuleBuffer ( void*, u32, u32, const char*, int* );   /* defined below; forward-declared for _load_module's compressed-module dispatch */

static void _load_module ( int anIndex, int afStatus ) {

 int lRes, lModRes;

 if ( afStatus ) {

  char lBuff[ 128 ]; sprintf ( lBuff, STR_LOADING.m_pStr, s_LoadParams[ anIndex ].m_pName );

  GUI_Status ( lBuff );

 }  /* end if */

 lRes = s_LoadParams[ anIndex ].m_fComp
  ? SifExecDecompModuleBuffer (
     s_LoadParams[ anIndex ].m_pBuffer, s_LoadParams[ anIndex ].m_BufSize,
     s_LoadParams[ anIndex ].m_nArgs,   s_LoadParams[ anIndex ].m_pArgs, &lModRes
    )
  : SifExecModuleBuffer (
     s_LoadParams[ anIndex ].m_pBuffer, s_LoadParams[ anIndex ].m_BufSize,
     s_LoadParams[ anIndex ].m_nArgs,   s_LoadParams[ anIndex ].m_pArgs, &lModRes
    );

 if ( anIndex == 1 && lRes >= 0 ) g_IOPFlags |= SMS_IOPF_DEV9_IS;

}  /* end _load_module */

#include "SMS_GS.h"
#include <slib.h>
#include <sbv_patches.h>
//#include "s_iop_image.h"

extern slib_exp_lib_list_t _slib_cur_exp_lib_list __attribute__(   (  section( ".data" )  )   );

int SifExecDecompModuleBuffer(void *ptr, u32 size, u32 arg_len, const char *args, int *mod_res)
{
	unsigned char *irx_data;
	int irx_size, ret = -1;

	if( mod_res ) *mod_res = -1;   /* so a short-circuited/failed load can't leave a stale positive for a caller that gates on it ( e.g. smbman ) */

	if((irx_size = lzma2_get_uncompressed_size((unsigned char *)ptr, size)) > 0)
	{
		irx_data = (unsigned char *)memalign(64, irx_size);
		if( !irx_data ) return -1;   /* OOM -> don't lzma2_uncompress into NULL */
		ret = lzma2_uncompress((unsigned char *)ptr, size, irx_data, irx_size);
		
		if(ret > 0)
			ret = SifExecModuleBuffer( irx_data, irx_size, arg_len, args, mod_res );
		
		free(irx_data);	
	}

	return ret;
}

void SMS_IOPReset ( int afExit ) {

 int i;
#if NO_DEBUG
 SifInitRpc ( 0 );
 SifExitIopHeap ();
 SifLoadFileExit(); 
 SifExitRpc     (); 

 while(!SifIopReset(s_pUDNL, 0)){};

 FlushCache(0);
 FlushCache(2);

 while (!SifIopSync()) {;}

 SifInitRpc ( 0 );

 _slib_cur_exp_lib_list.tail = NULL;
 _slib_cur_exp_lib_list.head = NULL;
 sbv_patch_enable_lmb           ();
 sbv_patch_disable_prefix_check ();
 sbv_patch_fileio               ();

 //while(!SifIopReset(s_iop_image, 0)){};

 FlushCache(0);
 FlushCache(2);

 while (!SifIopSync()) {;}

 SifInitRpc ( 0 );
 _slib_cur_exp_lib_list.tail = NULL;
 _slib_cur_exp_lib_list.head = NULL;
 sbv_patch_enable_lmb           ();
 sbv_patch_disable_prefix_check ();

 RCX_Load  ();
 RCX_Start ();
 RCX_Open  ();

#if 0
 while ( 1 ) {
  unsigned int lColor = RC_Read ();
  GS_VSync ();
  GS_BGCOLOR() = lColor + 0x00000030;
 }
#endif

#else
 afExit = 1;
#endif  /* NO_DEBUG */
 sbv_patch_disable_prefix_check ();
 sbv_patch_enable_lmb           ();

 SifExecModuleBuffer ( &g_DataBuffer[ SMS_SMSUTILS_OFFSET ], SMS_SMSUTILS_SIZE, 0, NULL, &i );

#ifdef BDM
 SifExecDecompModuleBuffer ( &iomanx_irx, size_iomanx_irx, 0, NULL, &i );
 SifExecDecompModuleBuffer ( &filexio_irx, size_filexio_irx, 0, NULL, &i );
 fileXioInit();

 SifExecDecompModuleBuffer ( &bdm_irx, size_bdm_irx, 0, NULL, &i );
 SifExecDecompModuleBuffer ( &bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL, &i );

 if ( !afExit ) SifExecDecompModuleBuffer ( &sio2man_irx, size_sio2man_irx, 0, NULL, &i );

 SifExecDecompModuleBuffer ( &mcman_irx, size_mcman_irx, 0, NULL, &i );
 SifExecDecompModuleBuffer ( &mcserv_irx, size_mcserv_irx, 0, NULL, &i );
 SifExecDecompModuleBuffer ( &padman_irx, size_padman_irx, 0, NULL, &i );
#else
 static const char* lpModules[ 4 ] = { s_pSIO2MAN, s_pPADMAN, s_pMCMAN, s_pMCSERV };

 if ( !afExit ) SifExecModuleBuffer ( &g_DataBuffer[ SMS_SIO2MAN_OFFSET ], SMS_SIO2MAN_SIZE,  0, NULL, &i );

 for ( i = 1 - afExit; i < 4; ++i ) SifLoadModule ( lpModules[ i ], 0, NULL );
#endif

 SIF_BindRPC ( &s_SMSUClt, SMSUTILS_RPC_ID );

 DisableIntc(INTC_TIM0);
 DisableIntc(INTC_TIM1);

}  /* end SMS_IOPReset */

int SMS_IOPStartNet ( int afStatus ) {

 int  i, j;
 char lSMAPArgs[ 80 ];
 int  lSMAPALen;

 if (  !( g_IOPFlags & SMS_IOPF_DEV9_IS )  ) return 0;
 if (  !( g_IOPFlags & SMS_IOPF_DEV9    )  ) {
  SMS_IOCtl ( g_pDEV9X, DEV9CTLINIT, NULL );
  g_IOPFlags |= SMS_IOPF_DEV9;
 }  /* end if */

 if ( afStatus ) GUI_Status ( STR_INITIALIZING_NETWORK.m_pStr );

 memset (  lSMAPArgs, 0, sizeof ( lSMAPArgs )  );
 strncpy ( lSMAPArgs, g_pDefIP, 15 );
 i = strlen ( g_pDefIP ) + 1;
 strncpy ( lSMAPArgs + i, g_pDefMask, 15 );
 i += strlen ( g_pDefMask ) + 1;
 strncpy ( lSMAPArgs + i, g_pDefGW, 15 );
 i += strlen ( g_pDefGW ) + 1;
 lSMAPALen = i;

 j = ( g_Config.m_NetworkFlags >> 8 ) & 3;

 if ( j == 1 )

  j = 0x05E0;

 else if ( j == 2 ) {

  j = 0x0400;

  if ( g_Config.m_NetworkFlags & SMS_DF_HALF ) {

   if ( g_Config.m_NetworkFlags & SMS_DF_10 )
    j |= 0x0020;
   else j |= 0x080;

  } else {

   if ( g_Config.m_NetworkFlags & SMS_DF_10 )
    j |= 0x040;
   else j |= 0x0100;

  }  /* end else */

 } else j = 0;

 sprintf (  &lSMAPArgs[ i ], "%d", j  );
 lSMAPALen += strlen ( &lSMAPArgs[ i ] ) + 1;

 SifExecDecompModuleBuffer ( &g_DataBuffer[ SMS_PS2IP_OFFSET ], SMS_PS2IP_SIZE, 0, NULL, &i );

 if ( i >= 0 ) {

  SifExecDecompModuleBuffer ( &g_DataBuffer[ SMS_PS2SMAP_OFFSET ], SMS_PS2SMAP_SIZE, lSMAPALen, &lSMAPArgs[ 0 ], &i );

  if ( i >= 0 ) {

   if ( g_Config.m_NetworkFlags & SMS_DF_SMB ) {

    /* Modern smbman: an iomanX device "smb" reached via fileXioDevctl. iomanx
     * and filexio are already loaded in SMS_IOPReset, and smbman binds to the
     * untouched SMSTCPIP ( SMS_PS2IP ) stack loaded just above. The smbman_irx
     * blob + its size are only compiled/embedded in BDM builds ( see Makefile ),
     * so guard the reference -- SMS_IOPStartNet itself is built in all configs. */
#ifdef BDM
    SifExecDecompModuleBuffer ( &smbman_irx, size_smbman_irx, 0, NULL, &i );

    if ( i >= 0 ) g_IOPFlags |= SMS_IOPF_SMB;
#else
    i = -1;  /* SMB unavailable in non-BDM builds */
#endif

   } else {

    SifExecDecompModuleBuffer ( &g_DataBuffer[ SMS_PS2HOST_OFFSET ], SMS_PS2HOST_SIZE, 0, NULL, &i );

    if ( i >= 0 ) g_IOPFlags |= SMS_IOPF_NET;

   }  /* end else */

  }  /* end if */

 }  /* end if */

 return g_IOPFlags & ( SMS_IOPF_NET | SMS_IOPF_SMB );

}  /* end SMS_IOPStartNet */

#ifdef BDM
static int checkConnectedMassDev ( int afUnit ) {

    int fd;
    char path[32];

    snprintf(path, sizeof(path), "mass%d:", afUnit);   /* literal base: immune to g_pUSB being mutated to massN by the browser */
    fd = fioDopen(path);
    if (fd >= 0) {
        fioDclose(fd);
        return 1;
    }

    return 0;
}
#endif

int SMS_IOPStartUSB ( int afStatus ) {

 int  i;

#ifdef BDM
 int ret;

 if ( g_IOPFlags & SMS_IOPF_USB ) return g_IOPFlags & SMS_IOPF_USB;   /* idempotent: already mounted -> don't re-load usbd */

 SifExecDecompModuleBuffer ( &usbd_irx, size_usbd_irx, 0, NULL, &i );
 g_IOPFlags |= SMS_IOPF_USB;

 SifExecDecompModuleBuffer ( &usbmass_bd_irx, size_usbmass_bd_irx, 0, NULL, &i );
 g_IOPFlags |= SMS_IOPF_UMS;

 // give the modules a few seconds to load
 for ( i = 0; i < 5; i++ ) {
  ret = 0x01000000;
  while ( ret-- ) asm ( "nop\nnop\nnop\nnop" );
 }

 // check for connected devices.. hot plugging isn't going to work
 if (  checkConnectedMassDev ( 0 )  ) { g_MassFlags |= 0x00000002; s_MassAdded |= ( 1 << 0 ); }
 if (  checkConnectedMassDev ( 1 )  ) { g_MassFlags |= 0x00000800; s_MassAdded |= ( 1 << 1 ); }
 if (  checkConnectedMassDev ( 2 )  ) { g_MassFlags |= 0x00002000; s_MassAdded |= ( 1 << 2 ); }
 if (  checkConnectedMassDev ( 3 )  ) { g_MassFlags |= 0x00008000; s_MassAdded |= ( 1 << 3 ); }
#else
 char lBuf[ 64 ];

 sprintf ( lBuf, g_pFmt3, g_pMC0SMS, g_SlashStr, s_pUSBD );

 if ( afStatus ) GUI_Status ( STR_LOCATING_USBD.m_pStr );

 i = fioOpen ( lBuf, O_RDONLY );

 if ( i >= 0 ) {
  fioClose ( i );
  i = SifLoadModule ( lBuf, 0, NULL );
 }  /* end if */

 if ( i < 0 ) SifExecDecompModuleBuffer ( &g_DataBuffer[ SMS_USBD_OFFSET ], SMS_USBD_SIZE, 0, NULL, &i );

 g_IOPFlags |= SMS_IOPF_USB;

 lBuf[ strlen ( lBuf ) - 5 ] = 'M';

 i = fioOpen ( lBuf, O_RDONLY );

 if ( i >= 0 ) {
  fioClose ( i );
  i = SifLoadModule ( lBuf, 0, NULL );
 }  /* end if */

 if ( i < 0 ) {
  SifExecDecompModuleBuffer ( &g_DataBuffer[ SMS_USB_MASS_OFFSET ], SMS_USB_MASS_SIZE, 0, NULL, &i );
  g_IOPFlags |= SMS_IOPF_UMS;
  *( int* )g_pUSB = 0x20736D75;
 }  /* end if */

#endif
 return g_IOPFlags & SMS_IOPF_USB;

}  /* end SMS_IOPStartUSB */

#ifdef BDM
int SMS_IOPStartMX4SIO ( int afStatus ) {

 static const unsigned int lBit[ 4 ] = { 0x00000002, 0x00000800, 0x00002000, 0x00008000 };

 int i, ret, before = 0;

 if ( g_IOPFlags & SMS_IOPF_MMCE   ) return 0;                             /* shares the SIO2 port with MMCE ( reciprocal of the guard in SMS_IOPStartMMCE ) */
 if ( g_IOPFlags & SMS_IOPF_MX4SIO ) return g_IOPFlags & SMS_IOPF_MX4SIO;  /* idempotent: don't load mx4sio_bd twice on an autostart boot */

 for ( i = 0; i < 4; ++i ) if (  checkConnectedMassDev ( i )  ) before |= ( 1 << i );

 SifExecDecompModuleBuffer ( &mx4sio_bd_irx, size_mx4sio_bd_irx, 0, NULL, &i );

 // give the module a few seconds to load
 for ( i = 0; i < 5; ++i ) {
  ret = 0x01000000;
  while ( ret-- ) asm ( "nop\nnop\nnop\nnop" );
 }  /* end for */

 g_IOPFlags |= SMS_IOPF_MX4SIO;

 for ( i = 0; i < 4; ++i ) {

  if (  checkConnectedMassDev ( i )  ) {

   if (  !( s_MassAdded & ( 1 << i ) )  ) {   /* don't re-add a unit already in the strip */
    g_MassFlags |= lBit[ i ];
    s_MassAdded |= ( 1 << i );
   }  /* end if */

   if (  !( before & ( 1 << i ) )  ) g_Mx4sioMask |= ( 1 << i );

  }  /* end if */

 }  /* end for */

 return g_IOPFlags & SMS_IOPF_MX4SIO;

}  /* end SMS_IOPStartMX4SIO */
#else
int SMS_IOPStartMX4SIO ( int afStatus ) { return 0; }
#endif

#ifdef BDM
/* Snapshot which mass units ( 0..3 ) are currently present. Used to diff before
 * vs after loading a block driver so we can attribute the newly-appeared units to
 * that driver's device type. */
static unsigned int _bdm_scan ( void ) {
 int i; unsigned int lMask = 0;
 for ( i = 0; i < 4; ++i ) if (  checkConnectedMassDev ( i )  ) lMask |= ( 1 << i );
 return lMask;
}

/* After a BDM block driver loads, register the units that appeared: emit each new
 * unit's one-shot connect event ( once, guarded by s_MassAdded so the device strip
 * never duplicates ) and tag its type in *apTypeMask so _dev_icon_index picks the
 * right icon. aBefore = the mass-unit bitmap captured BEFORE the driver loaded. */
static void _bdm_register_new ( unsigned int aBefore, unsigned int* apTypeMask ) {
 static const unsigned int lBit[ 4 ] = { 0x00000002, 0x00000800, 0x00002000, 0x00008000 };
 int i;
 for ( i = 0; i < 4; ++i ) {
  if (  checkConnectedMassDev ( i )  ) {
   if (  !( s_MassAdded & ( 1 << i ) )  ) {
    g_MassFlags |= lBit[ i ];
    s_MassAdded |= ( 1 << i );
   }  /* end if */
   if (  !( aBefore & ( 1 << i ) )  ) *apTypeMask |= ( 1 << i );
  }  /* end if */
 }  /* end for */
}

int SMS_IOPStartILINK ( int afStatus ) {

 unsigned int lBefore = _bdm_scan ();
 int          i, ret;

 /* i.LINK is self-contained -- it drives its own IEEE1394 hardware, needs no dev9
  * or usbd. Load the bus manager first, then the SBP-2 block driver ( which binds
  * to bdm and registers massN: ). */
 SifExecDecompModuleBuffer ( &iLinkman_irx,    size_iLinkman_irx,    0, NULL, &i );
 SifExecDecompModuleBuffer ( &IEEE1394_bd_irx, size_IEEE1394_bd_irx, 0, NULL, &i );

 /* SBP-2 discovery + login after the bus reset can take up to ~5s ( spec ), so
  * wait noticeably longer than the SD-based drivers before probing. */
 for ( i = 0; i < 12; ++i ) {
  ret = 0x01000000;
  while ( ret-- ) asm ( "nop\nnop\nnop\nnop" );
 }  /* end for */

 g_IOPFlags |= SMS_IOPF_ILINK;

 _bdm_register_new ( lBefore, &g_IlinkMask );

 return g_IOPFlags & SMS_IOPF_ILINK;

}  /* end SMS_IOPStartILINK */

int SMS_IOPStartATA ( int afStatus ) {

 unsigned int lBefore;
 int          i, ret;
 static char  lP1[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "ATA: preparing bus (DEV9)...";
 static char  lP2[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "ATA: loading driver (ata_bd)...";
 static char  lP3[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "ATA: scanning drive (mounting exFAT)...";

 /* Internal HDD as a BDM ( massN: / exFAT ) device via ata_bd, which bundles atad
  * and probes the ATA bus itself. Mutually exclusive with SMS's PFS HDD ( same atad
  * / same physical drive ) -- whichever starts first owns the bus. Requires DEV9.
  * Progress is shown at each stage: if it stalls, the LAST on-screen message pins
  * the exact stage ( ata_bd can hang on some drive layouts, e.g. an APAJail'd HDD ). */
 if ( s_AtaBusOwner == 1 ) return 0;                        /* PFS HDD owns the bus */
 if (  !( g_IOPFlags & SMS_IOPF_DEV9_IS )  ) return 0;      /* no DEV9 hardware      */

 GUI_Status ( lP1 );
 if (  !( g_IOPFlags & SMS_IOPF_DEV9 )  ) {                 /* loaded but shut down  */
  SMS_IOCtl ( g_pDEV9X, DEV9CTLINIT, NULL );
  g_IOPFlags |= SMS_IOPF_DEV9;
 }  /* end if */

 lBefore = _bdm_scan ();

 GUI_Status ( lP2 );
 SifExecDecompModuleBuffer ( &ata_bd_irx, size_ata_bd_irx, 0, NULL, &i );
 s_AtaBusOwner = 2;

 /* give the drive time to spin up + atad to detect it */
 for ( i = 0; i < 6; ++i ) {
  ret = 0x01000000;
  while ( ret-- ) asm ( "nop\nnop\nnop\nnop" );
 }  /* end for */

 GUI_Status ( lP3 );
 g_IOPFlags |= SMS_IOPF_ATA;

 _bdm_register_new ( lBefore, &g_AtaMask );

 return g_IOPFlags & SMS_IOPF_ATA;

}  /* end SMS_IOPStartATA */

int SMS_IOPStartMMCE ( int afStatus ) {

 static char lMmce[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "mmceX:/";
 int         i, lTry;

 /* MMCE ( SD2PSX / MemCard PRO ) exposes its microSD over the memory-card / SIO2
  * port via mmceman as browsable mmce0: / mmce1: devices -- NOT BDM mass:, so it
  * gets its own device id ( 7 ) and detection path. That SIO2 port is the SAME one
  * MX4SIO drives, so the two cannot coexist ( NHDDL refuses to load mmceman while
  * MX4SIO is active ); MX4SIO is the primary target, so defer to it. Only mmceman is
  * needed ( NHDDL loads it alone -- mmcedrv is not required ). Probe each slot and
  * raise a connect event for any newly-present card. */
 if ( g_IOPFlags & SMS_IOPF_MMCE ) return g_IOPFlags & SMS_IOPF_MMCE;   /* idempotent: already up ( e.g. resolver started it ) -> don't reload mmceman / re-acquire the pad */
 if ( g_IOPFlags & SMS_IOPF_MX4SIO ) return 0;   /* shares the SIO2 port with MX4SIO */

 SifExecDecompModuleBuffer ( &mmceman_irx, size_mmceman_irx, 0, NULL, &i );

 g_IOPFlags |= SMS_IOPF_MMCE;

 /* Detection ( NHDDL parity, devices_mmce.c:26 ): the PING ( devctl 0x1 ) proves
  * ONLY that the SD2PSX / MemCard PRO controller answered on this SIO2 port -- NOT
  * that a microSD is inserted or its FAT mounted. mmceman has NO mount op; the FAT
  * is mounted lazily inside mmce_fs_dopen and can still be settling right after the
  * load-time RESET. So gate "present / browsable" on a REAL dir-open: ping first
  * ( fast-skips a port with no controller ), then retry fileXioDopen( "mmceN:/" --
  * the exact string the browser opens, SMS_FileDir.c:248 ) a few times, and list
  * the slot only once it actually opens. This stops a controller-present-but-no-
  * card / not-yet-mounted slot from showing up as an empty, unbrowsable device
  * ( the earlier ping-only detection did exactly that ). Browsing then works via
  * the FULL_IOMAN legacy-hook in iomanx.irx that bridges fio* to the iomanX mmce
  * device, same as mass:. */
 for ( i = 0; i < 2; ++i ) {

  lMmce[ 4 ] = i + '0';

  if (  fileXioDevctl ( lMmce, 0x1 /* MMCE_CMD_PING */, NULL, 0, NULL, 0 ) == -1  ) continue;   /* no controller on this port */

  for ( lTry = 0; lTry < 6; ++lTry ) {

   int lFD = fileXioDopen ( lMmce );   /* mounts the FAT lazily; < 0 while still settling / no card */

   if ( lFD >= 0 ) {

    fileXioDclose ( lFD );

    if (  !( s_MmceAdded & ( 1 << i ) )  ) {
     g_MmceFlags |= ( 1 << i );
     s_MmceAdded |= ( 1 << i );
    }  /* end if */

    break;

   }  /* end if */

   { int lSpin = 0x00800000; while ( lSpin-- ) asm ( "nop\nnop\nnop\nnop" ); }   /* let the FAT settle, then retry */

  }  /* end for */

 }  /* end for */

 /* mmceman's sio2man hook + the SIO2 PING/RESET traffic above desyncs padman's
  * already-open controller port ( the pad reads dead after Start MMCE ). Re-open
  * both pad ports now -- while the GUI pad poller is suspended and BEFORE
  * _start_device drains the still-held confirm, so the drain then acts on a live
  * pad. Also covers the AUTO_MMCE-at-boot path ( SMS_IOPInit ), which runs after
  * GUI_Initialize has opened the pad but before GUI_Run starts polling. */
 PadReacquire ();

 return g_IOPFlags & SMS_IOPF_MMCE;

}  /* end SMS_IOPStartMMCE */
#else
int SMS_IOPStartILINK ( int afStatus ) { return 0; }
int SMS_IOPStartATA   ( int afStatus ) { return 0; }
int SMS_IOPStartMMCE  ( int afStatus ) { return 0; }
#endif

#ifdef BDM
void SMS_IOPRefreshMass ( void ) {

 static const unsigned int lBit[ 4 ] = { 0x00000002, 0x00000800, 0x00002000, 0x00008000 };

 int i;

 /* Re-probe the BDM mass slots ( USB / MX4SIO ) so a newly connected drive is
  * picked up without a reboot. g_MassFlags is a one-shot "device connected"
  * event flag: the GUI loop consumes each set bit, adds the device to the strip
  * and clears the bit -- with NO duplicate check. So we must only raise a
  * connect event for a unit that is NOT already in the strip; otherwise every
  * refresh would append another copy of the same drive ( the reported bug ).
  * Units that were present and are now gone are left in place -- SMS has no safe
  * hot-remove path here ( a disconnect bit set in g_MassFlags would re-fire every
  * GUI loop, since the disconnect handler never clears it from g_MassFlags ). */
 for ( i = 0; i < 4; ++i ) {

  if (  checkConnectedMassDev ( i ) && !( s_MassAdded & ( 1 << i ) )  ) {

   g_MassFlags |= lBit[ i ];
   s_MassAdded |= ( 1 << i );

   /* If MX4SIO is the active BDM backend, tag the freshly detected unit so it
    * shows the MX4SIO icon rather than the default USB one. */
   if ( g_IOPFlags & SMS_IOPF_MX4SIO ) g_Mx4sioMask |= ( 1 << i );

  }  /* end if */

 }  /* end for */

}  /* end SMS_IOPRefreshMass */
#else
void SMS_IOPRefreshMass ( void ) {}
#endif

int SMS_IOPStartHDD ( int afStatus ) {

 int i;

#ifdef BDM
 if ( s_AtaBusOwner == 2 ) return 0;   /* ATA-as-BDM ( ata_bd ) already owns the ATA bus */
#endif

 if (  !( g_IOPFlags & SMS_IOPF_DEV9_IS )  ) return 0;
 if (  !( g_IOPFlags & SMS_IOPF_DEV9    )  ) {
  SMS_IOCtl ( g_pDEV9X, DEV9CTLINIT, NULL );
  g_IOPFlags |= SMS_IOPF_DEV9;
 }  /* end if */

 for ( i = 3; i < 6; ++i ) _load_module ( i, afStatus );
#ifdef BDM
 s_AtaBusOwner = 1;   /* PFS HDD now owns the ATA bus ( ps2atad loaded ) */
#endif

 i = SMS_IOCtl ( g_pHDD, PS2HDD_IOCTL_STATUS, NULL );

 if ( i == 0 || i == 1 ) g_IOPFlags |= SMS_IOPF_HDD;

 return g_IOPFlags & SMS_IOPF_HDD;

}  /* end SMS_IOPStartHDD */

void SMS_IOPSetXLT ( void ) {

 /* smbman has no codepage ioctl ( the legacy SMSSMB SMB_IOCTL_SETCP is gone ).
  * Server share/file names come back UTF-8 / OEM as the server sends them. */

}  /* end SMS_IOPSetXLT */

extern void _exit_handler ( void*, int );

static int  s_PwrOffThreadID;
static char s_PwrOffThreadStack[ 2048 ] __attribute__(   ( aligned( 16 )  )   );

static void _poweroff_thread ( void* apData ) {

 int lFD;

 while ( 1 ) {

  SleepThread ();

  ChangeThreadPriority (  GetThreadId (), 99  );

  if ( g_Config.m_BrowserFlags & SMS_BF_EXIT ) _exit_handler ( NULL, 1 );

  SMS_IOCtl ( g_pPFS, PFS_IOCTL_CLOSE_ALL,      NULL );
  SMS_IOCtl ( g_pHDD, PS2HDD_IOCTL_FLUSH_CACHE, NULL );

  lFD = fioDopen ( g_pDEV9X );

  if ( lFD >= 0 ) fioIoctl ( lFD, DEV9CTLSHUTDOWN, NULL );

  DIntr ();
   ee_kmode_enter ();
    *(  ( unsigned char* )0xBF402017 ) = 0x00;
    *(  ( unsigned char* )0xBF402016 ) = 0x0F;
   ee_kmode_exit ();
  EIntr ();

 }  /* end while */

}  /* end _poweroff_thread */

static void _poweroff_handler ( void* apHdr ) {
 iWakeupThread ( s_PwrOffThreadID );
}  /* end _poff_intr_callback */

void SMS_IOPowerOff ( void ) {
 WakeupThread ( s_PwrOffThreadID );
}  /* end SMS_IOPowerOff */

static SifCmdHandlerData_t handlerdata[32];

#ifdef BDM
/* Non-re-mountable boot ( SMB / host / cdrom, flagged by SMS_ConfigFallback ):
 * config CANNOT live next to the ELF. A memory card is the natural home here
 * ( "nowhere else to go" ), so if one is present KEEP the mc0:/SMS default and do
 * nothing. ONLY when there is no card do we fall back to the first attached,
 * writable FS device and keep SMS.cfg at its ROOT ( "<dev>N:/SMS.cfg" -- root needs
 * no mkdir ), re-found next boot by scanning the SAME fixed order ( mass0..3, then
 * mmce0..1 ) so a USB unit-renumber is tolerated. USB is tried first ( idempotent
 * mount, no pad / DEV9 ); MMCE only if MX4SIO isn't the SIO2 owner ( never the case
 * on an SMB boot ). All probes are READ-ONLY, so we never drop a stray file. If
 * nothing writable is attached either, leave s_CfgOnFS 0 -> mc0: ( save will error,
 * but there is genuinely nowhere ). Called ONLY from SMS_IOPInit ( post-GUI ), so a
 * mount stall is visible, never a black boot. */
static void _cfg_resolve_fallback ( void ) {

 int  n, lUsb = -1, lMmce = -1, lFD;
 char lPath[ 24 ];

 if (  SMS_MCPresent ()  ) return;   /* memory card present -> config stays on mc0:/SMS ( s_CfgOnFS 0 ) */

 SMS_IOPStartUSB ( 1 );                                   /* idempotent ( guard at top of SMS_IOPStartUSB ) */

 for ( n = 0; n < 4; ++n ) {

  if (  !checkConnectedMassDev ( n )  ) continue;

  if ( lUsb < 0 ) lUsb = n;                               /* remember the first present USB unit ( save target ) */

  sprintf ( lPath, "mass%d:/SMS.cfg", n );
  lFD = fioOpen ( lPath, O_RDONLY );

  if ( lFD >= 0 ) { fioClose ( lFD ); SMS_ConfigUseFSPath ( lPath ); return; }   /* existing cfg on USB */

 }  /* end for */

 if (  !( g_IOPFlags & SMS_IOPF_MX4SIO )  ) {             /* SIO2 free -> MMCE may be started */

  SMS_IOPStartMMCE ( 1 );

  for ( n = 0; n < 2; ++n ) {

   sprintf ( lPath, "mmce%d:/", n );
   lFD = fioDopen ( lPath );
   if ( lFD < 0 ) continue;                               /* device not present */
   fioDclose ( lFD );

   if ( lMmce < 0 ) lMmce = n;                            /* remember the first present MMCE unit */

   sprintf ( lPath, "mmce%d:/SMS.cfg", n );
   lFD = fioOpen ( lPath, O_RDONLY );

   if ( lFD >= 0 ) { fioClose ( lFD ); SMS_ConfigUseFSPath ( lPath ); return; }   /* existing cfg on MMCE */

  }  /* end for */

 }  /* end if */

/* No existing SMS.cfg anywhere -> pick a save target ( USB preferred ). The file
 * is created on the user's first Save. */
 if ( lUsb  >= 0 ) { sprintf ( lPath, "mass%d:/SMS.cfg", lUsb  ); SMS_ConfigUseFSPath ( lPath ); return; }
 if ( lMmce >= 0 ) { sprintf ( lPath, "mmce%d:/SMS.cfg", lMmce ); SMS_ConfigUseFSPath ( lPath ); return; }

/* Nothing attached -> mc0: last resort ( s_CfgOnFS stays 0 ). */

}  /* end _cfg_resolve_fallback */
#endif  /* BDM */

void SMS_IOPInit ( void ) {

 int         i, lFD;
 char        lBuff[ 64 ];
 ee_thread_t lThreadParam;

#ifdef BDM
/* Booted from a filesystem device? SMS.cfg lives on THAT device ( the argv[0]
 * boot drive ), but SMS_LoadConfig already ran inside GUI_Initialize BEFORE any
 * device was mounted, so it silently fell back to defaults ( "settings not loaded
 * on boot" ). Mount the boot device HERE -- keyed off the config path's device
 * prefix, i.e. lazy-load via arg0 -- then re-load the config BEFORE the auto-start
 * section reads m_NetworkFlags, and re-apply the palette. Done post-GUI ( display
 * already up ) so a mount hang is a visible stall, NOT a black boot ( mounting
 * pre-GUI black-screened ). Display MODE still needs a reboot to switch ( GS
 * already inited ), but colours / auto-start / browser+player settings persist. */
 if ( SMS_ConfigOnFS () ) {

  const char* lpCfg = SMS_ConfigPath ();   /* e.g. "mmce0:/APPS/SMS.cfg" */

  if ( strncmp ( lpCfg, "mmce", 4 ) == 0 ) {

   SMS_IOPStartMMCE ( 1 );   /* booted from SD2PSX / MemCard PRO */

  } else if ( strncmp ( lpCfg, "mass", 4 ) == 0 ) {

   int lCfgFD;

   SMS_IOPStartUSB ( 1 );                       /* mass could be USB ... */

   lCfgFD = fioOpen ( lpCfg, O_RDONLY );        /* is the cfg actually on USB? */
   if ( lCfgFD >= 0 ) fioClose ( lCfgFD );
   else SMS_IOPStartMX4SIO ( 1 );               /* ... or MX4SIO ( also mass ) */

  } else if ( strncmp ( lpCfg, "pfs", 3 ) == 0 || strncmp ( lpCfg, "hdd", 3 ) == 0 ) {

   SMS_IOPStartHDD ( 1 );                        /* internal HDD ( PFS ) */

  }  /* end else if */

  SMS_LoadConfig ();
  GUI_SetColors  ();
  SMS_LocaleInit ();   /* re-read SMS.lng now the boot device is mounted ( CWD-first ) */
  SMS_LocaleSet  ();

 } else if ( SMS_ConfigFallback () ) {   /* SMB / host / cdrom boot: config can't live on the boot device */

/* Resolve an attached, writable USB / MMCE device to hold SMS.cfg ( pins the path
 * + flips to the FS save/load branch ), then load it. mc0: only if nothing writable
 * is attached. Same post-GUI slot as the re-mountable branch, before AUTO_* reads
 * m_NetworkFlags. */
  _cfg_resolve_fallback ();
  SMS_LoadConfig ();
  GUI_SetColors  ();

 }  /* end else if */
#endif

 SifLoadModule ( s_pLIBSD, 0, NULL );

 SMS_IOPDVDVInit ();

 lFD = fioOpen ( g_pIPConf, O_RDONLY );

 if ( lFD >= 0 ) {

  memset (  lBuff, 0, sizeof ( lBuff )  );
  i = fioRead (  lFD, lBuff, sizeof ( lBuff ) - 1  );
  fioClose ( lFD );

  if ( i > 0 ) {

   char lChr;

   lBuff[ i ] = '\x00';

   for (  i = 0; (  ( lChr = lBuff[ i ] ) != '\0'  ); ++i  )

    if (  lChr == ' ' || lChr == '\r' || lChr == '\n' ) lBuff[ i ] = '\x00';

   strncpy ( g_pDefIP, lBuff, 15 );
   i = strlen ( g_pDefIP ) + 1;
   strncpy ( g_pDefMask, lBuff + i, 15 );
   i += strlen ( g_pDefMask ) + 1;
   strncpy ( g_pDefGW, lBuff + i, 15 );

  }  /* end if */

 }  /* end if */

 for ( i = 0; i < 3; ++i ) _load_module ( i, 1 );

 if ( g_IOPFlags & SMS_IOPF_DEV9_IS ) {
#if NO_DEBUG
  if (   !(  g_Config.m_NetworkFlags & ( SMS_DF_AUTO_HDD | SMS_DF_AUTO_NET | SMS_DF_AUTO_ATA )  )   )
   SMS_IOCtl ( g_pDEV9X, DEV9CTLSHUTDOWN, NULL );
  else g_IOPFlags |= SMS_IOPF_DEV9;
#else
  g_IOPFlags |= SMS_IOPF_DEV9;
#endif  /* NO_DEBUG */
 }  /* end if */

 SPU_Initialize ();

 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_HDD ) SMS_IOPStartHDD ( 1 );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_NET ) SMS_IOPStartNet ( 1 );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_USB ) SMS_IOPStartUSB ( 1 );
#ifdef BDM
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_MX4SIO ) SMS_IOPStartMX4SIO ( 1 );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_ATA    ) SMS_IOPStartATA    ( 1 );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_ILINK  ) SMS_IOPStartILINK  ( 1 );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_MMCE   ) SMS_IOPStartMMCE   ( 1 );
#endif

 GUI_Status ( STR_INITIALIZING_SMS.m_pStr );

 SMS_IOPSetSifCmdHandler ( _poweroff_handler, SMS_SIF_CMD_POWEROFF );

 lThreadParam.initial_priority   = 48;
 lThreadParam.stack_size         = sizeof ( s_PwrOffThreadStack );
 lThreadParam.gp_reg             = &_gp;
 lThreadParam.func               = _poweroff_thread;
 lThreadParam.stack              = s_PwrOffThreadStack;
 StartThread (  s_PwrOffThreadID = CreateThread ( &lThreadParam ), NULL  );

 DI();
  SifSetCmdBuffer(&handlerdata[0], 32);
  SifAddCmdHandler ( 18, _sif_cmd_handler, NULL );
 EI();

 SPU_LoadData (  g_SMSounds, sizeof ( g_SMSounds )  );

 if (  RCX_Load () && RCX_Start ()  )
  g_IOPFlags |= SMS_IOPF_RMMAN2;
 else if (  RC_Load () && RC_Start ()  ) g_IOPFlags |= SMS_IOPF_RMMAN;

 FlushCache ( 0 );

}  /* end SMS_IOPInit */

int SMS_IOPQueryTotalFreeMemSize ( void ) {

 int retVal;

 SifCallRpc ( &s_SMSUClt, 0, 0, NULL, 0, &retVal, 4, 0, 0 );

 return retVal;

}  /* end SMS_IOPQueryTotalFreeMemSize */

int SMS_IOPDVDVInit ( void ) {

 int retVal;

 SifCallRpc ( &s_SMSUClt, 1, 0, NULL, 0, &retVal, 4, 0, 0 );

 return retVal;

}  /* end SMS_IOPDVDVInit */

int SMS_IOCtl ( const char* apDev, int aCmd, void* apData ) {

 int lFD = fioDopen ( apDev );

 if ( lFD >= 0 ) {

  int retVal = fioIoctl ( lFD, aCmd, apData );

  fioDclose ( lFD );

  return retVal;

 } else return lFD;

}  /* end SMS_IOCtl */

int SMS_HDDMount ( const char* aFSys, const char* aPath, int aMode ) {

 PS2HDDMountInfo lMountInfo;

 lMountInfo.m_Mode = aMode;
 strcpy ( lMountInfo.m_Path, aPath );

 return SMS_IOCtl ( aFSys, PFS_IOCTL_MOUNT, &lMountInfo );

}  /* end SMS_HDDMount */
