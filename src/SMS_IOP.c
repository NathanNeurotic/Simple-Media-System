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

extern char g_pBootDir[];   /* SMS_Config.c: "<dev>/path/" on a non-mc boot, empty on mc */
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

/* Which network personality owns the single EMAC3 NIC this boot: 0 = free, 1 = SMS
 * SMB/host ( PS2SMAP + SMSTCPIP ), 2 = udpfs ( udpfs_smap, its own SMAP ). Both register
 * the 'smap' library, so only the first to start may own it -- the other backs off ( no
 * runtime swap without a full IOP reset ). In common scope, NOT under #ifdef BDM: the
 * non-BDM SMS_IOPStartNet reads it, and the SMS_IOPNetOwnedBy* accessors expose it to
 * the menu. Without BDM nothing ever SETS it, so it stays 0 ( "free" ) -- truthful. */
static int s_NetOwner;

#ifdef BDM
unsigned int g_Mx4sioMask;
unsigned int g_AtaMask;
unsigned int g_IlinkMask;
unsigned int g_UdpfsFlags;   /* bit0 = TRANSIENT "raise the mount event" ( SMS_GUI.c clears it the moment it raises ); bit1 = STICKY "udpfs: is mounted", never cleared. Two bits because the guard/menu need persistent state while the event must be one-shot -- the MMCE split ( transient g_MmceFlags vs persistent SMS_IOPF_MMCE ) done in one word. NOT a mass bitmask: udpfs is an iomanX device, not BDM. */
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
extern unsigned char ds34usb_irx    [];
extern unsigned char ds34bt_irx     [];
extern unsigned char libsd_irx      [];
extern unsigned char udpfs_smap_irx [];
extern unsigned char udpfs_ministack_irx [];
extern unsigned char udpfs_ioman_irx [];

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
extern unsigned int size_ds34usb_irx;
extern unsigned int size_ds34bt_irx;
extern unsigned int size_libsd_irx;
extern unsigned int size_udpfs_smap_irx;
extern unsigned int size_udpfs_ministack_irx;
extern unsigned int size_udpfs_ioman_irx;

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
/* s_NetOwner is declared in common scope above ( SMS_IOPStartNet, non-BDM, reads it ). */
/* MMCE units ( bit0=mmce0:, bit1=mmce1: ) already announced to the device strip,
 * so a re-probe doesn't append a duplicate ( same idea as s_MassAdded ). */
static unsigned int s_MmceAdded;

/* UDPFS per-module residency. File scope ( not function-local statics ) so the in-place
 * network-stack switch can clear them: after an IOP reset those modules are GONE, and a
 * stale "already loaded" here would make SMS_IOPStartUDPFS skip the loads and then fail at
 * the probe with no way back. */
static int s_fSmap, s_fStack, s_fIoman;
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
static int  s_fLIBSD;   /* rom0:LIBSD loaded? 0 on a PSX/DESR, whose BIOS has no such file */

struct {

 const char* m_pName;
 void*       m_pBuffer;
 int         m_BufSize;
 int         m_nArgs;
 void*       m_pArgs;

} s_LoadParams[ 6 ] __attribute__(   (  section( ".data" )  )   ) = {
 { s_pAudSrv,  &g_DataBuffer[ SMS_AUDSRV_OFFSET   ], SMS_AUDSRV_SIZE,   0,                    NULL      },
 { s_pPS2Dev9, &g_DataBuffer[ SMS_PS2DEV9_OFFSET  ], SMS_PS2DEV9_SIZE,  0,                    NULL      },
 { s_pPS2POff, &g_DataBuffer[ SMS_POWEROFF_OFFSET ], SMS_POWEROFF_SIZE, 0,                    NULL      },
 { s_pPS2ATAD, &g_DataBuffer[ SMS_PS2ATAD_OFFSET  ], SMS_PS2ATAD_SIZE,  0,                    NULL      },
 { s_pPS2HDD,  &g_DataBuffer[ SMS_PS2HDD_OFFSET   ], SMS_PS2HDD_SIZE,   sizeof ( s_HDDArgs ), s_HDDArgs },
 { s_pPS2FS,   &g_DataBuffer[ SMS_PS2FS_OFFSET    ], SMS_PS2FS_SIZE,    sizeof ( s_PFSArgs ), s_PFSArgs }
};

static SifRpcClientData_t s_SMSUClt __attribute__ (   (  aligned( 64 ), section( ".bss" )  )   );

static void ( *s_SMS_SIFCmdHandler[ 6 ] ) ( void* ) __attribute__(   (  section( ".data" )  )   );

void SMS_IOPSetSifCmdHandler (  void ( *apFunc ) ( void* ), int aCmd  ) {

 s_SMS_SIFCmdHandler[ aCmd ] = apFunc;

}  /* end SMS_SetSifCmdHandler */

static void _sif_cmd_handler ( void* apPkt, void* apArg ) {

 s_SMS_SIFCmdHandler[ (  ( SifCmdHeader_t* )apPkt  ) -> opt ] ( apPkt );

}  /* end _sif_cmd_handler */

