/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2006-2008 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#include "SMS_GUI.h"
#include "SMS_CDDA.h"
#include "SMS_CDVD.h"
#include "SMS_EE.h"
#include "SMS_RC.h"
#include "SMS_IOP.h"
#include "SMS_PAD.h"
#include "SMS_DSP.h"
#include "SMS_DMA.h"
#include "SMS_Locale.h"
#include "SMS_Config.h"
#include "SMS_History.h"
#include "SMS_OS.h"
#include "SMS_GS.h"
#include "SMS_PgInd.h"
#include "SMS_CDDA.h"
#include "SMS_VIF.h"
#include <kernel.h>
#include <sys/ioctl.h>
#include <fileio.h>

extern char g_pBootDir[];   /* SMS_Config.c: "<dev>/path/" on a non-mc boot, empty on mc */

#ifdef DISABLE_EXTRA_TIMERS_FUNCTIONS
DISABLE_EXTRA_TIMERS_FUNCTIONS();
#endif

int main ( int argc, char** argv ) {

 if ( argc > 0 && argv[ 0 ][ 0 ] == 'm' && argv[ 0 ][ 1 ] == 'c' ) {

  char lSlot = argv[ 0 ][ 2 ];

  SMS_SetMCSlot ( lSlot );
  g_pIPConf   [ 2 ] = lSlot;
  g_pBXDATASYS[ 2 ] = lSlot;
  g_pSMSSkn   [ 2 ] = lSlot;
  g_pSMSRMMAN [ 2 ] = lSlot;
  g_pExec0    [ 2 ] = lSlot;
  g_pExec1    [ 2 ] = lSlot;
  g_MCSlot          = lSlot - '0';

/* Settings live next to the ELF ( CWD ) on an mc boot too -- same fio path as every other
 * device -- instead of the fragile fixed mc?:/SMS/ libmc+icon.sys save. The slot patches
 * above still pin IPCONFIG.DAT + the exec/skin paths to the boot card. */
  SMS_ConfigSetCWD ( argv[ 0 ] );

 } else if ( argc > 0 ) SMS_ConfigSetCWD ( argv[ 0 ] );   /* non-mc boot -> settings in CWD ( next to the ELF ) */

 SMS_IOPReset ( 0 );
 SMS_EEInit   ();
 CDVD_Init    ();
 CDDA_Init    ();

 GUI_Initialize ( 1 );
 SMS_PgIndStart ();
 GUI_Status ( STR_INITIALIZING_SMS.m_pStr );
#ifndef EMBEDDED
 if ( g_Config.m_BrowserFlags & SMS_BF_UXH ) SMS_OSInit ( argv[ 0 ] );
#endif  /* EMBEDDED */
 SMS_IOPInit     ();
 SMS_EEPort2Init ();
#ifdef BDM
 {
  int lDS34u = SMS_IOPStartDS34USB ( 1 );   /* DS3/4 over USB ( wired )                          */
  int lDS34b = SMS_IOPStartDS34BT  ( 1 );   /* DS3/4 over Bluetooth ( paired controller + dongle ) */
  if ( lDS34u || lDS34b ) SMS_PadDS34Init ();   /* bind + poll if either driver loaded ( off the SIO2 bus ) */
 }
#endif
 CDVD_SetSpeed   ();

 if (  CDDA_DiskType () != DiskType_None  ) CDVD_Stop ();

 SMS_LoadXLT ();
 SMS_EEScanDir ( g_pMC0SMS, g_pExtMBF, g_Config.m_pMBFList );
 if ( g_pBootDir[ 0 ] ) SMS_EEScanDir ( g_pBootDir, g_pExtMBF, g_Config.m_pMBFList );   /* CWD .mbf too ( dedups ) */
 SMS_HistoryLoad ();
 GUI_DeleteObject ( g_pVerStr );
 SMS_PgIndStop ();

#ifdef BDM
/* SMS_IOPInit loaded the config only after the first GUI_Initialize had painted with
 * defaults: the boot device ( or the memory card it fell back to ) was not readable
 * that early. The loading spinner is stopped now, so re-init the GUI ( same call as
 * returning from the player ) to apply the saved display mode, picture position and
 * size, charset and skin. An mc boot, and a cdrom / host / SMB boot with a card, read
 * their config early and never set g_CfgLate. */
 if ( g_CfgLate ) GUI_Initialize ( 0 );
#endif

 GUI_Run ();

 return 0;

}  /* end main */