static void _load_module ( int anIndex, int afStatus ) {

 int lRes, lModRes;

 if ( afStatus ) {

  char lBuff[ 128 ]; sprintf ( lBuff, STR_LOADING.m_pStr, s_LoadParams[ anIndex ].m_pName );

  GUI_Status ( lBuff );

 }  /* end if */

 lRes = SifExecModuleBuffer (
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
	
	if((irx_size = lzma2_get_uncompressed_size((unsigned char *)ptr, size)) > 0)
	{
		irx_data = (unsigned char *)memalign(64, irx_size);
		ret = lzma2_uncompress((unsigned char *)ptr, size, irx_data, irx_size);
		
		if(ret > 0)
			ret = SifExecModuleBuffer( irx_data, irx_size, arg_len, args, mod_res );
		
		free(irx_data);	
	}

	return ret;
}

/* ===== EXIT-PATH BREADCRUMBS ( DIAGNOSTIC ) ================================
 * The udpfs exit hang has now survived TWO fixes ( 11891bd, ffe2e07 ) that were
 * both aimed at SifIopReset and later, and real ps2sdk source has since ruled out
 * everything BEFORE it -- SifExitIopHeap and SifLoadFileExit are pure EE-local
 * memsets ( ps2sdk ee/kernel/src/iopheap.c:52, loadfile.c:63 ), SifInitRpc /
 * SifExitRpc are EE-local, and the PFS umount is unreachable when g_PD < 0. So the
 * wedge is at or after the reset and we have never known WHICH statement. Stop
 * guessing and measure.
 *
 * Why this works where a debugger cannot: GUI_Status ( SMS_GUIDesktop.c:792 ) is
 * 100% EE-side -- GSFont measuring, EE-heap realloc, DMA_Wait(VIF1), then a GIF
 * packet. No SIF, no RPC, no IOP anywhere in it. So it still renders after
 * SifExitRpc ( which only touches SIF0 ) AND after the IOP reboots, because the
 * GS/GIF/VIF1/EE-heap are untouched by an IOP reset. It also flushes synchronously,
 * so a crumb printed before statement N stays latched on screen forever if N never
 * returns ( the clock timer is already dead by then, so nothing repaints over it ).
 *
 * Each crumb names the call ABOUT TO RUN => the last code on screen IS the blocking
 * call. The border colour encodes the same number, as a backstop if text ever dies.
 * Strings are kept short ( <= 18 chars, under "Loading boot browser" ) so the status
 * line never reallocs mid-teardown. */
void SMS_ExitCrumb ( int aN, const char* apWhat ) {

/* On-screen diagnostic breadcrumb. OFF in release builds; build with SMS_DIAG=1 to enable.
 *
 * Why this exists at all, and why it beats a network log for boot/network faults: it is a
 * pure EE-side GS write -- no SIF, no RPC, no IOP, no network, no memory card. It therefore
 * still renders after SifExitRpc, after an IOP reboot, and on a console with nothing
 * attached. It flushes synchronously, so a code printed immediately BEFORE a call stays
 * latched on screen forever if that call never returns.
 *
 * That last property is the whole point: the last code visible IS the blocking call. This is
 * how the UDPFS exit hang was found -- narrowing E10 vs E11 identified the exact spin.
 *
 * The border colour encodes the same number as a backstop if text rendering itself dies.
 * Keep the strings short ( <= 18 chars ) so the status line never reallocs mid-teardown. */
#ifdef SMS_DIAG

 char lB[ 32 ];

 sprintf ( lB, "E%02d %s", aN, apWhat );
 GS_BGCOLOR() = ( unsigned int )( aN * 0x000A0A0A ) | 0x00000030;
 GUI_Status ( lB );

#else
 ( void )aN;
 ( void )apWhat;
#endif

}  /* end SMS_ExitCrumb */

/* only on the EXIT reset: the boot reset ( afExit == 0, from main.c ) runs before the
 * GUI exists and must stay byte-identical. */
#define CRUMB( n, s ) do { if ( afExit ) SMS_ExitCrumb ( ( n ), ( s ) ); } while ( 0 )

/* Stage crumb for paths OUTSIDE the reset ( UDPFS bring-up, etc ). Compiles to NOTHING
 * without SMS_DIAG -- deliberately a macro rather than a call to a no-op function, because
 * the string literals themselves are data: five short ones were enough to push this tree's
 * small-data section past the GP-relative addressing window and break the link. A diagnostic
 * that is off must cost zero bytes, not almost zero. */
#ifdef SMS_DIAG
#define DIAG_CRUMB( n, s ) SMS_ExitCrumb ( ( n ), ( s ) )
#else
#define DIAG_CRUMB( n, s ) do { } while ( 0 )
#endif

void SMS_IOPReset ( int afExit ) {

 int i;

/* ⚠ TWO CALLERS, and the afExit branches below ARE LIVE:
 *     src/main.c:60      SMS_IOPReset ( 0 )  -- the boot reset.
 *     src/SMS_EE.c:628   "jal SMS_IOPReset" with "addiu $a0, $zero, 1" in the delay slot,
 *                        i.e. afExit = 1, from SMS_EExec -- the "Exit to -> EXEC0/EXEC1"
 *                        path. It is INLINE ASM, so it does NOT match a grep for
 *                        "SMS_IOPReset (" -- an earlier revision of this very comment
 *                        claimed "afExit is always 0" for exactly that reason. It is not.
 *
 * What DID change: the default "Exit SMS" -> boot browser path ( lIdx == 0 ) no longer
 * calls this function at all -- see the E11 FIX note in SMS_GUIMenuSMS.c. There the reset
 * was dead work before Exit(0) and hung whenever a bus-mastering IOP module was still live
 * ( measured with smap, then again with usbd ). So the afExit-gated code below -- the CRUMB
 * macro, the pre-reset SifInitRpc, the empty-arg-vs-s_pUDNL choice -- is now reached ONLY
 * via SMS_EExec, which is a non-default setting the user must deliberately select.
 *
 * That exec path is still exposed to the same hang and CANNOT take the same remedy: it
 * needs a freshly reset IOP to ExecPS2 into, so the reset there is real work, not dead
 * work. On a no-dev9 boot it takes the long s_pUDNL arm with none of the dev9 mitigations.
 * Deliberately left as-is: no safe minimal fix exists and this is boot/exit-critical code. */

/* EXIT-HANG NOTE ( the "hangs on Loading boot browser" after UDPFS/network ): the fix is
 * the reset ARGUMENT below, NOT a DEV9 shutdown here. A previous version power-cut DEV9
 * ( DEV9CTLSHUTDOWN ) at this point -- that was WRONG and made it worse: dev9Shutdown cuts
 * the SPEED device with raw register writes while the neutrino smap RX thread ( prio 8 )
 * can be mid dev9DmaTransfer busy-waiting `while(chcr & TR)` with NO timeout, so cutting
 * power strands that DMA and the thread spins forever. Removed. See the reset below. */

#if NO_DEBUG
 CRUMB (  4, "SifInitRpc 1" );
 SifInitRpc ( 0 );
 CRUMB (  5, "SifExitIopHeap" );
 SifExitIopHeap ();
 CRUMB (  6, "SifLoadFileExit" );
 SifLoadFileExit();
 CRUMB (  7, "SifExitRpc" );
 SifExitRpc     ();

/* E10 FIX ( measured 2026-07-20: the exit hangs at E10, the FIRST SifInitRpc AFTER the
 * reboot -- IOP rebooted, E08/E09 passed, but RPCINIT never comes ). The ONE code-verified
 * structural difference from EVERY proven-working reference: R3Z ( init.c:1786 ), OPL
 * ( system.c:192 ) and neutrino ( ee/loader/main.c:727 ) all make SifInitRpc(0) the LAST
 * SIF call before SifIopReset -- they enter the reset with the SIF0 command channel LIVE.
 * SMS uniquely entered it right after SifExitRpc(), whose sceSifExitCmd does
 * DisableDmac(DMAC_SIF0) + RemoveDmacHandler ( ps2sdk sifcmd.c:285 ), so SIF0 was TORN DOWN
 * across the dev9-active reboot window and the post-reset re-handshake at E10 never finished.
 * Re-init here so SIF0 is armed going into the reset, matching all three references. The IOP
 * is still alive at this point, so RPCINIT is already set and this returns immediately -- it
 * only leaves SIF0's DMAC handler in place for the reboot. This is DIFFERENT from both prior
 * failed fixes ( dev9 power-cut; empty-arg reset ), which never touched the pre-reset SIF
 * state. NOT claimed as certain: if E10 still shows on hardware, the hang is still the
 * post-reset SifInitRpc and the next build splits it into CMDINIT vs RPCINIT.
 * Gated to the SAME condition as the empty-arg reset ( afExit && DEV9 ): only the hanging
 * DEV9-exit path changes -- the boot reset ( afExit==0 ) and the already-working non-network
 * exits stay byte-identical, so this cannot regress boot or a non-network exit. */
/* NOW UNGATED ON DEV9 ( was afExit && DEV9 ). The only path that still reaches this
 * function with afExit==1 is SMS_EExec ( Exit to -> EXEC0/EXEC1 ), and it hangs for the
 * SAME reason on ANY bus-mastering module, not just smap -- usbd/OHCI included. Arming
 * SIF0 going into the reset is exactly as valid there as it was for dev9. The boot reset
 * ( afExit==0 ) is untouched, so this cannot regress boot. */
 if ( afExit ) SifInitRpc ( 0 );   /* EXEC and SWITCH: arm SIF0 going into the reset */

/* On an EXIT reset, use the EMPTY arg -- NOT s_pUDNL ("rom0:UDNL rom0:EELOADCNF"). This
 * is THE udpfs/network exit-hang fix. The UDNL variant runs a long IOP-kernel reload on
 * the still-live IOP; when DEV9 is powered, the neutrino smap RX driver keeps DMAing
 * inbound LAN frames ( it opens RX to broadcast/multicast, so there is always ARP/mDNS/
 * etc. traffic ) into IOP RAM DURING that reload, corrupting it -- the IOP never signals
 * BOOTEND and the EE spins forever in `while(!SifIopSync()){}` below. The empty-arg reset
 * is the short, tolerant reboot that every network-capable reference uses with a live
 * DEV9/smap ( wLaunchELF-R3Z, OPL, NHDDL, neutrino ) and it does NOT exhibit this. SMS
 * re-loads all its own IOP modules right after ( SifExecModuleBuffer, below ), so nothing
 * that the UDNL update path provided is lost. Gated narrowly on afExit && SMS_IOPF_DEV9
 * so ONLY the broken path changes: the boot reset ( afExit==0 ) and the already-working
 * non-network exits ( mc / USB / MX4SIO, DEV9 never powered ) keep s_pUDNL byte-identical;
 * only a DEV9-powered exit -- the one that hangs -- switches to the empty-arg reset. */
 CRUMB (  8, "SifIopReset" );      /* spins if sceSifSetDma keeps failing        */
/* NOW UNGATED ON DEV9 ( was afExit && DEV9 ). s_pUDNL is the LONG "rom0:UDNL rom0:EELOADCNF"
 * kernel reload, and its length IS the vulnerability: any IOP module still DMAing into IOP
 * RAM during it corrupts the reload, so the IOP never reaches BOOTEND. That was measured
 * with smap, but usbd/OHCI DMAs on its own too ( periodic SOF / interrupt endpoints ), so a
 * no-dev9 exec exit was taking the LONGEST window with NONE of the mitigations -- the worst
 * possible combination. The empty-arg reset is the short, tolerant reboot every other
 * loader uses with live bus masters ( OPL system.c, wLaunchELF-R3Z, NHDDL, neutrino ).
 * Nothing is lost: SMS re-loads its own IOP modules right below, and SMS_EExec then calls
 * SMS_IOPStartUSB itself. Boot ( afExit==0 ) keeps s_pUDNL byte-identical. */
 while(  !SifIopReset( afExit ? "" : s_pUDNL, 0 )  ){};   /* EXEC and SWITCH take the short, bus-master-tolerant reset */

 FlushCache(0);
 FlushCache(2);

 CRUMB (  9, "SifIopSync" );       /* IOP never sets BOOTEND -> never reboots    */
 while (!SifIopSync()) {;}

/* SPLIT of the old E10 ( "SifInitRpc 2" ), because Fix 1 did NOT cure it -- it still hangs
 * at the post-reset SifInitRpc. That call contains TWO independent infinite spins and the
 * single breadcrumb could not tell them apart:
 *   SITE A = SifInitCmd()'s CMDINIT wait ( ps2sdk sifcmd.c: while(!(SMFLAG & CMDINIT)) ) --
 *            waits on the IOP hardware raising CMDINIT. Hang here => the rebooted IOP never
 *            comes up far enough to raise CMDINIT => the IOP BOOT itself is wedged ( points
 *            at dev9/SPEED residual disturbing the reboot, NOT a SIF0 delivery problem ).
 *   SITE B = SifInitRpc()'s RPCINIT wait ( sifrpc.c: while(!getSreg(RPCINIT)) ) -- CMDINIT
 *            arrived but the IOP SIFRPC server never delivered SET_SREG over SIF0. Hang here
 *            => a SIF0 delivery/ordering problem ( the Fix-1 class ).
 * SifInitCmd() is idempotent ( if(sif0_id>=0) return ), so calling it here and then letting
 * SifInitRpc() call it again is safe -- the 2nd call returns immediately. Whichever E-code
 * ( E10 vs E11 ) latches on hardware tells us which spin, and therefore which fix. */
 CRUMB ( 10, "SifInitCmd" );       /* SITE A: CMDINIT ( IOP boot wedged if it hangs here ) */
 SifInitCmd ();
 CRUMB ( 11, "SifInitRpc" );       /* SITE B: RPCINIT ( SIF0 delivery if it hangs here )   */
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

 CRUMB ( 12, "RCX_Load" );
 RCX_Load  ();
 CRUMB ( 13, "RCX_Start" );
 RCX_Start ();
 CRUMB ( 14, "RCX_Open" );
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

 CRUMB ( 15, "SMSUTILS" );
 SifExecModuleBuffer ( &g_DataBuffer[ SMS_SMSUTILS_OFFSET ], SMS_SMSUTILS_SIZE, 0, NULL, &i );

#ifdef BDM
 CRUMB ( 16, "iomanx" );
 SifExecDecompModuleBuffer ( &iomanx_irx, size_iomanx_irx, 0, NULL, &i );
 CRUMB ( 17, "filexio" );
 SifExecDecompModuleBuffer ( &filexio_irx, size_filexio_irx, 0, NULL, &i );
 CRUMB ( 18, "fileXioInit" );
 fileXioInit();

 CRUMB ( 19, "bdm" );
 SifExecDecompModuleBuffer ( &bdm_irx, size_bdm_irx, 0, NULL, &i );
 CRUMB ( 20, "bdmfs" );
 SifExecDecompModuleBuffer ( &bdmfs_fatfs_irx, size_bdmfs_fatfs_irx, 0, NULL, &i );

/* sio2man: loaded on BOOT and on a MID-SESSION reset, skipped only on the exec exit.
 * The test is `!= 1` rather than `!afExit` so the two long-standing paths keep their exact
 * behaviour ( boot 0 still loads it, exec-exit 1 still skips it ) while the new mid-session
 * reset ( 2 ) gets it back -- without sio2man there is no mcman/mcserv/padman, i.e. no
 * memory card and no controller, which is fatal when SMS keeps running afterwards. The
 * exec exit does not need it because it is handing the machine to another ELF. */
 if ( afExit != SMS_IOPRESET_EXEC ) SifExecDecompModuleBuffer ( &sio2man_irx, size_sio2man_irx, 0, NULL, &i );

 CRUMB ( 21, "mcman" );
 SifExecDecompModuleBuffer ( &mcman_irx, size_mcman_irx, 0, NULL, &i );
 CRUMB ( 22, "mcserv" );
 SifExecDecompModuleBuffer ( &mcserv_irx, size_mcserv_irx, 0, NULL, &i );
 CRUMB ( 23, "padman" );
 SifExecDecompModuleBuffer ( &padman_irx, size_padman_irx, 0, NULL, &i );
#else
 static const char* lpModules[ 4 ] = { s_pSIO2MAN, s_pPADMAN, s_pMCMAN, s_pMCSERV };

 if ( !afExit ) SifExecModuleBuffer ( &g_DataBuffer[ SMS_SIO2MAN_OFFSET ], SMS_SIO2MAN_SIZE,  0, NULL, &i );

 for ( i = 1 - afExit; i < 4; ++i ) SifLoadModule ( lpModules[ i ], 0, NULL );
#endif

 CRUMB ( 24, "BindRPC SMSU" );
 SIF_BindRPC ( &s_SMSUClt, SMSUTILS_RPC_ID );

 DisableIntc(INTC_TIM0);
 DisableIntc(INTC_TIM1);

}  /* end SMS_IOPReset */

#undef CRUMB

int SMS_IOPStartNet ( int afStatus ) {

 int  i, j;
 char lSMAPArgs[ 80 ];
 int  lSMAPALen;

 if (  s_NetOwner == 2  ) return 0;   /* udpfs owns the NIC this boot -- can't also load SMS's SMAP */

 if (  !( g_IOPFlags & SMS_IOPF_DEV9_IS )  ) return 0;
 if (  !( g_IOPFlags & SMS_IOPF_DEV9    )  ) {
  DIAG_CRUMB ( 40, "dev9 init" );
  SMS_IOCtl ( g_pDEV9X, DEV9CTLINIT, NULL );
  g_IOPFlags |= SMS_IOPF_DEV9;
 }  /* end if */

 s_NetOwner = 1;   /* SMS SMB/host networking now owns the NIC */

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

 SifExecModuleBuffer ( &g_DataBuffer[ SMS_PS2IP_OFFSET ], SMS_PS2IP_SIZE, 0, NULL, &i );

 if ( i >= 0 ) {

  SifExecModuleBuffer ( &g_DataBuffer[ SMS_PS2SMAP_OFFSET ], SMS_PS2SMAP_SIZE, lSMAPALen, &lSMAPArgs[ 0 ], &i );

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

    SifExecModuleBuffer ( &g_DataBuffer[ SMS_PS2HOST_OFFSET ], SMS_PS2HOST_SIZE, 0, NULL, &i );

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

 if ( g_IOPFlags & SMS_IOPF_UMS ) return SMS_IOPF_USB;   /* usb-mass already up -> nothing to do. ( Guard on UMS not USB: ds34usb can load usbd ALONE, and this must still add usbmass + rescan on top of that. ) */

 if (  !( g_IOPFlags & SMS_IOPF_USB )  ) {               /* usbd may already be resident ( ds34usb loads it standalone ) -> don't double-load */
  SifExecDecompModuleBuffer ( &usbd_irx, size_usbd_irx, 0, NULL, &i );
  g_IOPFlags |= SMS_IOPF_USB;
 }  /* end if */

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

/* Idempotent, mirroring SMS_IOPStartMMCE: an mx4sio-card boot re-mounts the drive in
 * config resolution AND may then hit AUTO_MX4SIO -- without this, the second call
 * loaded a SECOND copy of mx4sio_bd onto the live SIO2 bus. The old double-load
 * happened to survive on hardware, but it was never a defined thing to do. */
 if (  g_IOPFlags & SMS_IOPF_MX4SIO  ) return g_IOPFlags & SMS_IOPF_MX4SIO;

/* MMCE ( mmceman ) drives the SAME SIO2 port, and neither module can be unloaded, so
 * whoever binds it first owns it for the boot -- the same first-come-first-served rule
 * as s_AtaBusOwner / s_NetOwner. SMS_IOPStartMMCE already defers to MX4SIO; this is the
 * missing other half. Loading mx4sio_bd onto a port mmceman already holds is undefined,
 * not merely useless, so refuse cleanly instead. MX4SIO is the primary device: nothing
 * on the boot path may start MMCE speculatively ahead of it ( cf. _cfg_resolve_fallback,
 * which probes MMCE but must never start it ). */
 if (  g_IOPFlags & SMS_IOPF_MMCE  ) return 0;

 for ( i = 0; i < 4; ++i ) if (  checkConnectedMassDev ( i )  ) before |= ( 1 << i );

/* Mirror of the residency rule in SMS_IOPStartMMCE: a load that did not take must not
 * claim the SIO2 bus, or a failed MX4SIO start would lock MMCE out for the boot. */
 if (  SifExecDecompModuleBuffer ( &mx4sio_bd_irx, size_mx4sio_bd_irx, 0, NULL, &i ) < 0  ) return 0;

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
 char*        lP1 = STR_ATA_PREP_BUS.m_pStr;   /* now translatable ( .lng 288..290 ) */
 char*        lP2 = STR_ATA_LOAD_DRV.m_pStr;
 char*        lP3 = STR_ATA_SCAN.m_pStr;

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

/* Is the single EMAC3 NIC already claimed by SMS's own SMB/host stack this boot? The
 * Device menu needs this to decide whether offering "Start UDPFS" is honest:
 * SMS_IOPStartNet claims s_NetOwner BEFORE loading its modules ( deliberately -- the
 * claim must stand even if a later module fails, so udpfs never lands on a half-built
 * NIC ), which means SMS_IOPF_NET/SMB can be clear while the NIC is still spoken for. */
int SMS_IOPNetOwnedBySMB   ( void ) { return s_NetOwner == 1; }
int SMS_IOPNetOwnedByUDPFS ( void ) { return s_NetOwner == 2; }

/* The network mode that is actually RUNNING right now, as opposed to the one saved in the
 * config. The two differ exactly when the user has changed the setting since boot, which is
 * the only case where an in-place switch is worth offering.
 * s_NetOwner alone cannot tell HOST from SMB -- both claim owner 1 before either module
 * loads -- so the residency flags decide which of the two actually came up. */
unsigned int SMS_IOPNetLiveMode ( void ) {

 if ( s_NetOwner == 2 ) return SMS_NETMODE_UDPFS;

 if ( s_NetOwner == 1 ) {

  if ( g_IOPFlags & SMS_IOPF_SMB ) return SMS_NETMODE_SMB;
  if ( g_IOPFlags & SMS_IOPF_NET ) return SMS_NETMODE_HOST;

/* Owner claimed but neither module up: a half-failed start. Report the mode the config asked
 * for, so a retry is offered rather than the menu silently agreeing with a dead stack. */
  return SMS_ConfigNetMode ();

 }  /* end if */

 return SMS_NETMODE_OFF;

}  /* end SMS_IOPNetLiveMode */

int SMS_IOPStartUDPFS ( int afStatus ) {

 int         i, lTry;
 char        lArg[ 24 ];
 static char lUdpfs[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) = "udpfs:/";
 char* lP1 = STR_UDPFS_PREP_NET.m_pStr;    /* now translatable ( .lng 279..287 ) */
 char* lP2 = STR_UDPFS_LOAD_ETH.m_pStr;
 char* lP3 = STR_UDPFS_START_IP.m_pStr;
 char* lP4 = STR_UDPFS_DISCOVER.m_pStr;
 char* lE1 = STR_UDPFS_FAIL_NOIP.m_pStr;
 char* lE2 = STR_UDPFS_FAIL_SMAP.m_pStr;
 char* lE3 = STR_UDPFS_FAIL_STACK.m_pStr;
 char* lE4 = STR_UDPFS_FAIL_DEV.m_pStr;
 char* lE5 = STR_UDPFS_FAIL_DISC.m_pStr;
 /* Per-module load state, mirroring wLaunchELF-R3Z's have_udpfs_* ( init.c:612 ). A
  * module that is already resident answers a second load with MODULE_NO_RESIDENT_END
  * ( its RegisterLibraryEntries fails ), which is NOT an error -- its library is still
  * there. Without these, a retry after a later-stage failure would report a bogus
  * [SMAP]/[STACK] failure for a module that is in fact up. */

 /* UDPFS ( "UDP File System", transport UDPRDMA, port 0xF5F6 / service 0xF5F5 ) serves
  * a PC FOLDER over UDP; it mounts as the iomanX device "udpfs:" -- NOT a BDM mass:
  * drive -- so it gets its own device id and browses/streams through fio like mmce:.
  * Three modules, in this order ( the order R3Z uses, init.c:612 ):
  *    udpfs_smap      the EMAC3 Ethernet driver         ( no args )
  *    udpfs_ministack the tiny IP/UDP/ARP stack         ( ip=<this PS2's static IP> )
  *    udpfs_ioman     the "udpfs:" iomanX device        ( no args )
  * DEV9 MUST be initialised BEFORE udpfs_smap: SMS's dev9Init wipes every registered
  * interrupt + DMA callback ( iop/SMSDev9/src/ps2dev9.c ), and smap registers its
  * callbacks once at load with no way to re-register, so a late DEV9CTLINIT silently
  * kills the DMA path. The SERVER is found by UDPRDMA broadcast discovery -- there is
  * no server address to configure; only this PS2's own static IP ( IPCONFIG.DAT ->
  * g_pDefIP ) is passed, to ministack. MUTUALLY EXCLUSIVE with SMS's SMB/host stack:
  * one EMAC3 NIC, and both register the 'smap' library -- one network personality per
  * boot, tracked by s_NetOwner ( no runtime swap without a full IOP reset ). Pure
  * dev9/SMAP, so it never touches the SIO2 bus: MX4SIO / MMCE are unaffected.
  * ( NOT udpbd: that is the legacy 0xBDBD BLOCK-device protocol needing a disk image,
  *   a subset of UDPFS. SMS no longer ships it -- see iop/SMSUdpfs/ATTRIBUTION.md. ) */
/* Idempotent on the DEVICE being present, NOT merely on the modules being loaded. The
 * modules can be up while discovery failed ( server not started yet, PHY still
 * negotiating ), and that must stay RETRYABLE: gating this on SMS_IOPF_UDPFS would
 * early-return without re-probing, and since the Device-menu row hides on the same
 * condition the user would be locked out of udpfs for the whole boot with the drive
 * simply absent -- the "nothing but Start ... no device, nothing" report. Re-entry is
 * cheap: s_fSmap/s_fStack/s_fIoman skip the loads and we land straight on the probe. */
 if (  g_UdpfsFlags & 2  ) return 1;   /* already mounted ( STICKY bit1 -- bit0 is the one-shot event and is cleared by the GUI within ~64ms ) */
 if (  s_NetOwner == 1 || ( g_IOPFlags & ( SMS_IOPF_NET | SMS_IOPF_SMB ) )  ) return 0;   /* SMB/host owns the NIC */
 if (  !( g_IOPFlags & SMS_IOPF_DEV9_IS )  ) return 0;   /* no DEV9 hardware */
 if (  !g_pDefIP[ 0 ]  ) { GUI_Error ( lE1 ); return 0; }   /* no static IP -> ministack has nothing to bind */

 GUI_Status ( lP1 );
 if (  !( g_IOPFlags & SMS_IOPF_DEV9 )  ) {
  SMS_IOCtl ( g_pDEV9X, DEV9CTLINIT, NULL );
  g_IOPFlags |= SMS_IOPF_DEV9;
 }  /* end if */

 /* Every stage below reports WHICH stage failed. A silent `return 0` here is what
  * turns a hardware test into five round-trips ( cf. the SMB self-diagnosing
  * "SMB FAIL [CONN|PROTO|LOGON]" in SMS_GUIDevMenu.c ). */
 GUI_Status ( lP2 );
 DIAG_CRUMB ( 41, "udpfs smap" );
 if ( !s_fSmap ) {
  if (  SifExecDecompModuleBuffer ( &udpfs_smap_irx, size_udpfs_smap_irx, 0, NULL, &i ) < 0 || i  ) {
   GUI_Error ( lE2 );
   return 0;
  }  /* end if */
  s_fSmap = 1;
 }  /* end if */

 /* smap is up -> the NIC is ours for this boot, even if a later stage fails. Claiming
  * it only now ( not before the load ) leaves SMB usable if smap itself never loaded. */
 s_NetOwner = 2;

 GUI_Status ( lP3 );
 DIAG_CRUMB ( 42, "udpfs stack" );
 if ( !s_fStack ) {
  sprintf ( lArg, "ip=%s", g_pDefIP );   /* single NUL-terminated arg token, argc-parsed by the module */
  if (  SifExecDecompModuleBuffer ( &udpfs_ministack_irx, size_udpfs_ministack_irx, strlen ( lArg ) + 1, lArg, &i ) < 0 || i  ) {
   GUI_Error ( lE3 );
   return 0;
  }  /* end if */
  s_fStack = 1;
 }  /* end if */

 DIAG_CRUMB ( 43, "udpfs ioman" );
 if ( !s_fIoman ) {
  if (  SifExecDecompModuleBuffer ( &udpfs_ioman_irx, size_udpfs_ioman_irx, 0, NULL, &i ) < 0 || i  ) {
   GUI_Error ( lE4 );
   return 0;
  }  /* end if */
  s_fIoman = 1;
 }  /* end if */

 g_IOPFlags |= SMS_IOPF_UDPFS;

 /* The device registers unconditionally and connects lazily on the first dopen ( see
  * the SMS note in iop/SMSUdpfs/udpfs/src/udpfs_ioman.c ), so this probe is what
  * actually establishes the link.
  *
  * NO EE-side delay between attempts, and only TWO of them: each fileXioDopen is a
  * SYNCHRONOUS SIF RPC that blocks right through udprdma_discover's full 5-SECOND
  * budget ( udpfs_core.c -> udprdma.c, itself 4 broadcast retries inside that window ).
  * The dopen IS the wait -- an added spin just pads a 5s block, and eight attempts
  * would freeze a serverless boot for ~40s. Two gives a ~10s window with 8 broadcasts,
  * ample for the SMAP PHY to finish autonegotiating on its own thread after _start.
  * Failing here is NOT fatal: the device stays registered and the Device-menu row stays
  * offered ( it is gated on g_UdpfsFlags, not on the module flag ), so starting the PC
  * server and hitting Start again re-probes and connects. */
 GUI_Status ( lP4 );
 DIAG_CRUMB ( 44, "udpfs probe" );
 for ( lTry = 0; lTry < 2; ++lTry ) {

  int lFD = fileXioDopen ( lUdpfs );

  if ( lFD >= 0 ) {
   fileXioDclose ( lFD );
   g_UdpfsFlags |= 3;   /* bit0: raise the mount event once; bit1: sticky "mounted" for the guard + menu row */
   break;
  }  /* end if */

 }  /* end for */

 if (  !( g_UdpfsFlags & 2 )  ) GUI_Error ( lE5 );

 return g_IOPFlags & SMS_IOPF_UDPFS;

}  /* end SMS_IOPStartUDPFS */

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

/* Flag RESIDENCY, not the attempt. SMS_IOPF_MMCE now also locks MX4SIO out of the shared
 * SIO2 bus ( SMS_IOPStartMX4SIO's reciprocal guard, and the Device-menu rows ), so
 * setting it after a load that did NOT take would cost the user MX4SIO -- the primary
 * device -- for the whole boot, on the strength of a button press that did nothing. */
 if (  SifExecDecompModuleBuffer ( &mmceman_irx, size_mmceman_irx, 0, NULL, &i ) < 0  ) return 0;

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
int SMS_IOPStartUDPFS ( int afStatus ) { return 0; }
/* s_NetOwner is a BDM-only concept, but these accessors are called from common menu
 * code ( SMS_GUIMenuSMS.c's Start-Network / Start-UDPFS rows, outside its #ifdef BDM ).
 * Without BDM there is no udpfs/SMB NIC ownership, so "never claimed" is truthful. */
int SMS_IOPNetOwnedBySMB   ( void ) { return 0; }
int SMS_IOPNetOwnedByUDPFS ( void ) { return 0; }
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

#ifdef BDM
 if ( g_IOPFlags & SMS_IOPF_SMBLOGIN ) {
  int lCP1251 = g_Config.m_DisplayCharset == GSCodePage_WinCyrillic;
  fileXioDevctl ( "smb:", SMB_DEVCTL_SMS_CP1251,
                  &lCP1251, sizeof ( lCP1251 ), NULL, 0 );
 }
#endif

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

/* MMCE is PROBED here but never STARTED. This resolver runs early in SMS_IOPInit --
 * BEFORE the configured auto-starts ( AUTO_MX4SIO / AUTO_MMCE ) -- and mmceman claims
 * the SIO2 bus permanently once loaded, with no unload path. Starting it speculatively
 * just to look for an SMS.cfg would therefore decide the SIO2 owner before the user's
 * own configuration is even read, and MX4SIO -- the primary device -- would be left
 * loading onto a bus mmceman already owns. The `!( g_IOPFlags & SMS_IOPF_MX4SIO )`
 * test below cannot protect against that: MX4SIO has not run yet at this point, so it
 * always passes. The trigger is a card-less network boot ( no mc, no USB ), which the
 * old libmc-uninitialised bug used to mask by making SMS_MCPresent() claim a card was
 * present and returning at the top.
 * So: only probe MMCE when something ELSE has already brought it up. If it is not up,
 * config simply stays on mc0: -- the same "genuinely nowhere to go" outcome as a boot
 * with no writable device at all, and a far better trade than silently costing the
 * user MX4SIO for the whole boot. */
 if (  g_IOPFlags & SMS_IOPF_MMCE  ) {                    /* already the SIO2 owner -> free to probe */

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

/* Load the DS3/DS4-over-USB driver ( ds34usb.irx ). It is read on the EE via SIF
 * RPC ( SMS_PadDS34Init ) -- input arrives over usbd, NOT the SIO2 pad bus, so
 * padman / MX4SIO are untouched. usbd is ensured first ( idempotent -> one usbd
 * instance ). Idempotent; a load failure is non-fatal ( the EE reader just yields
 * nothing and native pad input is unaffected ). */
int SMS_IOPStartDS34USB ( int afStatus ) {

 int i;

 (void)afStatus;

 if ( g_IOPFlags & SMS_IOPF_DS34USB ) return g_IOPFlags & SMS_IOPF_DS34USB;

/* ds34usb imports usbd -> load usbd ALONE if it isn't up yet. It does NOT need
 * usbmass_bd, so we skip the USB-mass stack + its multi-second settle loop
 * ( force-loading those on EVERY boot -- incl. MX4SIO / SD -- would be a needless
 * boot-path regression ). SMS_IOPStartUSB guards on SMS_IOPF_UMS, so a later
 * "Start USB" still adds usbmass + rescans on top of this. */
 if (  !( g_IOPFlags & SMS_IOPF_USB )  ) {
  SifExecDecompModuleBuffer ( &usbd_irx, size_usbd_irx, 0, NULL, &i );
  g_IOPFlags |= SMS_IOPF_USB;
 }  /* end if */

 if (  SifExecDecompModuleBuffer ( &ds34usb_irx, size_ds34usb_irx, 0, NULL, &i ) < 0  )
  return 0;   /* module didn't load -> DON'T flag it, so the EE skips ds34usb_init entirely ( no bind spin ) */

 g_IOPFlags |= SMS_IOPF_DS34USB;

 return SMS_IOPF_DS34USB;

}  /* end SMS_IOPStartDS34USB */

/* DS3/4 over Bluetooth ( ds34bt.irx: its own HCI/L2CAP stack over usbd + a USB BT
 * dongle ). Same shape as SMS_IOPStartDS34USB -- usbd alone, load-result checked,
 * idempotent, non-fatal on failure. Reads a controller that was already paired to
 * the dongle ( in-app pairing writes the dongle MAC into the pad, see the pair
 * handler ). Still off the SIO2 bus. */
int SMS_IOPStartDS34BT ( int afStatus ) {

 int i;

 (void)afStatus;

 if ( g_IOPFlags & SMS_IOPF_DS34BT ) return g_IOPFlags & SMS_IOPF_DS34BT;

 if (  !( g_IOPFlags & SMS_IOPF_USB )  ) {
  SifExecDecompModuleBuffer ( &usbd_irx, size_usbd_irx, 0, NULL, &i );
  g_IOPFlags |= SMS_IOPF_USB;
 }  /* end if */

 if (  SifExecDecompModuleBuffer ( &ds34bt_irx, size_ds34bt_irx, 0, NULL, &i ) < 0  )
  return 0;

 g_IOPFlags |= SMS_IOPF_DS34BT;

 return SMS_IOPF_DS34BT;

}  /* end SMS_IOPStartDS34BT */
#endif  /* BDM */

/* Load IPCONFIG.DAT into g_pDefIP / g_pDefMask / g_pDefGW.
 *
 * CWD first, memory card second, so a console with NO CARD can be given an address by
 * dropping the file next to SMS.ELF. Returns 1 if a file was read.
 *
 * Safe to call more than once: it only overwrites the globals when it actually reads a file,
 * so an early card read is never clobbered by a later CWD miss, and a later CWD hit correctly
 * wins over the card (that is the file the user just edited next to the ELF).
 *
 * Both probes are bare fioOpens, which merely error on an absent or unmounted device. */
static int _read_ipconfig ( void ) {

 char lBuff[ 64 ];
 int  lFD = -1;
 int  i;

 if ( g_pBootDir[ 0 ] ) {

  char lIPCwd[ 128 ];

  if (  strlen ( g_pBootDir ) + 13 < sizeof ( lIPCwd )  ) {

   strcpy ( lIPCwd, g_pBootDir );
   strcat ( lIPCwd, "IPCONFIG.DAT" );
   lFD = fioOpen ( lIPCwd, O_RDONLY );

  }  /* end if */

 }  /* end if */

 if ( lFD < 0 ) lFD = fioOpen ( g_pIPConf, O_RDONLY );

 if ( lFD < 0 ) return 0;

 memset (  lBuff, 0, sizeof ( lBuff )  );
 i = fioRead (  lFD, lBuff, sizeof ( lBuff ) - 1  );
 fioClose ( lFD );

 if ( i <= 0 ) return 0;

 {
  char lChr;

  lBuff[ i ] = '\x00';

  for (  i = 0; (  ( lChr = lBuff[ i ] ) != '\0'  ); ++i  )

   if (  lChr == ' ' || lChr == '\r' || lChr == '\n' ) lBuff[ i ] = '\x00';

  strncpy ( g_pDefIP, lBuff, 15 );
  i = strlen ( g_pDefIP ) + 1;
  strncpy ( g_pDefMask, lBuff + i, 15 );
  i += strlen ( g_pDefMask ) + 1;
  strncpy ( g_pDefGW, lBuff + i, 15 );
 }

 return 1;

}  /* end _read_ipconfig */

void SMS_IOPInit ( void ) {

 int         i, lFD;
 char        lBuff[ 64 ];
 ee_thread_t lThreadParam;

/* IPCONFIG.DAT, first attempt. Reads the memory card, plus a CWD probe that will only
 * succeed if the boot device happens to already be mounted.
 *
 * It is read THIS early because a boot from the UDPFS network drive needs this PS2's static
 * IP before the drive can be brought up at all -- that is a hard ordering constraint.
 *
 * But the boot device is NOT mounted yet: SMS resets the IOP in main.c, which drops every
 * driver, and the boot device is only re-mounted during config resolution further down. So a
 * CWD read here fails on exactly the card-less setups it was added for. _read_ipconfig is
 * therefore called AGAIN after that mount, which is where a CWD file actually becomes
 * readable. Diagnosed by GhostTownUS, who spotted that wLaunchELF-R3Z does not reset the IOP
 * and so keeps its boot driver live -- which is why config-off-card works there and did not
 * here. */
 _read_ipconfig ();

/* The DEV9 driver module ( index 1 -- SMS's dev9 v1.1 clone ) also loads ahead of
 * config resolution: it sets SMS_IOPF_DEV9_IS, which both the config-time udpfs start
 * AND the config-time SMS_IOPStartHDD gate on. ( The HDD gate is why a pfs/hdd boot's
 * config re-load silently no-opped before this hoist: DEV9_IS was never set that
 * early, so StartHDD returned 0 and the config stayed on defaults. ) ONLY index 1
 * moves: index 0 ( AUDSRV ) must stay after SifLoadModule( LIBSD ) below, whose
 * library it imports; index 2 ( POWEROFF ) keeps its original slot with it. The
 * DEV9 keep-up/shutdown DECISION also stays below -- it reads m_NetworkFlags, which
 * only means something after SMS_LoadConfig has run. */
 _load_module ( 1, 1 );

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
 if (  SMS_ConfigOnFS () && strncmp ( SMS_ConfigPath (), "mc", 2 ) != 0  ) {
/* mc boots are excluded on purpose. An mc-launched ELF now keeps its config in CWD
 * ( s_CfgOnFS = 1 ), but UNLIKE the FS devices below, the memory card re-mounts EARLY
 * ( MC_Init + mcman before GUI_Initialize ), so SMS_LoadConfig ALREADY read the CWD
 * config in the early pass ( SMS_GUI.c ) -- there is nothing to mount and nothing to
 * re-read here. Entering this block would call SMS_LoadConfig again, which resets
 * g_Config to defaults before re-reading and would CLOBBER the boot-time video-mode
 * override, so mc skips the whole late-mount + reload path exactly like a classic mc
 * boot did. ( "mmce" starts with "mmc", so this "mc" test never catches an MMCE path. ) */

  const char* lpCfg   = SMS_ConfigPath ();   /* e.g. "mmce0:/APPS/SMS.cfg" */
  int         lFSGone = 0;                    /* 1 = argv[0] boot device unreachable after the IOP reset */

  if ( strncmp ( lpCfg, "mmce", 4 ) == 0 ) {

   SMS_IOPStartMMCE ( 1 );   /* booted from SD2PSX / MemCard PRO */

  } else if ( strncmp ( lpCfg, "mass", 4 ) == 0 ) {

   int  lCfgFD, lRootFD, lN;
   char lDev[ 16 ];

   SMS_IOPStartUSB ( 1 );                       /* mass could be USB ... */

   lCfgFD = fioOpen ( lpCfg, O_RDONLY );        /* is the cfg actually on USB? */
   if ( lCfgFD >= 0 ) fioClose ( lCfgFD );
   else SMS_IOPStartMX4SIO ( 1 );               /* ... or MX4SIO ( also mass ) */

/* A "mass:" the LOADER mounted but SMS cannot bring back after its own IOP reset --
 * e.g. a network BLOCK drive ( udpbd / udpfs_bd ) under another loader, or a drive
 * that simply went away -- leaves us pinned to a device that no longer exists, and
 * every save then fails with NO memory-card fallback. Probe the device ROOT ( openable
 * even on a first-ever USB / MX4SIO boot that has no SMS.cfg yet, so a real drive
 * is never mistaken for a dead one ). Safe by construction: if this dopen fails,
 * SMS_LoadConfig's fioOpen below would fail too -- config is already unusable, so
 * degrading to the card can only help; a working drive has a mounted root -> keep. */
   for ( lN = 0; lN < 12 && lpCfg[ lN ] && lpCfg[ lN ] != ':'; ++lN ) lDev[ lN ] = lpCfg[ lN ];
   lDev[ lN ] = ':'; lDev[ lN + 1 ] = '/'; lDev[ lN + 2 ] = '\x00';   /* "massN:/" ( N up to 2+ digits ) */

   lRootFD = fioDopen ( lDev );
   if ( lRootFD >= 0 ) fioDclose ( lRootFD );
   else lFSGone = 1;

  } else if ( strncmp ( lpCfg, "pfs", 3 ) == 0 || strncmp ( lpCfg, "hdd", 3 ) == 0 ) {

   int lHddFD;

/* StartHDD loads atad/ps2hdd/ps2fs and now actually RUNS here ( the hoisted dev9 sets
 * DEV9_IS, which it gates on -- before the hoist it silently returned 0 ). But it does
 * NOT mount the boot PARTITION: nothing on the boot path issues PFS_IOCTL_MOUNT ( only
 * the browser's interactive partition-open does ), and the partition name is not
 * recoverable from a "pfs0:" path. So config-on-pfs cannot LOAD here. Probe it and, on
 * the expected miss, degrade to the memory card exactly like the mass / udpfs / SMB
 * paths -- the user's real card settings beat silent defaults. */
   SMS_IOPStartHDD ( 1 );                        /* internal HDD ( PFS ) -- loads the driver, does NOT mount the boot partition */

   lHddFD = fioOpen ( lpCfg, O_RDONLY );
   if ( lHddFD >= 0 ) fioClose ( lHddFD );        /* partition somehow mounted + cfg present -> keep */
   else lFSGone = 1;                              /* unmounted ( the normal case ) -> card fallback */

  } else if ( strncmp ( lpCfg, "udpfs", 5 ) == 0 ) {

/* Booted from the UDPFS network drive ( wLaunchELF-R3Z etc. -- argv[0] is
 * "udpfs:/..." ). Bring the drive back up and keep SMS.cfg on it. IPCONFIG and the
 * DEV9 module were hoisted above this block precisely so the start can succeed here;
 * SMS_IOPStartUDPFS itself surfaces a staged, modal error if it can't ( no IP / no
 * DEV9 / no server ), and every failure below degrades to the memory card via the
 * same lFSGone path a phantom mass: uses -- the SMB-boot behaviour, so this can only
 * ever ADD to what worked before.
 * The probe is deliberately a WRITABILITY probe, not a root probe: a udpfs server
 * can be started read-only, and pinning config to a read-only drive is the exact
 * "every save errors, no card fallback" trap that keeps SMB excluded from CWD
 * config. An existing SMS.cfg proves the drive ( reads work today; if the server is
 * read-only, saves error but settings still LOAD -- strictly better than losing
 * them ); no SMS.cfg -> create it empty, which both proves write access and gives
 * SMS_LoadConfig below a well-defined miss ( short read -> keeps defaults ) until
 * the first real save fills it. Creation failing for ANY reason -> card. */
   int lCfgFD;

   SMS_IOPStartUDPFS ( 1 );

   lCfgFD = fioOpen ( lpCfg, O_RDONLY );
   if ( lCfgFD >= 0 ) fioClose ( lCfgFD );
   else {
/* Deliberately NO O_TRUNC: this branch is only meant for a MISSING SMS.cfg, but if the
 * O_RDONLY above failed transiently ( server hiccup between the two opens ) while a
 * real config exists, a truncating probe would wipe the user's settings just to prove
 * writability. O_CREAT alone proves exactly as much and cannot destroy anything. */
    lCfgFD = fioOpen ( lpCfg, O_WRONLY | O_CREAT );
    if ( lCfgFD >= 0 ) fioClose ( lCfgFD );
    else lFSGone = 1;
   }  /* end else */

  }  /* end else if */

  if ( lFSGone ) {   /* phantom "mass:" / unreachable udpfs: degrade to the memory-card fallback, exactly like an SMB boot */
   SMS_ConfigClearFS     ();
   _cfg_resolve_fallback ();
  }  /* end if */

  SMS_LoadConfig ();
  GUI_SetColors  ();
  SMS_LocaleInit ();   /* re-read SMS.lng now the boot device is mounted ( CWD-first ) */
  SMS_LocaleSet  ();

/* IPCONFIG.DAT, second attempt -- for the SAME reason SMS.lng is re-read on the line above:
 * the boot device only became mountable a few lines up. The early attempt near the top of
 * this function runs before the IOP has any driver for it, so a CWD IPCONFIG.DAT could never
 * be opened there. This is the point where it can.
 *
 * Only overwrites the address when a file is actually read, so a memory-card IPCONFIG picked
 * up earlier survives if there is no CWD copy -- and a CWD copy correctly wins if there is
 * one, since that is the file sitting next to the ELF the user just edited. */
  _read_ipconfig ();

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

/* LIBSD, and whether AUDSRV can follow it.
 *
 * The result used to be discarded. On a PSX (DESR) that is fatal: the DESR BIOS has NO
 * rom0:LIBSD -- its ROM carries PLIBSD (the XMB variant) and TLIBSD (testmode), neither
 * interchangeable with the standard one -- so the load fails, AUDSRV cannot resolve its
 * imports and never loads, no audio RPC server is ever registered, and SPU_Initialize then
 * blocked forever waiting to bind to it. Diagnosed by israpps from a DESR-5000 ROM dump.
 *
 * Now the failure is detected and audio is simply skipped: no LIBSD means no AUDSRV, which
 * means no SPU. Silence is a very small loss next to a console that will not finish booting,
 * and every other device still comes up. ( SIF_BindRPC is separately bounded now, so even an
 * unforeseen missing server degrades instead of hanging -- two independent guards, because
 * this class of failure is invisible until it wedges someone's hardware. )
 *
 * A PSX could get audio back by shipping ps2sdk's own libsd as an embedded IRX rather than
 * relying on the BIOS copy. That is the proper fix and is worth doing; it needs a build path
 * and hardware to verify, so it is deliberately not bundled into a hang fix. */
 s_fLIBSD = SifLoadModule ( s_pLIBSD, 0, NULL ) >= 0;

#ifdef BDM
/* FALLBACK: our own libsd when the BIOS has none.
 *
 * The console's copy is tried FIRST and is used whenever it exists, so a retail PS2 takes
 * exactly the path it always did -- same module, same behaviour, and this embedded copy is
 * never touched. It exists solely for machines whose BIOS lacks the file.
 *
 * That is the PSX / DESR: its ROM carries PLIBSD (XMB, v3.4) and TLIBSD (testmode, v1.4),
 * neither a drop-in for the standard module, and no plain LIBSD at all. Without this those
 * consoles run SMS with NO SOUND -- unacceptable for a media player, which is why the module
 * is shipped rather than the failure merely being survived.
 *
 * ps2sdk's freesd exports libsd v1.4 with every ordinal SMS's AUDSRV imports
 * ( {4,5,7,9,11,15,17,18,19,20,26,27} ) mapping to a real function, none a _retonly stub --
 * checked by tools/build_libsd.sh, which is what makes it a genuine drop-in rather than a
 * module that binds clean and then does nothing. */
 if ( !s_fLIBSD ) {

  int lRes;

  s_fLIBSD = SifExecDecompModuleBuffer ( &libsd_irx, size_libsd_irx, 0, NULL, &lRes ) >= 0;

 }  /* end if */
#endif

 SMS_IOPDVDVInit ();

/* IPCONFIG read + the DEV9 module ( s_LoadParams index 1 ) were hoisted ABOVE the
 * config-resolution block -- see the note there. AUDSRV stays here because it imports
 * the LIBSD library loaded just above; POWEROFF keeps its original slot. */
 DIAG_CRUMB ( 50, "AUDSRV" );
 if ( s_fLIBSD ) _load_module ( 0, 1 );   /* AUDSRV imports LIBSD; pointless and unsafe without it */
/* E51/E52 bracket the POWEROFF load specifically. A PSX (DESR-7100) was reported hanging with
 * "Loading POWEROFF" on screen -- but that message is printed BEFORE the load, and the next
 * status update is some way past the DEV9 block below, so the stall could be either the
 * module's _start or the DEV9 work that follows. SifExecModuleBuffer is synchronous, so an
 * IOP module whose _start never returns hangs the EE with no timeout. These two codes
 * separate the cases; guessing between them would mean changing boot behaviour on hardware
 * that cannot be tested here. */
 DIAG_CRUMB ( 51, "POWEROFF" );
 _load_module ( 2, 1 );
 DIAG_CRUMB ( 52, "past POWEROFF" );

 if ( g_IOPFlags & SMS_IOPF_DEV9_IS ) {
#if NO_DEBUG
/* The !SMS_IOPF_DEV9 term: config resolution now runs BEFORE this point and may have
 * legitimately powered DEV9 up ( a udpfs boot re-mounting its config drive, or an hdd
 * boot mounting pfs ). Shutting DEV9 down here on the strength of the AUTO flags alone
 * would kill the device the config was just loaded from, mid-boot. Once claimed, keep. */
  if (   !( g_IOPFlags & SMS_IOPF_DEV9 ) &&
         !(  g_Config.m_NetworkFlags & ( SMS_DF_AUTO_HDD | SMS_DF_AUTO_NET | SMS_DF_AUTO_ATA | SMS_DF_AUTO_UDPFS )  )   )
   SMS_IOCtl ( g_pDEV9X, DEV9CTLSHUTDOWN, NULL );
  else g_IOPFlags |= SMS_IOPF_DEV9;
#else
  g_IOPFlags |= SMS_IOPF_DEV9;
#endif  /* NO_DEBUG */
 }  /* end if */

 DIAG_CRUMB ( 53, "SPU init" );
 if ( s_fLIBSD ) SPU_Initialize ();   /* no LIBSD -> no AUDSRV -> nothing to bind to */

 DIAG_CRUMB ( 54, "HDD APA" );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_HDD ) SMS_IOPStartHDD ( 1 );

/* NETWORK: exactly one stack, chosen through the mode accessor rather than by testing the
 * raw bits. It used to read SMS_DF_AUTO_NET here and SMS_DF_AUTO_UDPFS further down, which
 * meant a config with both set -- writable by any older build, since the bits are
 * independent -- started HOST/SMB and then hit SMS_IOPStartUDPFS's `s_NetOwner == 1` guard,
 * which returns 0 SILENTLY. The user got no network drive and no explanation.
 * Going through SMS_ConfigNetMode makes that unrepresentable: it resolves such a config the
 * same way the console actually behaved, and it is the same value the menu paints from, so
 * the lit row is always the stack that really starts. */
 DIAG_CRUMB ( 55, "network" );
 switch (  SMS_ConfigNetMode ()  ) {

  case SMS_NETMODE_HOST :
  case SMS_NETMODE_SMB  : SMS_IOPStartNet ( 1 ); break;   /* one function; SMS_DF_SMB picks smbman vs ps2host */
#ifdef BDM
  case SMS_NETMODE_UDPFS: SMS_IOPStartUDPFS ( 1 ); break;
#endif
  default               : break;                          /* SMS_NETMODE_OFF */

 }  /* end switch */

 DIAG_CRUMB ( 56, "USB" );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_USB ) SMS_IOPStartUSB ( 1 );
#ifdef BDM
 DIAG_CRUMB ( 57, "MX4SIO" );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_MX4SIO ) SMS_IOPStartMX4SIO ( 1 );
 DIAG_CRUMB ( 58, "HDD BDM" );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_ATA    ) SMS_IOPStartATA    ( 1 );
 DIAG_CRUMB ( 59, "iLink" );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_ILINK  ) SMS_IOPStartILINK  ( 1 );
 DIAG_CRUMB ( 60, "MMCE" );
 if ( g_Config.m_NetworkFlags & SMS_DF_AUTO_MMCE   ) SMS_IOPStartMMCE   ( 1 );
#endif

 GUI_Status ( STR_INITIALIZING_SMS.m_pStr );

 DIAG_CRUMB ( 61, "poweroff thr" );
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

 DIAG_CRUMB ( 62, "IOP init done" );

}  /* end SMS_IOPInit */

#ifdef BDM
/* ---------------------------------------------------------------------------------------
 * IN-PLACE NETWORK STACK SWITCH
 *
 * SMS can run exactly one network stack per boot -- one NIC, s_NetOwner claimed before any
 * module loads, and nothing in the tree can unload an IOP module. Historically that meant a
 * mode change only took effect on the NEXT launch, with nothing on screen to say so.
 *
 * wLaunchELF-R3Z solves the same problem by REBOOTING THE IOP in place rather than
 * relaunching the app ( refs/wLaunchELF_R3Z/src/init.c: switchNetworkStack ->
 * resetRuntimeDeviceState -> Reset ). Its rule is: if the requested stack is already the
 * live one do nothing, and only reset when a DIFFERENT stack is already up. That is exactly
 * the behaviour asked for here, so this mirrors it.
 *
 * The order below is not arbitrary:
 *   1. SAVE FIRST. The reset drops every storage driver, so the config must already be on
 *      disk before it happens -- otherwise a switch could lose the very setting that caused
 *      it. ( This is also why sio2man is kept on a SWITCH reset: mc must come back. )
 *   2. QUIESCE. The DS34 pad poller is an EE thread issuing SIF RPCs to ds34usb/ds34bt at
 *      ~60Hz. An RPC in flight across an IOP reboot is the documented hang class, so we take
 *      the poller's own mutex -- the one it holds around every read -- and keep it until the
 *      new stack is up. R3Z does the same thing with stopDs34Input().
 *   3. UNMOUNT what has a handle open ( PFS ), like R3Z's unmountAll().
 *   4. RESET, short-arg, sio2man retained.
 *   5. FORGET the residency state. SMS tracks loaded modules in g_IOPFlags and a handful of
 *      statics; at boot those start zeroed, but MID-SESSION they are stale the moment the IOP
 *      reboots. Leaving them set makes every later start a no-op against a module that is no
 *      longer there. R3Z clears ~35 have_* flags at the same point for the same reason.
 *   6. REBUILD: dev9 for the NIC, then the requested stack.
 *
 * Returns 1 if the switch was performed, 0 if it was unnecessary or refused.
 * ------------------------------------------------------------------------------------- */
int SMS_IOPNetSwitch ( unsigned int aMode ) {

 int lPrev = s_NetOwner;

/* Nothing is live -> the mode is a plain autostart preference and takes effect on the next
 * launch. No reset, no prompt: resetting here would cost the user their mounted devices to
 * achieve nothing. */
 if ( !lPrev ) return 0;

/* 1. Persist BEFORE anything is torn down. */
 SMS_SaveConfig ();

 GUI_Status ( STR_INITIALIZING_NETWORK.m_pStr );

/* 2. Stop the pad poller touching the IOP across the reboot. */
 SMS_PadDS34Lock ();

/* 3. Release the PFS handle if the internal HDD is mounted. */
 if ( g_PD >= 0 ) {
  SMS_IOCtl ( g_pPFS, PFS_IOCTL_UMOUNT, NULL );
  g_PD = -1;
 }  /* end if */

/* 4. Short, bus-master-tolerant reset that keeps sio2man ( mc + pads ). */
 SMS_IOPReset ( SMS_IOPRESET_SWITCH );

/* 5. Forget everything the reset invalidated. DEV9_IS and EURO are hardware/region FACTS,
 *    not module residency, so they survive; every driver bit does not. */
 g_IOPFlags &= ( SMS_IOPF_DEV9_IS | SMS_IOPF_EURO );

 s_NetOwner    = 0;
 s_AtaBusOwner = 0;
 s_MassAdded   = 0;
 s_MmceAdded   = 0;
 g_Mx4sioMask  = 0;
 g_AtaMask     = 0;
 g_IlinkMask   = 0;
 g_UdpfsFlags  = 0;
 g_MmceFlags   = 0;
 s_fSmap = s_fStack = s_fIoman = 0;

/* 6. Bring the requested stack up. Each starter re-loads dev9 itself as needed. */
 switch ( aMode ) {

  case SMS_NETMODE_HOST :
  case SMS_NETMODE_SMB  : SMS_IOPStartNet   ( 1 ); break;
  case SMS_NETMODE_UDPFS: SMS_IOPStartUDPFS ( 1 ); break;
  default               :                          break;   /* OFF -- leave the NIC idle */

 }  /* end switch */

 SMS_PadDS34Unlock ();

 return 1;

}  /* end SMS_IOPNetSwitch */
#endif  /* BDM */


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
