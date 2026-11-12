/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2006-2008 Eugene Plotnikov <e-plotnikov@operamail.com>
# (c) 2006 bix64
# (c) 2007 lior e
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#include "SMS.h"
#include "SMS_GUIMenu.h"
#include "SMS_GS.h"
#include "SMS_PAD.h"
#include "SMS_Locale.h"
#include "SMS_GUIcons.h"
#include "SMS_Config.h"

extern char g_pBootDir[];   /* SMS_Config.c: "<dev>/path/" on a non-mc boot, empty on mc */
extern int  g_LngLines;     /* SMS_Locale.c -- 0 = no SMS.lng found, >0 = lines parsed */
extern const int g_LngExpect;   /* SMS_Locale.c -- lines a valid pack must have */
#include "SMS_IOP.h"
#include "SMS_MC.h"
#include "SMS_FileDir.h"
#include "SMS_EE.h"
#include "SMS_Timer.h"
#include "SMS_SPU.h"
#include "SMS_Sounds.h"
#include "SMS_RC.h"
#include "SMS_CDVD.h"
#include "SMS_GUIClock.h"
#include "SMS_IOP.h"
#include "SMS_ioctl.h"
#include "libds34usb.h"   /* ds34usb_get_bdaddr / ds34usb_set_bdaddr -- BT pairing */
#include "libds34bt.h"    /* ds34bt_get_bdaddr ( dongle MAC )        -- BT pairing */

#include <kernel.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <libhdd.h>
#include <sys/ioctl.h>
#include <sifrpc.h>

typedef struct _Sample {

 DECLARE_GUI_OBJECT()

} _Sample;

static int        s_fHDD;
static GUIObject* s_pSample;

static void _display_handler  ( GUIMenu*, int );
static void _device_handler   ( GUIMenu*, int );
static void _browser_handler  ( GUIMenu*, int );
static void _player_handler   ( GUIMenu*, int );
static void _help_handler     ( GUIMenu*, int );
       void _save_handler     ( GUIMenu*, int );
       void _shutdown_handler ( GUIMenu*, int );
       void _exit_handler     ( GUIMenu*, int );

static void _tvsys_handler    ( GUIMenu*, int );
static void _charset_handler  ( GUIMenu*, int );
static void _lang_handler     ( GUIMenu*, int );
       void _adjleft_handler  ( GUIMenu*, int );
       void _adjright_handler ( GUIMenu*, int );
       void _adjup_handler    ( GUIMenu*, int );
       void _adjdown_handler  ( GUIMenu*, int );

static void _cntslot_handler  ( GUIMenu*, int );
#ifndef BDM
static void _autonet_handler  ( GUIMenu*, int );
#endif
static void _autousb_handler  ( GUIMenu*, int );
#ifdef BDM
static void _automx4sio_handler ( GUIMenu*, int );
static void _autoata_handler    ( GUIMenu*, int );
static void _autoilink_handler  ( GUIMenu*, int );
static void _automce_handler    ( GUIMenu*, int );
static void _autoudpfs_handler  ( GUIMenu*, int );
static void _autohost_handler   ( GUIMenu*, int );
static void _autosmb_handler    ( GUIMenu*, int );
#endif
static void _autohdd_handler  ( GUIMenu*, int );
static void _starthost_handler ( GUIMenu*, int );
#ifdef BDM
static void _startsmb_handler  ( GUIMenu*, int );
#endif
static void _startusb_handler ( GUIMenu*, int );
#ifdef BDM
static void _startmx4sio_handler ( GUIMenu*, int );
static void _startata_handler    ( GUIMenu*, int );
static void _startilink_handler  ( GUIMenu*, int );
static void _startmce_handler    ( GUIMenu*, int );
static void _startudpfs_handler  ( GUIMenu*, int );
static void _pairbt_handler      ( GUIMenu*, int );
#endif
static void _starthdd_handler ( GUIMenu*, int );
static void _refresh_handler  ( GUIMenu*, int );
extern int  _smb_logon        ( void );   /* SMS_GUIDevMenu.c -- (re)connect SMB */
extern int  g_SMBError;                   /* SMS_GUI.c -- last SMB logon result  */
extern char g_SaveDiag[];                 /* SMS_Config.c -- TEMP: failing save stage */
static void _editipc_handler  ( GUIMenu*, int );
static void _cdvd_handler     ( GUIMenu*, int );
static void _cdvd_spd_handler ( GUIMenu*, int );
static void _dirbtn_handler   ( GUIMenu*, int );

static void _ip1_handler     ( GUIMenu*, int );
static void _ip2_handler     ( GUIMenu*, int );
static void _ip3_handler     ( GUIMenu*, int );
static void _ip4_handler     ( GUIMenu*, int );
static void _nm1_handler     ( GUIMenu*, int );
static void _nm2_handler     ( GUIMenu*, int );
static void _nm3_handler     ( GUIMenu*, int );
static void _nm4_handler     ( GUIMenu*, int );
static void _gw1_handler     ( GUIMenu*, int );
static void _gw2_handler     ( GUIMenu*, int );
static void _gw3_handler     ( GUIMenu*, int );
static void _gw4_handler     ( GUIMenu*, int );
static void _saveipc_handler ( GUIMenu*, int );

static void _usebg_handler  ( GUIMenu*, int );
static void _sound_handler  ( GUIMenu*, int );
static void _sortfs_handler ( GUIMenu*, int );
static void _filter_handler ( GUIMenu*, int );
static void _dsphdl_handler ( GUIMenu*, int );
static void _usexh_handler  ( GUIMenu*, int );
static void _hidesp_handler ( GUIMenu*, int );
static void _abclr_handler  ( GUIMenu*, int );
static void _ibclr_handler  ( GUIMenu*, int );
static void _txtclr_handler ( GUIMenu*, int );
static void _sltxt_handler  ( GUIMenu*, int );
static void _sclr_handler   ( GUIMenu*, int );
static void _exit_2_handler ( GUIMenu*, int );
static void _pwrbtn_handler ( GUIMenu*, int );

static void _autols_handler  ( GUIMenu*, int );
static void _opaqs_handler   ( GUIMenu*, int );
static void _dsbt_handler    ( GUIMenu*, int );
static void _aadsp_handler   ( GUIMenu*, int );
static void _spdif_handler   ( GUIMenu*, int );
static void _pdw22_handler   ( GUIMenu*, int );
       void _subclr_handler  ( GUIMenu*, int );
       void _subbclr_handler ( GUIMenu*, int );
       void _subiclr_handler ( GUIMenu*, int );
       void _subu_handler    ( GUIMenu*, int );
static void _sbclr_handler   ( GUIMenu*, int );
static void _vbclr_handler   ( GUIMenu*, int );
static void _clres_handler   ( GUIMenu*, int );
static void _vol_handler     ( GUIMenu*, int );
static void _salign_handler  ( GUIMenu*, int );
static void _suboff_handler  ( GUIMenu*, int );
static void _poff_handler    ( GUIMenu*, int );
static void _sblen_handler   ( GUIMenu*, int );
static void _sbpos_handler   ( GUIMenu*, int );
static void _sfonth_handler  ( GUIMenu*, int );
static void _sfontv_handler  ( GUIMenu*, int );
static void _smbcs_handler   ( GUIMenu*, int );
static void _advset_handler  ( GUIMenu*, int );
static void _disph_handler   ( GUIMenu*, int );
static void _dispw_handler   ( GUIMenu*, int );
static void _dispc_handler   ( GUIMenu*, int );
static void _disps1_handler  ( GUIMenu*, int );
static void _disps2_handler  ( GUIMenu*, int );
static void _disps3_handler  ( GUIMenu*, int );
static void _apply_handler   ( GUIMenu*, int );

static void _enter_sample ( GUIMenu* );
static void _leave_sample ( GUIMenu* );

static void _mp3_handler        ( GUIMenu*, int );
static void _mp3_rand_handler   ( GUIMenu*, int );
static void _mp3_repeat_handler ( GUIMenu*, int );
static void _mp3_asd_handler    ( GUIMenu*, int );
static void _mp3_adp_handler    ( GUIMenu*, int );

static void _network_handler ( GUIMenu*, int );
static void _netprot_handler ( GUIMenu*, int );
static void _smbsrv_handler  ( GUIMenu*, int );
static void _opmode_handler  ( GUIMenu*, int );
static void _duplex_handler  ( GUIMenu*, int );
static void _standt_handler  ( GUIMenu*, int );

static GUIMenuItem s_SMSMenu[] __attribute__(   (  section( ".data" )  )   ) = {
 { 0, &STR_DISPLAY_SETTINGS,     GUICON_DISPLAY, 0, _display_handler,  0, 0 },
 { 0, &STR_DEVICE_SETTINGS,      GUICON_NETWORK, 0, _device_handler,   0, 0 },
 { 0, &STR_BROWSER_SETTINGS,     GUICON_BROWSER, 0, _browser_handler,  0, 0 },
 { 0, &STR_PLAYER_SETTINGS,      GUICON_PLAYER,  0, _player_handler,   0, 0 },
 { 0, &STR_HELP,                 GUICON_HELP,    0, _help_handler,     0, 0 },
 { 0, &STR_SAVE_SETTINGS,        GUICON_SAVE,    0, _save_handler,     0, 0 },
 { 0, &STR_SHUTDOWN_CONSOLE,     GUICON_EXIT,    0, _shutdown_handler, 0, 0 },
 { 0, &STR_EXIT_TO_BOOT_BROWSER, GUICON_FINISH,  0, _exit_handler,     0, 0 }
};

static GUIMenuItem s_DispMenu[] __attribute__(   (  section( ".data" )  )   ) = {
 { MENU_ITEM_TYPE_TEXT, &STR_TV_SYSTEM,          0, 0, _tvsys_handler,    0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_CHARACTER_SET,      0, 0, _charset_handler,  0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_LANGUAGE,           0, 0, _lang_handler,     0, 0 },
 {                   0, &STR_ADJUST_IMAGE_LEFT,  0, 0, _adjleft_handler,  0, 0 },
 {                   0, &STR_ADJUST_IMAGE_RIGHT, 0, 0, _adjright_handler, 0, 0 },
 {                   0, &STR_ADJUST_IMAGE_UP,    0, 0, _adjup_handler,    0, 0 },
 {                   0, &STR_ADJUST_IMAGE_DOWN,  0, 0, _adjdown_handler,  0, 0 },
 {                   0, &STR_ADVANCED_SETTINGS,  0, 0, _advset_handler,   0, 0 }
};

static GUIMenuItem s_AdvDispMenu[] __attribute__(   (  section( ".data" )  )   ) = {
 { MENU_ITEM_TYPE_TEXT, &STR_DISPLAY_HEIGHT,     0, 0, _disph_handler,  0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_DISPLAY_WIDTH,      0, 0, _dispw_handler,  0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_COLOR_RESOLUTION,   0, 0, _dispc_handler,  0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_SYNC_PAR_1,         0, 0, _disps1_handler, 0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_SYNC_PAR_2,         0, 0, _disps2_handler, 0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_SYNC_PAR_3,         0, 0, _disps3_handler, 0, 0 },
 {                   0, &STR_APPLY_SETTINGS,     0, 0, _apply_handler,  0, 0 }
};

/* The device-menu labels below ( MX4SIO / HDD BDM / i.LINK / MMCE / UDPFS start+autostart,
 * Pair Bluetooth, Refresh connections ) are now in the translatable string table
 * ( SMS_Locale, indices 269+ ) via STR_* macros -- no longer hardcoded here.
 *
 * Every static row is placed by NAME ( [ DEVROW_x ] = ... ) and every handler and the status block
 * in _device_handler write a row's ON/OFF ( or text ) value through the same name, so the order is
 * defined once, in the DEVROW numbers. An earlier reorder ( 9985cc3 ) moved the rows but not the
 * hardcoded numbers, and toggles then painted the wrong lines. To reorder, change only the
 * numbers ( 0..DEVROW_LAST, each used once ) and run tools/check_devmenu.py: it fails when a
 * label, its handler, its flag and its repaint stop belonging together. */
#ifdef BDM
/* Grouped: storage autostarts; Network settings with the three network modes; CD/DVD; the two
 * controller rows. The Start rows are appended after these at run time ( _device_handler ). */
# define DEVROW_AUTO_USB     0
# define DEVROW_AUTO_ILINK   1
# define DEVROW_AUTO_MX4SIO  2
# define DEVROW_AUTO_MMCE    3
# define DEVROW_AUTO_HDD     4   /* HDD APA */
# define DEVROW_AUTO_ATA     5   /* HDD BDM */
# define DEVROW_NETWORK      6
/* The three NETWORK MODE rows are one choice: selecting any of them repaints all three
 * ( _switch_netmode ), so these are also needed away from their own handlers. */
# define DEVROW_HOST         7
# define DEVROW_SMB          8
# define DEVROW_UDPFS        9
# define DEVROW_CDVD        10
# define DEVROW_CDVD_SPEED  11
# define DEVROW_CNTSLOT     12
# define DEVROW_DIRBTN      13
# define DEVROW_LAST        13
static GUIMenuItem s_DevMenu[ 28 ] __attribute__(   (  section( ".data" )  )   ) = {
 [ DEVROW_AUTO_USB    ] = {                   0, &STR_AUTOSTART_USB,       0, 0, _autousb_handler,    0, 0 },
 [ DEVROW_AUTO_ILINK  ] = {                   0, &STR_AUTOSTART_ILINK,     0, 0, _autoilink_handler,  0, 0 },
 [ DEVROW_AUTO_MX4SIO ] = {                   0, &STR_AUTOSTART_MX4SIO,    0, 0, _automx4sio_handler, 0, 0 },
 [ DEVROW_AUTO_MMCE   ] = {                   0, &STR_AUTOSTART_MMCE,      0, 0, _automce_handler,    0, 0 },
 [ DEVROW_AUTO_HDD    ] = {                   0, &STR_AUTOSTART_HDD,       0, 0, _autohdd_handler,    0, 0 },
 [ DEVROW_AUTO_ATA    ] = {                   0, &STR_AUTOSTART_HDD_BDM,   0, 0, _autoata_handler,    0, 0 },
 [ DEVROW_NETWORK     ] = {                   0, &STR_NETWORK_SETTINGS,    0, 0, _network_handler,    0, 0 },
 [ DEVROW_HOST        ] = {                   0, &STR_AUTOSTART_HOST,      0, 0, _autohost_handler,   0, 0 },
 [ DEVROW_SMB         ] = {                   0, &STR_AUTOSTART_SMB,       0, 0, _autosmb_handler,    0, 0 },
 [ DEVROW_UDPFS       ] = {                   0, &STR_AUTOSTART_UDPFS,     0, 0, _autoudpfs_handler,  0, 0 },
 [ DEVROW_CDVD        ] = {                   0, &STR_DISABLE_CDVD,        0, 0, _cdvd_handler,       0, 0 },
 [ DEVROW_CDVD_SPEED  ] = { MENU_ITEM_TYPE_TEXT, &STR_CDVD_SPEED,          0, 0, _cdvd_spd_handler,   0, 0 },
 [ DEVROW_CNTSLOT     ] = { MENU_ITEM_TYPE_TEXT, &STR_CONTROLLER_SLOT2,    0, 0, _cntslot_handler,    0, 0 },
 [ DEVROW_DIRBTN      ] = { MENU_ITEM_TYPE_TEXT, &STR_DIRECTIONAL_BUTTONS, 0, 0, _dirbtn_handler,     0, 0 }
};
#else
# define DEVROW_NETWORK      0
# define DEVROW_CNTSLOT      1
# define DEVROW_AUTO_NET     2
# define DEVROW_AUTO_USB     3
# define DEVROW_AUTO_HDD     4
# define DEVROW_CDVD         5
# define DEVROW_CDVD_SPEED   6
# define DEVROW_DIRBTN       7
# define DEVROW_LAST         7
static GUIMenuItem s_DevMenu[ 12 ] __attribute__(   (  section( ".data" )  )   ) = {
 [ DEVROW_NETWORK     ] = {                   0, &STR_NETWORK_SETTINGS,    0, 0, _network_handler,  0, 0 },
 [ DEVROW_CNTSLOT     ] = { MENU_ITEM_TYPE_TEXT, &STR_CONTROLLER_SLOT2,    0, 0, _cntslot_handler,  0, 0 },
 [ DEVROW_AUTO_NET    ] = {                   0, &STR_AUTOSTART_NETWORK,   0, 0, _autonet_handler,  0, 0 },
 [ DEVROW_AUTO_USB    ] = {                   0, &STR_AUTOSTART_USB,       0, 0, _autousb_handler,  0, 0 },
 [ DEVROW_AUTO_HDD    ] = {                   0, &STR_AUTOSTART_HDD,       0, 0, _autohdd_handler,  0, 0 },
 [ DEVROW_CDVD        ] = {                   0, &STR_DISABLE_CDVD,        0, 0, _cdvd_handler,     0, 0 },
 [ DEVROW_CDVD_SPEED  ] = { MENU_ITEM_TYPE_TEXT, &STR_CDVD_SPEED,          0, 0, _cdvd_spd_handler, 0, 0 },
 [ DEVROW_DIRBTN      ] = { MENU_ITEM_TYPE_TEXT, &STR_DIRECTIONAL_BUTTONS, 0, 0, _dirbtn_handler,   0, 0 }
};
#endif

static GUIMenuItem s_IPCMenu[] __attribute__(   (  section( ".data" )  )   ) = {
 { MENU_ITEM_TYPE_TEXT, &STR_PS2_IP1,           0, 0, _ip1_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_PS2_IP2,           0, 0, _ip2_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_PS2_IP3,           0, 0, _ip3_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_PS2_IP4,           0, 0, _ip4_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_NETMASK1,          0, 0, _nm1_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_NETMASK2,          0, 0, _nm2_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_NETMASK3,          0, 0, _nm3_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_NETMASK4,          0, 0, _nm4_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_GATEWAY1,          0, 0, _gw1_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_GATEWAY2,          0, 0, _gw2_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_GATEWAY3,          0, 0, _gw3_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_GATEWAY4,          0, 0, _gw4_handler,     0, 0 },
 { 0,                   &STR_SAVE_IPCONFIG_DAT, 0, 0, _saveipc_handler, 0, 0 }
};

static GUIMenuItem s_BrowserMenu[] __attribute__(   (  section( ".data" )  )   ) = {
 { MENU_ITEM_TYPE_TEXT,   &STR_USE_BACKGROUND_IMAGE, 0, 0, _usebg_handler,  0, 0 },
 { 0,                     &STR_SOUND_FX,             0, 0, _sound_handler,  0, 0 },
 { 0,                     &STR_SORT_FS_OBJECTS,      0, 0, _sortfs_handler, 0, 0 },
 { 0,                     &STR_FILTER_MEDIA_FILES,   0, 0, _filter_handler, 0, 0 },
 { 0,                     &STR_DISPLAY_HDL_PART,     0, 0, _dsphdl_handler, 0, 0 },
 { 0,                     &STR_HIDE_SYSTEM_PART,     0, 0, _hidesp_handler, 0, 0 },
 { 0,                     &STR_USE_XH,               0, 0, _usexh_handler,  0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_ACTIVE_BORDER_CLR,    0, 0, _abclr_handler,  0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_INACTIVE_BORDER_CLR,  0, 0, _ibclr_handler,  0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_TEXT_CLR,             0, 0, _txtclr_handler, 0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_STATUS_LINE_TEXT_CLR, 0, 0, _sltxt_handler,  0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_SEL_BAR_CLR,          0, 0, _sclr_handler,   0, 0 },
 { MENU_ITEM_TYPE_TEXT,   &STR_RESET_BUTTON_ACTION,  0, 0, _pwrbtn_handler, 0, 0 },
 { MENU_ITEM_TYPE_TEXT,   &STR_EXIT_TO,              0, 0, _exit_2_handler, 0, 0 }

};

static GUIMenuItem s_PlayerMenu[] __attribute__(   (  section( ".data" )  )   ) = {
 { 0,                     &STR_MP3_SETTINGS,        0, 0, _mp3_handler,     0, 0 },
 { 0,                     &STR_AUTOLOAD_SUBTITLES,  0, 0, _autols_handler,  0, 0 },
 { 0,                     &STR_OPAQUE_SUBTITLES,    0, 0, _opaqs_handler,   0, 0 },
 { 0,                     &STR_DISPLAY_SB_TIME,     0, 0, _dsbt_handler,    0, 0 },
 { 0,                     &STR_SPDIF_DD,            0, 0, _spdif_handler,   0, 0 },
 { 0,                     &STR_PULLDOWN22,          0, 0, _pdw22_handler,   0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_SUBTITLE_COLOR,      0, 0, _subclr_handler,  0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_SUBTITLE_BOLD_COLOR, 0, 0, _subbclr_handler, 0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_SUBTITLE_ITL_COLOR,  0, 0, _subiclr_handler, 0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_SUBTITLE_UND_COLOR,  0, 0, _subu_handler,    0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_SCROLLBAR_COLOR,     0, 0, _sbclr_handler,   0, 0 },
 { MENU_ITEM_TYPE_PALIDX, &STR_VOLUME_BAR_COLOR,    0, 0, _vbclr_handler,   0, 0 },
 { MENU_ITEM_TYPE_TEXT,   &STR_COLOR_RESOLUTION,    0, 0, _clres_handler,   0, 0 },
 { MENU_ITEM_TYPE_TEXT,   &STR_DEFAULT_VOLUME,      0, 0, _vol_handler,     0, 0 },
 { MENU_ITEM_TYPE_TEXT,   &STR_SUBTITLE_ALIGNMENT,  0, 0, _salign_handler,  0, 0 },
 { MENU_ITEM_TYPE_TEXT,   &STR_AUTO_POWER_OFF,      0, 0, _poff_handler,    0, 0 },
 { MENU_ITEM_TYPE_TEXT,   &STR_SCROLLBAR_LENGTH,    0, 0, _sblen_handler,   0, 0 },
 { MENU_ITEM_TYPE_TEXT,   &STR_SCROLLBAR_POSITION,  0, 0, _sbpos_handler,   0, 0 },
 { MENU_ITEM_TYPE_TEXT,   &STR_SUBTITLE_OFFSET,     0, 0, _suboff_handler,  _enter_sample, _leave_sample },
 { MENU_ITEM_TYPE_TEXT,   &STR_SUB_FONT_HSIZE,      0, 0, _sfonth_handler,  _enter_sample, _leave_sample },
 { MENU_ITEM_TYPE_TEXT,   &STR_SUB_FONT_VSIZE,      0, 0, _sfontv_handler,  _enter_sample, _leave_sample },
 { MENU_ITEM_TYPE_TEXT,   &STR_SUBTITLE_MBCS,       0, 0, _smbcs_handler,   0, 0 }
};

static GUIMenuItem s_MP3Menu[] __attribute__(   (  section( ".data" )  )   ) = {
 { 0,                   &STR_AUDIO_ANIM_DISPLAY, 0, 0, _aadsp_handler,      0, 0 },
 { 0,                   &STR_RANDOMIZE_PLAYLIST, 0, 0, _mp3_rand_handler,   0, 0 },
 { 0,                   &STR_REPEAT_MODE,        0, 0, _mp3_repeat_handler, 0, 0 },
 { 0,                   &STR_AUDIO_SPECTRUM_DSP, 0, 0, _mp3_asd_handler,    0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_MP3_PARAMETER,      0, 0, _mp3_adp_handler,    0, 0 }
};

static GUIMenuItem s_HelpMenu[] __attribute__(   (  section( ".data" )  )   ) = {
 { 0, &STR_HELP_01, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_02, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_03, 0, 0, 0, 0, 0 },
 { 0, &STR_SPACE,   0, 0, 0, 0, 0 },
 { 0, &STR_HELP_04, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_05, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_06, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_07, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_08, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_09, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_10, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_11, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_12, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_13, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_14, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_15, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_16, 0, 0, 0, 0, 0 },
 { 0, &STR_SPACE,   0, 0, 0, 0, 0 },
 { 0, &STR_HELP_17, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_18, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_19, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_20, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_21, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_22, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_23, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_24, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_25, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_26, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_27, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_28, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_29, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_30, 0, 0, 0, 0, 0 },
 { 0, &STR_SPACE,   0, 0, 0, 0, 0 },
 { 0, &STR_HELP_31, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_32, 0, 0, 0, 0, 0 },
 { 0, &STR_HELP_33, 0, 0, 0, 0, 0 }
};

static GUIMenuItem s_NetMenu[ 6 ] __attribute__(   (  section( ".data" )  )   ) = {
 { 0,                   &STR_EDIT_IPCONFIG,    0, 0, _editipc_handler, 0, 0 },
 { MENU_ITEM_TYPE_TEXT, &STR_NETWORK_PROTOCOL, 0, 0, _netprot_handler, 0, 0 }
};

static void _Sample_Render ( GUIObject* apObj, int aCtx ) {

 if ( !apObj -> m_pGSPacket ) {

  int            lLen   = STR_SAMPLE.m_Len;
  int            lDWC   = GS_TXT_PACKET_SIZE( lLen );
  int            lWidth = GSFont_WidthEx ( STR_SAMPLE.m_pStr, lLen, g_Config.m_SubHIncr );
  u64*           lpDMA  = GSContext_NewList ( lDWC << 1 );

  g_GSCtx.m_TextColor = 0;
  GSFont_RenderEx (
   STR_SAMPLE.m_pStr, lLen, 0, g_GSCtx.m_Height - g_Config.m_PlayerSubOffset, lpDMA, g_Config.m_SubHIncr, g_Config.m_SubVIncr
  );
  GSFont_RenderEx (
   STR_SAMPLE.m_pStr, lLen, g_GSCtx.m_Width - lWidth - 96, g_GSCtx.m_Height - g_Config.m_PlayerSubOffset, lpDMA + lDWC, g_Config.m_SubHIncr, g_Config.m_SubVIncr
  );

  apObj -> m_pGSPacket = lpDMA;

 }  /* end if */

 GSContext_CallList ( aCtx, apObj -> m_pGSPacket );

}  /* end _Sample_Render */

static GUIObject* _create_sample ( void ) {

 _Sample* retVal = ( _Sample* )calloc (  1, sizeof ( _Sample )  );

 retVal -> Render  = _Sample_Render;
 retVal -> Cleanup = GUIObject_Cleanup;

 return ( GUIObject* )retVal;

}  /* end _create_sample */

static void _redraw_sample ( void ) {

 GSContext_NewPacket ( 1, 0, GSPaintMethod_Init );
  GSContext_CallList ( 1, s_pSample -> m_pGSPacket );
 GSContext_Flush ( 1, GSFlushMethod_KeepLists );

}  /* end _redraw_sample */

static void _setup_dimensions ( GUIMenu* apMenu ) {

 int lWidth  = g_GSCtx.m_Width  - ( g_GSCtx.m_Width  >> 2 );
 int lHeight = g_GSCtx.m_Height - ( g_GSCtx.m_Height >> 2 );

 lWidth += lWidth / 12;

 apMenu -> m_X      = ( g_GSCtx.m_Width  - lWidth  ) >> 1;
 apMenu -> m_Y      = (  ( g_GSCtx.m_Height - lHeight ) >> 1  ) + 8;
 apMenu -> m_Width  = lWidth;
 apMenu -> m_Height = lHeight;

}  /* end _setup_dimensions */

void GUIMenuSMS_UpdateStatus ( GUIMenu* apMenu ) {

 int           lIdx;
 char          lBuf[ 1024 ];
 SMS_ListNode* lpNode = apMenu -> m_pState -> m_pHead;
 GUIMenuState* lpState = ( GUIMenuState* )( unsigned int )lpNode -> m_Param;

 memset (  lBuf, 0, sizeof ( lBuf )  );
 strncpy ( lBuf, lpState -> m_pTitle -> m_pStr, lpState -> m_pTitle -> m_Len );
 lpNode = lpNode -> m_pNext;
 lIdx   = strlen ( lBuf );

 while ( lpNode ) {

  GUIMenuState* lpState = ( GUIMenuState* )( unsigned int )lpNode -> m_Param;

  lBuf[ lIdx++ ] = '/';
  strncpy ( &lBuf[ lIdx ], lpState -> m_pTitle -> m_pStr, lpState -> m_pTitle -> m_Len );

  lIdx  += lpState -> m_pTitle -> m_Len;
  lpNode = lpNode -> m_pNext;

 }  /* end while */

 GUI_Status ( lBuf );

}  /* end GUIMenuSMS_UpdateStatus */

extern int GUIMenu_HandleEvent ( GUIObject*, u64           );

int GUIMenuSMS_HandleEvent ( GUIObject* apObj, u64           anEvent ) {

 GUIMenu*      lpMenu  = ( GUIMenu* )apObj;
 GUIMenuState* lpState = (  ( GUIMenuState* )( unsigned int )lpMenu -> m_pState -> m_pTail -> m_Param  );

 switch ( anEvent & GUI_MSG_PAD_MASK ) {

  case RC_STOP         :
  case RC_RETURN       :
  case RC_RESET        :
  case SMS_PAD_TRIANGLE: {

   if ( lpState -> m_pCurr -> Leave ) lpState -> m_pCurr -> Leave ( lpMenu );

   if ( lpState -> HandleEvent ) apObj -> HandleEvent = lpState -> HandleEvent;

   if ( !GUI_MenuPopState ( lpMenu )  ) {

    void* lpActive = lpMenu -> m_pActiveObj;

    GUI_DeleteObject ( g_SMSMenuStr );

    g_pActiveNode = lpActive;

    GUI_Redraw ( GUIRedrawMethod_Redraw );
    GUI_UpdateStatus ();

    if ( s_fHDD ) GUI_PostMessage ( GUI_MSG_MOUNT_BIT | GUI_MSG_HDD );

   } else {

    GUIMenuSMS_UpdateStatus ( lpMenu );
    lpMenu -> Redraw ( lpMenu );

   }  /* end else */

  } return GUIHResult_Handled;

 }  /* end switch */

 return GUIMenu_HandleEvent ( apObj, anEvent );

}  /* end GUIMenuSMS_HandleEvent */

void GUIMenuSMS_Redraw ( GUIMenu* apMenu ) {

 if ( apMenu ) apMenu -> Cleanup (  ( GUIObject* )apMenu  );

 GUI_Redraw ( GUIRedrawMethod_Redraw );
 SMS_GUIClockRedraw ();

 if ( apMenu ) GUIMenuSMS_UpdateStatus ( apMenu );

}  /* end GUIMenuSMS_Redraw */

static SMString* s_Charsets[ 4 ] __attribute__(   (  section( ".data" )  )   ) = {
 &STR_WINLATIN2, &STR_WINCYRILLIC, &STR_WINLATIN1, &STR_WINGREEK
};

static SMString* s_VModeName[ 9 ] __attribute__(   (  section( ".data" )  )   ) = {
 &STR_NTSC,     &STR_PAL,       &STR_DTV_480P,   &STR_DTV_576P,
 &STR_DTV_720P, &STR_DTV_1080I, &STR_VESA_60_HZ, &STR_VESA_75_HZ,
 &STR_AUTO
};

static void _update_display_menu ( void ) {

 SMString* lpStr = s_VModeName[ GS_VMode2Index ( g_Config.m_DisplayMode ) ];

 s_DispMenu[ 0 ].m_IconRight = ( unsigned int )lpStr;
 s_DispMenu[ 1 ].m_IconRight = ( unsigned int )s_Charsets[ g_Config.m_DisplayCharset ];

 if (  strcmp ( g_Config.m_Language, g_pDefStr ) && ( g_Config.m_BrowserFlags & SMS_BF_UDFL )  )
  lpStr = &STR_USER_DEFINED;
 else lpStr = &STR_DEFAULT;

 s_DispMenu[ 2 ].m_IconRight = ( unsigned int )lpStr;

}  /* end _update_display_menu */

static void _display_handler ( GUIMenu* apMenu, int aDir ) {

 GUIMenuState* lpState = GUI_MenuPushState ( apMenu );

 _update_display_menu ();

 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_DispMenu;
 lpState -> m_pLast  = &s_DispMenu[ sizeof ( s_DispMenu ) / sizeof ( s_DispMenu[ 0 ] ) - 1 ];
 lpState -> m_pTitle = &STR_DISPLAY_SETTINGS1;

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _display_handler */

static SMString* s_Speeds[ 3 ] = {
 &STR_LOW, &STR_MEDIUM, &STR_HIGH
};

static void _device_handler ( GUIMenu* apMenu, int aDir ) {

 GUIMenuState* lpState = GUI_MenuPushState ( apMenu );
 unsigned int  lSize   = DEVROW_LAST;   /* static rows 0..DEVROW_LAST; the Start rows append after */

 if ( g_Config.m_NetworkFlags & SMS_DF_GAMEPAD )
  s_DevMenu[ DEVROW_CNTSLOT ].m_IconRight = ( unsigned int )&STR_GAMEPAD;
 else if (  ( g_IOPFlags & SMS_IOPF_RMMAN ) && ( g_Config.m_NetworkFlags & SMS_DF_REMOTE )  )
  s_DevMenu[ DEVROW_CNTSLOT ].m_IconRight = ( unsigned int )&STR_REMOTE_CONTROL;
 else s_DevMenu[ DEVROW_CNTSLOT ].m_IconRight = ( unsigned int )&STR_NONE;

/* Every row is painted through its DEVROW name, so this block does not depend on the order.
 * tools/check_devmenu.py checks each one against the row's label and handler. */
 s_DevMenu[ DEVROW_AUTO_USB    ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_AUTO_USB    ? GUICON_ON : GUICON_OFF;
 s_DevMenu[ DEVROW_AUTO_HDD    ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_AUTO_HDD    ? GUICON_ON : GUICON_OFF;  /* HDD APA */
#ifdef BDM
 s_DevMenu[ DEVROW_AUTO_ILINK  ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_AUTO_ILINK  ? GUICON_ON : GUICON_OFF;
 s_DevMenu[ DEVROW_AUTO_MX4SIO ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_AUTO_MX4SIO ? GUICON_ON : GUICON_OFF;
 s_DevMenu[ DEVROW_AUTO_MMCE   ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_AUTO_MMCE   ? GUICON_ON : GUICON_OFF;
 s_DevMenu[ DEVROW_AUTO_ATA    ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_AUTO_ATA    ? GUICON_ON : GUICON_OFF;  /* HDD BDM */
/* The three network rows are ONE choice, so they are painted from the single derived mode
 * rather than from three independent bits. That is what makes an inherited two-stacks-at-once
 * config ( which older builds could write ) display honestly instead of lighting two rows. */
 s_DevMenu[ DEVROW_HOST        ].m_IconRight = SMS_ConfigNetMode () == SMS_NETMODE_HOST  ? GUICON_ON : GUICON_OFF;
 s_DevMenu[ DEVROW_SMB         ].m_IconRight = SMS_ConfigNetMode () == SMS_NETMODE_SMB   ? GUICON_ON : GUICON_OFF;
 s_DevMenu[ DEVROW_UDPFS       ].m_IconRight = SMS_ConfigNetMode () == SMS_NETMODE_UDPFS ? GUICON_ON : GUICON_OFF;
#else
 s_DevMenu[ DEVROW_AUTO_NET    ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_AUTO_NET    ? GUICON_ON : GUICON_OFF;
#endif
 s_DevMenu[ DEVROW_CDVD        ].m_IconRight = g_Config.m_NetworkFlags & SMS_DF_CDVD        ? GUICON_ON : GUICON_OFF;
 s_DevMenu[ DEVROW_CDVD_SPEED  ].m_IconRight = ( unsigned int )s_Speeds[ g_Config.m_CDVDSpeed ];
 s_DevMenu[ DEVROW_DIRBTN      ].m_IconRight = ( unsigned int )( g_Config.m_BrowserFlags & SMS_BF_DIRB ? &STR_REMOTE_CONTROL : &STR_GAMEPAD );

/* START <device> rows, ordered the way Nad asked for ( item 3 of his batch ): the storage
 * devices first, in the order people actually reach for them, and the network stacks last.
 * Matches the autostart block above, so the two lists finally read the same way down.
 *
 * The DEV9 test used to WRAP the network rows and Start HDD APA together, which pinned all
 * three to wherever that one block sat -- reordering was impossible without moving rows that
 * have nothing to do with each other. It is applied PER ROW now, so each row sits on merit.
 * Every gating condition is otherwise byte-for-byte what it was; only placement changed.
 *
 * Safe to reorder, unlike the static block above: these rows are appended with ++lSize and
 * deliberately carry no m_IconRight, so not one of them owns a row number. The static rows
 * above are written through their DEVROW names instead; the first reorder, before those names,
 * missed _switch_flag's numbers and painted indicators on the wrong rows.
 * tools/check_devmenu.py verifies both lists; run it after touching either. */

 if (  !( g_IOPFlags & SMS_IOPF_UMS )  ) {   /* UMS ( usb-mass ) not USB ( usbd ): ds34usb now loads usbd alone at boot, so keep offering "Start USB" until the MASS stack is actually up */

  s_DevMenu[ ++lSize ].m_pOptionName = &STR_START_USB_NOW;
  s_DevMenu[   lSize ].Handler       = _startusb_handler;

 }  /* end if */

#ifdef BDM
 if (  !( g_IOPFlags & SMS_IOPF_ILINK )  ) {

  s_DevMenu[ ++lSize ].m_pOptionName = &STR_START_ILINK;
  s_DevMenu[   lSize ].Handler       = _startilink_handler;

 }  /* end if */

/* MX4SIO and MMCE drive the SAME SIO2 bus and neither module can be unloaded, so the
 * first to start owns it for the boot ( SMS_IOPStartMX4SIO / SMS_IOPStartMMCE each
 * refuse when the other holds it ). Kept adjacent so that shared constraint is visible.
 * ALWAYS EMITTED ( both of them ). These two used to be hidden whenever EITHER was up,
 * which is how "there is no Start MMCE option" gets reported: an MMCE that is already
 * running takes its own row away, and so does an MX4SIO that has taken the bus. A row
 * that silently vanishes is indistinguishable from a build that forgot to include it,
 * and it leaves no way to see WHY the device is unavailable.
 * Safe to always offer: both starters are idempotent and self-guarding --
 * SMS_IOPStartMX4SIO returns success immediately if MX4SIO is already up and refuses
 * cleanly if MMCE holds SIO2 ( SMS_IOP.c ), and SMS_IOPStartMMCE mirrors it exactly. So
 * pressing an already-started device is a harmless no-op, and pressing the one whose bus
 * the sibling owns reports an error -- which is the honest answer, and strictly more
 * informative than the row not being there at all.
 * Deliberately NOT setting m_IconRight here: dynamic rows shift index as devices appear
 * and disappear, so a state icon written now would go stale on whatever row later lands
 * on this slot -- the same index-locking trap as the static block above. */
 if ( 1 ) {

  s_DevMenu[ ++lSize ].m_pOptionName = &STR_START_MX4SIO;
  s_DevMenu[   lSize ].Handler       = _startmx4sio_handler;

 }  /* end if */

 if ( 1 ) {   /* always emitted -- see the MX4SIO/MMCE note above ( shown even when MX4SIO owns the shared SIO2 bus ) */

  s_DevMenu[ ++lSize ].m_pOptionName = &STR_START_MMCE;
  s_DevMenu[   lSize ].Handler       = _startmce_handler;

 }  /* end if */
#endif

/* HDD APA. The DEV9 term was inherited from the block that used to wrap this row; it is
 * spelled out here so the row can stand on its own. */
 if (  ( g_IOPFlags & SMS_IOPF_DEV9_IS ) && !( g_IOPFlags & SMS_IOPF_HDD )  ) {

  s_DevMenu[ ++lSize ].m_pOptionName = &STR_START_HDD_NOW;
  s_DevMenu[   lSize ].Handler       = _starthdd_handler;

 }  /* end if */

#ifdef BDM
 if (  !( g_IOPFlags & SMS_IOPF_ATA ) && ( g_IOPFlags & SMS_IOPF_DEV9_IS )  ) {

  s_DevMenu[ ++lSize ].m_pOptionName = &STR_START_HDD_BDM;
  s_DevMenu[   lSize ].Handler       = _startata_handler;

 }  /* end if */

/* Gated on the DRIVE being mounted ( g_UdpfsFlags ), NOT on the modules being loaded
 * ( SMS_IOPF_UDPFS ): if the modules came up but discovery failed -- server not started
 * yet, or the PHY still negotiating -- this row MUST stay offered so the user can start
 * the server and retry. Keying it on the module flag hid the row on the first failure
 * and left no way back to udpfs for the whole boot. */
/* The s_NetOwner test ( via SMS_IOPNetOwnedBySMB ) makes this row's condition IDENTICAL
 * to SMS_IOPStartUDPFS's own guard. SMS_IOPStartNet claims the NIC BEFORE loading its
 * modules -- deliberately, so udpfs can never land on a half-built NIC -- so a Start
 * Network whose modules failed leaves the NIC owned with no SMS_IOPF_NET/SMB set.
 * Without this test the row would be offered and could only answer a bare "Error". */
 if (  !( g_UdpfsFlags & 2 ) && ( g_IOPFlags & SMS_IOPF_DEV9_IS ) &&
       !( g_IOPFlags & ( SMS_IOPF_NET | SMS_IOPF_SMB ) ) && !SMS_IOPNetOwnedBySMB ()  ) {   /* Ethernet present + NIC genuinely free -> udpfs ( mutually exclusive with SMB ) */

  s_DevMenu[ ++lSize ].m_pOptionName = &STR_START_UDPFS;
  s_DevMenu[   lSize ].Handler       = _startudpfs_handler;

 }  /* end if */
#endif

/* The network stacks go last, per Nad. The accessor term covers the gap the flags can't:
 * udpfs claims the NIC ( s_NetOwner ) the moment its smap loads, BEFORE SMS_IOPF_UDPFS is
 * set -- so a partially-failed Start UDPFS ( smap up, later module failed ) leaves the NIC
 * owned with no flag to show for it, and these rows could then only answer a bare "Error"
 * ( SMS_IOPStartNet refuses an owned NIC ). Mirror of the same fix on the Start-UDPFS row. */
 if (  ( g_IOPFlags & SMS_IOPF_DEV9_IS ) &&
       !(  g_IOPFlags & ( SMS_IOPF_NET | SMS_IOPF_SMB | SMS_IOPF_UDPFS )  ) && !SMS_IOPNetOwnedByUDPFS ()  ) {   /* hide SMB/network start when udpfs owns the NIC */
/* Two explicit rows replace the old single "Start network support": SMS_IOPStartNet picks
 * SMB vs HOST from the SMS_DF_SMB config toggle, so a forced start just sets/clears that
 * bit before calling it ( same mutation the Network Settings protocol toggle performs --
 * deliberately persisted in g_Config, NOT saved, exactly like the toggle ).
 * Start SMB is BDM-only ( smbman is compiled in under #ifdef BDM -- in a non-BDM build the
 * flag would silently start HOST anyway ) and gated on SMS_IOPF_SMBINFO ( a server is
 * configured -- same gate _netprot_handler uses ), otherwise the row could only fail. */
#ifdef BDM
  if ( g_IOPFlags & SMS_IOPF_SMBINFO ) {

   s_DevMenu[ ++lSize ].m_pOptionName = &STR_START_SMB;
   s_DevMenu[   lSize ].Handler       = _startsmb_handler;

  }  /* end if */
#endif

  s_DevMenu[ ++lSize ].m_pOptionName = &STR_START_HOST;
  s_DevMenu[   lSize ].Handler       = _starthost_handler;

 }  /* end if */

#ifdef BDM
 if (  g_IOPFlags & SMS_IOPF_DS34BT  ) {   /* Bluetooth driver up -> offer controller pairing */

  s_DevMenu[ ++lSize ].m_pOptionName = &STR_PAIR_BT;
  s_DevMenu[   lSize ].Handler       = _pairbt_handler;

 }  /* end if */
#endif

 s_DevMenu[ ++lSize ].m_pOptionName = &STR_REFRESH_CONN;   /* re-probe / reconnect active devices */
 s_DevMenu[   lSize ].Handler       = _refresh_handler;

 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_DevMenu;
 lpState -> m_pLast  = &s_DevMenu[ lSize ];
 lpState -> m_pTitle = &STR_DEVICE_SETTINGS1;

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _device_handler */

static SMString* s_ExitTo[ 3 ] = {
 &STR_BOOT_BROWSER, &STR_EXEC0, &STR_EXEC1
};

static void _update_skin ( void ) {

 static SMString sl_SkinName;

 if ( !g_Config.m_SkinName[ 0 ] ) {
  sl_SkinName.m_pStr = STR_NONE.m_pStr;
  sl_SkinName.m_Len  = STR_NONE.m_Len;
 } else {
  sl_SkinName.m_pStr = g_Config.m_SkinName;
  sl_SkinName.m_Len  = strlen ( g_Config.m_SkinName );
 }  /* end else */

 s_BrowserMenu[ 0 ].m_IconRight = ( unsigned int )&sl_SkinName;

}  /* end _update_skin */

static void _browser_handler ( GUIMenu* apMenu, int aDir ) {

 GUIMenuState* lpState = GUI_MenuPushState ( apMenu );

 _update_skin ();

 s_BrowserMenu[  1 ].m_IconRight = g_Config.m_BrowserFlags & SMS_BF_SDFX ? GUICON_ON : GUICON_OFF;
 s_BrowserMenu[  2 ].m_IconRight = g_Config.m_BrowserFlags & SMS_BF_SORT ? GUICON_ON : GUICON_OFF;
 s_BrowserMenu[  3 ].m_IconRight = g_Config.m_BrowserFlags & SMS_BF_AVIF ? GUICON_ON : GUICON_OFF;
 s_BrowserMenu[  4 ].m_IconRight = g_Config.m_BrowserFlags & SMS_BF_HDLP ? GUICON_ON : GUICON_OFF;
 s_BrowserMenu[  5 ].m_IconRight = g_Config.m_BrowserFlags & SMS_BF_SYSP ? GUICON_ON : GUICON_OFF;
 s_BrowserMenu[  6 ].m_IconRight = g_Config.m_BrowserFlags & SMS_BF_UXH  ? GUICON_ON : GUICON_OFF;
 s_BrowserMenu[  7 ].m_IconRight = ( unsigned int )&g_Config.m_BrowserABCIdx;
 s_BrowserMenu[  8 ].m_IconRight = ( unsigned int )&g_Config.m_BrowserIBCIdx;
 s_BrowserMenu[  9 ].m_IconRight = ( unsigned int )&g_Config.m_BrowserTxtIdx;
 s_BrowserMenu[ 10 ].m_IconRight = ( unsigned int )&g_Config.m_BrowserSBCIdx;
 s_BrowserMenu[ 11 ].m_IconRight = ( unsigned int )&g_Config.m_BrowserSCIdx;
 s_BrowserMenu[ 12 ].m_IconRight = ( unsigned int )(  ( g_Config.m_BrowserFlags & SMS_BF_EXIT ) ? &STR_EXIT : &STR_POWER_OFF  );
 s_BrowserMenu[ 13 ].m_IconRight = ( unsigned int )s_ExitTo[ g_Config.m_BrowserFlags >> 28 ];
 s_BrowserMenu[ 13 ].m_Type     |= (  GSFont_WidthEx ( STR_EXIT_TO.m_pStr, STR_EXIT_TO.m_Len, -7 ) + 16  ) << 24;

 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_BrowserMenu;
 lpState -> m_pLast  = &s_BrowserMenu[ sizeof ( s_BrowserMenu ) / sizeof ( s_BrowserMenu[ 0 ] ) - 1 ];
 lpState -> m_pTitle = &STR_BROWSER_SETTINGS1;

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _browser_handler */

static char s_VolumeBuffer[  5 ] __attribute__(   (  section( ".bss" )  )   );
static char s_OffsetBuffer[ 32 ] __attribute__(   (  section( ".bss" )  )   );
static char s_ScrollBuffer[  9 ] __attribute__(   (  section( ".bss" )  )   );
static char s_PowoffBuffer[ 32 ] __attribute__(   (  section( ".bss" )  )   );
static char s_SHSizeBuffer[ 32 ] __attribute__(   (  section( ".bss" )  )   );
static char s_SVSizeBuffer[ 32 ] __attribute__(   (  section( ".bss" )  )   );

SMString g_StrPlayer[ 6 ] = {
 { 0, s_VolumeBuffer },
 { 0, s_OffsetBuffer },
 { 0, s_ScrollBuffer },
 { 0, s_PowoffBuffer },
 { 0, s_SHSizeBuffer },
 { 0, s_SVSizeBuffer }
};

void _update_poff ( int anIncr ) {

 int lTime = ( int )g_Config.m_PowerOff;

 lTime += anIncr;

 if ( lTime < 0 ) {

  strcpy ( s_PowoffBuffer, STR_AUTO.m_pStr );
  lTime = -60000;

 } else if ( lTime == 0 ) {

  strcpy ( s_PowoffBuffer, STR_OFF.m_pStr );

 } else {

  if ( lTime > 5400000 ) lTime = 5400000;

  sprintf ( s_PowoffBuffer, STR_MIN_FORMAT.m_pStr, lTime / 60000 );

 }  /* end else */

 g_Config.m_PowerOff = lTime;

 g_StrPlayer[ 3 ].m_Len = strlen ( s_PowoffBuffer );

}  /* end _update_poff */

static void _update_pstr ( int anIncr ) {

 SMString* lpStr;

 _update_poff ( anIncr );

 switch ( g_Config.m_PlayerSAlign ) {

  default:
  case 0 : lpStr = &STR_CENTER;       break;
  case 1 : lpStr = &STR_LEFT;         break;
  case 2 : lpStr = &STR_RIGHT;        break;
  case 3 : lpStr = &STR_CENTER_RIGHT; break;

 }  /* end switch */

 s_PlayerMenu[ 14 ].m_IconRight = ( unsigned int )lpStr;
 s_PlayerMenu[ 15 ].m_IconRight = ( unsigned int )&g_StrPlayer[ 3 ];

 if ( g_Config.m_PlayerFlags & SMS_PF_C32 )
  lpStr = &STR_32_BIT;
 else if ( g_Config.m_PlayerFlags & SMS_PF_C16 )
  lpStr = &STR_16_BIT;
 else lpStr = &STR_AUTO;

 s_PlayerMenu[ 12 ].m_IconRight = ( unsigned int )lpStr;

 sprintf (    s_OffsetBuffer, "%d",                  g_Config.m_PlayerSubOffset                                                      );
 sprintf (    s_VolumeBuffer, "%d%%",                ( int )(   (  ( float )g_Config.m_PlayerVolume / 24.0F ) * 100.0F + 0.5F   )    );
 sprintf (    s_ScrollBuffer, STR_PTS_FORMAT.m_pStr, g_Config.m_ScrollBarNum                                                         );
 sprintf (    s_SHSizeBuffer, "%d",                  32 + g_Config.m_SubHIncr                                                        );
 sprintf (    s_SVSizeBuffer, "%d",                  32 + g_Config.m_SubVIncr                                                        );

 g_StrPlayer[ 0 ].m_Len = strlen ( s_VolumeBuffer );
 g_StrPlayer[ 1 ].m_Len = strlen ( s_OffsetBuffer );
 g_StrPlayer[ 2 ].m_Len = strlen ( s_ScrollBuffer );
 g_StrPlayer[ 4 ].m_Len = strlen ( s_SHSizeBuffer );
 g_StrPlayer[ 5 ].m_Len = strlen ( s_SVSizeBuffer );

 s_PlayerMenu[ 13 ].m_IconRight = ( unsigned int )&g_StrPlayer[ 0 ];
 s_PlayerMenu[ 18 ].m_IconRight = ( unsigned int )&g_StrPlayer[ 1 ];
 s_PlayerMenu[ 16 ].m_IconRight = ( unsigned int )&g_StrPlayer[ 2 ];
 s_PlayerMenu[ 19 ].m_IconRight = ( unsigned int )&g_StrPlayer[ 4 ];
 s_PlayerMenu[ 20 ].m_IconRight = ( unsigned int )&g_StrPlayer[ 5 ];

 switch ( g_Config.m_ScrollBarPos ) {

  case SMScrollBarPos_Top     : lpStr = &STR_TOP;    break;
  case SMScrollBarPos_Bottom  : lpStr = &STR_BOTTOM; break;
  case SMScrollBarPos_Inactive: lpStr = &STR_OFF;    break;

 }  /* end switch */

 s_PlayerMenu[ 17 ].m_IconRight = ( unsigned int )lpStr;

}  /* end _update_pstr */

static void _update_mbf ( void ) {

 static SMString sl_MBFName;

 if ( !g_Config.m_MBFName[ 0 ] ) {
  sl_MBFName.m_pStr = STR_NONE.m_pStr;
  sl_MBFName.m_Len  = STR_NONE.m_Len;
 } else {
  sl_MBFName.m_pStr = g_Config.m_MBFName;
  sl_MBFName.m_Len  = strlen ( g_Config.m_MBFName );
 }  /* end else */

 s_PlayerMenu[ 21 ].m_IconRight = ( unsigned int )&sl_MBFName;

}  /* end _update_mbf */

static void _player_handler ( GUIMenu* apMenu, int aDir ) {

 GUIMenuState*  lpState = GUI_MenuPushState ( apMenu );
 unsigned short lVideoMode = GS_Params () -> m_GSCRTMode;

 s_PlayerMenu[  1 ].m_IconRight = g_Config.m_PlayerFlags & SMS_PF_SUBS  ? GUICON_ON : GUICON_OFF;
 s_PlayerMenu[  2 ].m_IconRight = g_Config.m_PlayerFlags & SMS_PF_OSUB  ? GUICON_ON : GUICON_OFF;
 s_PlayerMenu[  3 ].m_IconRight = g_Config.m_PlayerFlags & SMS_PF_TIME  ? GUICON_ON : GUICON_OFF;
 s_PlayerMenu[  4 ].m_IconRight = g_Config.m_PlayerFlags & SMS_PF_SPDIF ? GUICON_ON : GUICON_OFF;
 s_PlayerMenu[  5 ].m_IconRight = g_Config.m_PlayerFlags & SMS_PF_PDW22 ? GUICON_ON : GUICON_OFF;
 s_PlayerMenu[  6 ].m_IconRight = ( unsigned int )&g_Config.m_PlayerSCNIdx;
 s_PlayerMenu[  7 ].m_IconRight = ( unsigned int )&g_Config.m_PlayerSCBIdx;
 s_PlayerMenu[  8 ].m_IconRight = ( unsigned int )&g_Config.m_PlayerSCIIdx;
 s_PlayerMenu[  9 ].m_IconRight = ( unsigned int )&g_Config.m_PlayerSCUIdx;
 s_PlayerMenu[ 10 ].m_IconRight = ( unsigned int )&g_Config.m_PlayerSBCIdx;
 s_PlayerMenu[ 11 ].m_IconRight = ( unsigned int )&g_Config.m_PlayerVBCIdx;

 if (  ( lVideoMode == GSVideoMode_PAL ) || ( lVideoMode == GSVideoMode_DTV_640x576P )  ) {
  s_PlayerMenu[ 5 ].m_Type &= ~MENU_ITEM_HIDDEN;
  if ( g_Config.m_PlayerFlags & SMS_PF_PDW22 ) s_PlayerMenu[ 4 ].m_Type |= MENU_ITEM_HIDDEN;
 } else {
  s_PlayerMenu[ 5 ].m_Type |=  MENU_ITEM_HIDDEN;
  s_PlayerMenu[ 4 ].m_Type &= ~MENU_ITEM_HIDDEN;
 }  /* end else */

 _update_pstr ( 0 );
 _update_mbf  ();

 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_PlayerMenu;
 lpState -> m_pLast  = &s_PlayerMenu[ sizeof ( s_PlayerMenu ) / sizeof ( s_PlayerMenu[ 0 ] ) - 1 ];
 lpState -> m_pTitle = &STR_PLAYER_SETTINGS1;

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _player_handler */

static void _help_handler ( GUIMenu* apMenu, int aDir ) {

 GUIMenuState* lpState = GUI_MenuPushState ( apMenu );

 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_HelpMenu;
 lpState -> m_pLast  = &s_HelpMenu[ sizeof ( s_HelpMenu ) / sizeof ( s_HelpMenu[ 0 ] ) - 1 ];
 lpState -> m_pTitle = &STR_QUICK_HELP;
 lpState -> m_Flags  = MENU_FLAGS_TEXT;

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _help_handler */

void _save_handler ( GUIMenu* apMenu, int aDir ) {

 GUI_Status ( STR_SAVING_CONFIGURATION.m_pStr );

 if (  !SMS_SaveConfig ()  ) GUI_Error ( g_SaveDiag[ 0 ] ? g_SaveDiag : STR_ERROR.m_pStr );   /* TEMP diag: name the failing stage */

 if ( apMenu )

  GUIMenuSMS_UpdateStatus ( apMenu );

 else GUI_UpdateStatus ();

}  /* end _save_handler */

void _shutdown_handler ( GUIMenu* apMenu, int aDir ) {

 SMS_IOPowerOff ();

}  /* end _shutdown_handler */
#ifdef EMBEDDED
void SMS_EExec ( char* );
__asm__(
 ".text\n\t"
 "SMS_EExec:\n\t"
 "ori   $v1, $zero, 252\n\t"
 "syscall\n\t"
);
#endif  /* EMBEDDED */
void _exit_handler ( GUIMenu* apMenu, int aDir ) {

 int  lIdx = g_Config.m_BrowserFlags >> 28;
 char lBuffer[ 1024 ] __attribute__(   (  aligned( 4 )  )   );

 SMS_TimerDestroy ();
 SPU_Shutdown     ();

 sprintf ( lBuffer, STR_LOADING.m_pStr, s_ExitTo[ lIdx ] -> m_pStr );
 GUI_Status ( lBuffer );   /* the owner's known landmark: confirms this is the diag build */

/* DIAGNOSTIC breadcrumbs -- see the note above SMS_ExitCrumb in SMS_IOP.c. Each names the
 * call ABOUT to run, so the last E-code left on screen is the one that wedges. E02 is
 * inside the g_PD guard on purpose: if E02 never appears, the PFS umount is PROVEN to be
 * skipped on a udpfs boot and that whole suspect is closed. */
 SMS_ExitCrumb ( 1, "pfs umount?" );

 if ( g_PD >= 0 ) {
  SMS_ExitCrumb ( 2, "pfs umount" );
  SMS_IOCtl ( g_pPFS, PFS_IOCTL_UMOUNT, NULL );
 }  /* end if */

 if ( !lIdx ) {

/* E11 FIX -- now UNCONDITIONAL. On the boot-browser exit the in-place SMS_IOPReset(1) is
 * DEAD WORK: nothing uses its SIF re-handshake or its reloaded IOP modules before Exit(0),
 * which returns to the PS2 browser -- and the browser re-inits the IOP itself.
 * wLaunchELF-R3Z's PS2Browser exit is likewise a bare Exit(0) with NO in-place reset
 * ( refs/wLaunchELF_R3Z/src/main_actions.c:415 ).
 *
 * WHY IT IS NO LONGER GATED ON DEV9. The first cut of this fix only skipped the reset when
 * dev9 was up, because a live smap was the only DMA source we had MEASURED hanging it, and
 * leaving every other exit byte-identical was the conservative call. A second hardware
 * report then landed with NO dev9 at all -- booted from USB with MMCE also mounted -- and it
 * hung in exactly the same place ( E11 = the post-reset SifInitRpc ). That closes the
 * question: the hazard was never network-specific.
 *
 * The mechanism is spelled out at the SifIopReset call in SMS_IOP.c: a non-dev9 exit reset
 * passes s_pUDNL, the LONG "rom0:UDNL rom0:EELOADCNF" IOP-kernel reload, and ANY IOP module
 * still DMAing into IOP RAM during that reload corrupts it, so the IOP never reaches
 * BOOTEND / never raises RPCINIT and the EE spins forever. That argument was written about
 * smap's inbound-frame RX, but nothing in it is specific to a NIC -- usbd/OHCI is a periodic
 * DMA engine too ( SOF + interrupt-endpoint processing runs on its own, whether or not SMS
 * asked for anything ), and so is any other bus master we leave hot. Gating on dev9 just
 * meant the next hot module found the same trap.
 *
 * Skipping the reset removes the trap for every device instead of one. The risk of doing it
 * everywhere is low and already evidenced: the dev9 path proved on hardware that a bare
 * Exit(0) survives handing the browser an IOP with a live DMA-driving module ( smap ), which
 * is the strictly harder case than a quiescent one. Devices are not left dirty either -- the
 * PFS umount above still runs, and the browser resets the IOP from ROM regardless.
 *
 * If a hang is ever reported here again, re-enable the breadcrumbs ( one no-op line in
 * SMS_ExitCrumb, SMS_IOP.c ): E26 latching now means the wedge is inside the browser's own
 * IOP reset rather than anything SMS does. */
  SMS_ExitCrumb ( 26, "Exit(0) noreset" );
#ifndef EMBEDDED
  Exit ( 0 );
#else
  SifExitRpc ();
  __asm__ __volatile__(
   "ori $v1, $zero, 252\n\t"
   "syscall\n\t"
  );
#endif  /* EMBEDDED */

 } else {
/* ⚠ NOT COVERED BY THE E11 FIX ABOVE. This is the non-default "Exit to -> EXEC0/EXEC1"
 * setting ( Browser settings, STR_EXIT_TO; default is 0 = boot browser, so reaching here
 * takes a deliberate user change that then persists in m_BrowserFlags ). SMS_EExec runs
 * SMS_IOPReset( 1 ) via inline asm ( SMS_EE.c:628 ) and so is still exposed to the same
 * hang -- and on a no-dev9 boot it takes the LONG s_pUDNL kernel-reload arm with none of
 * the dev9 mitigations, which is the worst variant.
 *
 * The boot-browser remedy does NOT transplant here: that reset is dead work before Exit(0),
 * whereas this one is real work -- ExecPS2 needs a freshly reset IOP to land in. A correct
 * fix means quiescing the bus masters ( usbd/OHCI, smap ) before the reset, which is
 * boot/exit-critical surgery; the tree also already records the empty-arg reset as a
 * measured FAILED fix for the dev9 case ( see SMS_IOP.c ). Deliberately deferred, not
 * overlooked. Users hitting a freeze on exit should leave "Exit to" at its default. */
#ifdef EMBEDDED
  SifExitRpc ();
#endif  /* EMBEDDED */
  SMS_EExec ( s_ExitTo[ lIdx ] -> m_pStr );
 }  /* end else */

}  /* end _exit_handler */

static void _reinitialize ( GUIMenu* apMenu ) {

 GUI_MenuPopState ( apMenu );

 _display_handler ( apMenu, 0 );

 apMenu -> Cleanup (  ( GUIObject* )apMenu  );
 GUI_Initialize ( 0 );
 _setup_dimensions ( apMenu );
 apMenu -> Redraw ( apMenu );
 SMS_GUIClockStop ();
 GUIMenuSMS_UpdateStatus ( apMenu );
 SMS_GUIClockStart ( &g_Clock );

}  /* end _reinitialize */

static void _tvsys_handler ( GUIMenu* apMenu, int aDir ) {

 static const unsigned char sl_DMode[ 9 ] __attribute__(   (  section( ".rodata" )  )   ) = {
  GSVideoMode_Default,        GSVideoMode_NTSC,
  GSVideoMode_PAL,            GSVideoMode_DTV_720x480P,
  GSVideoMode_DTV_640x576P,   GSVideoMode_DTV_1280x720P,
  GSVideoMode_DTV_1920x1080I, GSVideoMode_VESA_60Hz,
  GSVideoMode_VESA_75Hz
 };

 int i, lDispMode = g_Config.m_DisplayMode;

 for (  i = 0; i < sizeof ( sl_DMode ) / sizeof ( sl_DMode[ 0 ] ); ++i  ) if ( sl_DMode[ i ] == lDispMode ) break;

 i += aDir;

 if ( i < 0 )
  i = sizeof ( sl_DMode ) / sizeof ( sl_DMode[ 0 ] ) - 1;
 else if (  i == sizeof ( sl_DMode ) / sizeof ( sl_DMode[ 0 ] )  )
  i = 0;

 g_Config.m_DisplayMode = sl_DMode[ i ];

 _reinitialize ( apMenu );

}  /* end _tvsys_handler */

static void _charset_handler ( GUIMenu* apMenu, int aDir ) {

 switch ( g_Config.m_DisplayCharset ) {

  case GSCodePage_WinLatin2  : g_Config.m_DisplayCharset = GSCodePage_WinCyrillic; break;
  case GSCodePage_WinCyrillic: g_Config.m_DisplayCharset = GSCodePage_WinGreek;    break;
  default                    :
  case GSCodePage_WinGreek   : g_Config.m_DisplayCharset = GSCodePage_WinLatin1;   break;
  case GSCodePage_WinLatin1  : g_Config.m_DisplayCharset = GSCodePage_WinLatin2;   break;

 }  /* end switch */

 _update_display_menu ();

 g_GSCtx.m_CodePage = g_Config.m_DisplayCharset;
 GSFont_Init   ();
 SMS_IOPSetXLT ();

 GUI_Redraw ( GUIRedrawMethod_InitClearObj );
 GUIMenuSMS_UpdateStatus ( apMenu );

}  /* end _charset_handler */

/* Deliberately NOT localised, and deliberately not added to the string table.
 *
 * Not localised because it only ever appears when there is no translation to show it in.
 * Not in the table because that table is exact-count: it holds 311 entries and a pack with
 * any other number is rejected outright, so adding a 312th string here would invalidate all
 * 20 shipped packs and every pack a user already has. */
static char s_pNoLng[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) =
 "No language file loaded.\n\nCopy SMS.lng to mc%d:/SMS/ - that exact name, capitals "
 "included - or highlight a .lng file in the browser and use the file menu to install it.";
static char s_pBadLng[] __attribute__(   (  section( ".data" ), aligned( 1 )  )   ) =
 "The language file was found but has %d lines; this version needs %d.\n\nIt is from a "
 "different SMS release. Use the SMS.lng that came with this build.";

static void _lang_handler ( GUIMenu* apMenu, int aDir ) {

/* Say WHY nothing is happening. Without this the row is simply inert: a tester copied a pack
 * to the wrong folder, pressed X repeatedly, and got no change and no explanation -- there
 * was no way to tell "wrong place" from "wrong file" from "feature broken". */
 if (  !( g_Config.m_BrowserFlags & SMS_BF_UDFL )  ) {

  char lBuff[ 256 ];

  if ( g_LngLines > 0 )
   sprintf ( lBuff, s_pBadLng, g_LngLines, g_LngExpect );
  else sprintf ( lBuff, s_pNoLng, g_MCSlot );

  GUI_Error ( lBuff );

  return;

 }  /* end if */

 if ( g_Config.m_BrowserFlags & SMS_BF_UDFL ) {

  if (  !strcmp ( g_Config.m_Language, g_pDefStr )  )

   g_Config.m_Language[ 0 ] = '\x00';

  else strcpy ( g_Config.m_Language, g_pDefStr );

  SMS_LocaleSet ();

  _update_display_menu ();
  GUI_Redraw ( GUIRedrawMethod_InitClearObj );
  GUIMenuSMS_UpdateStatus ( apMenu );

 }  /* end if */

}  /* end _lang_handler */

extern void _set_dx_dy ( int**, int** );

void _check_dc_offset ( void ) {

 int* lpDX;
 int* lpDY;

 _set_dx_dy ( &lpDX, &lpDY );

 g_GSCtx.m_OffsetX = SMS_clip ( g_GSCtx.m_OffsetX, -160, 160 );
 g_GSCtx.m_OffsetY = SMS_clip ( g_GSCtx.m_OffsetY,  -64,  64 );

 *lpDX = g_GSCtx.m_OffsetX;
 *lpDY = g_GSCtx.m_OffsetY;

}  /* end _check_dc_offset */

static void _init_set_dc ( void ) {

 int lMode       = g_Config.m_DisplayMode;
 int lHDI        = lMode == GSVideoMode_DTV_1920x1080I;
 int lColorDepth = g_Config.m_ColorDepth || lHDI ? GSPixelFormat_PSMCT16
                                                 : GSPixelFormat_PSMCT24;
 int lDX, lDY;

 if ( lMode == GSVideoMode_Default ) lMode = g_pBXDATASYS[ 6 ] == 'E' ? GSVideoMode_PAL : GSVideoMode_NTSC;

 _check_dc_offset ();

/* Same derived offsets GSContext_Init uses. This is the live "Adjust image" path, so without
 * it every nudge of the D-pad would drop the 720p centring and snap the canvas back to the top
 * left, making the setting impossible to use in that mode. */
 GS_DisplayOffsets ( lMode, &lDX, &lDY );

 GS_VSync ();
 GS_InitDC ( &g_GSCtx.m_DispCtx, lColorDepth, g_GSCtx.m_PWidth, g_GSCtx.m_PHeight, lDX, lDY );
 GS_SetDC (  &g_GSCtx.m_DispCtx, ( lMode <= GSVideoMode_PAL ) || lHDI  );

}  /* end _init_set_dc */

void _adjleft_handler ( GUIMenu* apMenu, int aDir ) {

 int* lpDX;
 int* lpDY;

 _set_dx_dy ( &lpDX, &lpDY );

 g_GSCtx.m_OffsetX = *lpDX -= 1;
 _init_set_dc ();

}  /* end _adjleft_handler */

void _adjright_handler ( GUIMenu* apMenu, int aDir ) {

 int* lpDX;
 int* lpDY;

 _set_dx_dy ( &lpDX, &lpDY );

 g_GSCtx.m_OffsetX = *lpDX += 1;
 _init_set_dc ();

}  /* end _adjright_handler */

void _adjup_handler ( GUIMenu* apMenu, int aDir ) {

 int* lpDX;
 int* lpDY;

 _set_dx_dy ( &lpDX, &lpDY );

 *lpDY += *lpDY & 1;

 g_GSCtx.m_OffsetY = *lpDY -= 2;
 _init_set_dc ();

}  /* end _adjup_handler */

void _adjdown_handler ( GUIMenu* apMenu, int aDir ) {

 int* lpDX;
 int* lpDY;

 _set_dx_dy ( &lpDX, &lpDY );

 *lpDY += *lpDY & 1;

 g_GSCtx.m_OffsetY = *lpDY += 2;
 _init_set_dc ();

}  /* end _adjdown_handler */

static void _switch_flag ( GUIMenu* apMenu, int anIndex, unsigned int* apFlag, unsigned int aFlag ) {

 unsigned int  lIconIdx;
 GUIMenuState* lpState = ( GUIMenuState* )( unsigned int )apMenu -> m_pState -> m_pTail -> m_Param;

 if ( *apFlag & aFlag ) {

  *apFlag &= ~aFlag;
  lIconIdx = GUICON_OFF;

 } else {

  *apFlag |= aFlag;
  lIconIdx = GUICON_ON;

 }  /* end else */

 lpState -> m_pItems[ anIndex ].m_IconRight = lIconIdx;
 apMenu -> Redraw ( apMenu );

}  /* end _switch_flag */

extern int GUI_ReadButtons0 ( void );
extern int GUI_ReadButtons1 ( void );
extern int GUI_ReadButtons2 ( void );

extern unsigned char g_PadBuf1[ 256 ] __attribute__(   (  aligned( 64 ), section( ".data"  )  )   );

static void _cntslot_handler ( GUIMenu* apMenu, int aDir ) {

 unsigned int lStr;

 if ( g_Config.m_NetworkFlags & SMS_DF_GAMEPAD ) {

  g_Config.m_NetworkFlags &= ~SMS_DF_GAMEPAD;
  PAD_ClosePort ( 1, 0 );

  if ( g_IOPFlags & SMS_IOPF_RMMAN ) {

   RC_Open ( 1 );
   GUI_ReadButtons = GUI_ReadButtons0;
   g_Config.m_NetworkFlags |=  SMS_DF_REMOTE;
   lStr                     = ( unsigned int )&STR_REMOTE_CONTROL;

  } else {
setNone:
   if ( g_IOPFlags & SMS_IOPF_RMMAN2 ) {

    RCX_Open ();
    GUI_ReadButtons = GUI_ReadButtons0;

   } else GUI_ReadButtons = GUI_ReadButtons2;

   lStr = ( unsigned int )&STR_NONE;

  }  /* end else */

 } else if ( g_Config.m_NetworkFlags & SMS_DF_REMOTE ) {

  g_Config.m_NetworkFlags &= ~SMS_DF_REMOTE;
  goto setNone;

 } else {

  if ( g_IOPFlags & SMS_IOPF_RMMAN2 )
   RCX_Close ();
  else if ( g_IOPFlags & SMS_IOPF_RMMAN ) RC_Close ( 1 );

  PAD_OpenPort ( 1, 0, g_PadBuf1 );
  GUI_ReadButtons = GUI_ReadButtons1;

  g_Config.m_NetworkFlags |= SMS_DF_GAMEPAD;
  lStr                     = ( unsigned int )&STR_GAMEPAD;

 }  /* end else */

 s_DevMenu[ DEVROW_CNTSLOT ].m_IconRight = lStr;

 apMenu -> Redraw ( apMenu );

}  /* end _cntslot_handler */

#ifdef BDM
/* Select one of the three network modes, or switch it off by re-picking the lit one.
 *
 * This cannot use _switch_flag: that toggles ONE bit and repaints ONE row, whereas picking a
 * network mode necessarily changes up to three rows at once, because turning HOST on has to
 * turn SMB and UDPFS off. The console can only ever run one network stack per boot -- one
 * NIC, ownership claimed before any module loads, and nothing in the tree can unload an IOP
 * module -- so presenting these as three independent toggles would let the user express a
 * state the hardware cannot enter. SMS_ConfigSetNetMode enforces that, and repainting all
 * three rows here is what makes the menu tell the truth about it.
 *
 * Re-selecting the active mode clears it. Without that there would be no way back to "no
 * networking" once any mode had been picked. */
static void _switch_netmode ( GUIMenu* apMenu, unsigned int aMode ) {

 GUIMenuState* lpState = ( GUIMenuState* )( unsigned int )apMenu -> m_pState -> m_pTail -> m_Param;
 unsigned int  lMode;

 SMS_ConfigSetNetMode (  SMS_ConfigNetMode () == aMode ? SMS_NETMODE_OFF : aMode  );

 lMode = SMS_ConfigNetMode ();

 lpState -> m_pItems[ DEVROW_HOST  ].m_IconRight = lMode == SMS_NETMODE_HOST  ? GUICON_ON : GUICON_OFF;
 lpState -> m_pItems[ DEVROW_SMB   ].m_IconRight = lMode == SMS_NETMODE_SMB   ? GUICON_ON : GUICON_OFF;
 lpState -> m_pItems[ DEVROW_UDPFS ].m_IconRight = lMode == SMS_NETMODE_UDPFS ? GUICON_ON : GUICON_OFF;

 apMenu -> Redraw ( apMenu );

/* If a DIFFERENT stack is already running, offer to apply the change now. Only then: with
 * nothing up, the setting is a plain autostart preference for the next launch and a reset
 * would cost the user their mounted devices to achieve nothing. Re-selecting the live mode
 * is likewise a no-op. This mirrors R3Z's switchNetworkStack, which resets only when the
 * live stack differs from the requested one.
 * Declining is safe and leaves the choice saved -- it simply takes effect on next launch --
 * so the prompt can never trap the user in a state they cannot get out of. */
 if (   SMS_IOPNetLiveMode () != SMS_NETMODE_OFF && SMS_IOPNetLiveMode () != lMode   ) {

  if (  GUI_Question ( STR_NET_SWITCH_ASK.m_pStr )  ) {

   SMS_IOPNetSwitch ( lMode );
   GUI_Redraw ( GUIRedrawMethod_Redraw );

  }  /* end if */

 }  /* end if */

}  /* end _switch_netmode */

static void _autohost_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_netmode ( apMenu, SMS_NETMODE_HOST );

}  /* end _autohost_handler */

static void _autosmb_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_netmode ( apMenu, SMS_NETMODE_SMB );

}  /* end _autosmb_handler */
#endif

#ifndef BDM
static void _autonet_handler ( GUIMenu* apMenu, int aDir ) {

/* non-BDM only: that build has no SMB ( SMS_IOPStartNet compiles it out ) and no UDPFS, so
 * its single "Autostart network" row IS the HOST mode and a plain bit toggle is correct.
 * The BDM build reaches HOST through _autohost_handler above. */
 _switch_flag ( apMenu, DEVROW_AUTO_NET, &g_Config.m_NetworkFlags, SMS_DF_AUTO_NET );

}  /* end _autonet_handler */
#endif

static void _autousb_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, DEVROW_AUTO_USB, &g_Config.m_NetworkFlags, SMS_DF_AUTO_USB );

}  /* end _autousb_handler */

#ifdef BDM
static void _automx4sio_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, DEVROW_AUTO_MX4SIO, &g_Config.m_NetworkFlags, SMS_DF_AUTO_MX4SIO );

}  /* end _automx4sio_handler */

static void _autoata_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, DEVROW_AUTO_ATA, &g_Config.m_NetworkFlags, SMS_DF_AUTO_ATA );   /* HDD BDM */

}  /* end _autoata_handler */

static void _autoilink_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, DEVROW_AUTO_ILINK, &g_Config.m_NetworkFlags, SMS_DF_AUTO_ILINK );

}  /* end _autoilink_handler */

static void _automce_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, DEVROW_AUTO_MMCE, &g_Config.m_NetworkFlags, SMS_DF_AUTO_MMCE );

}  /* end _automce_handler */

static void _autoudpfs_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_netmode ( apMenu, SMS_NETMODE_UDPFS );   /* one of the three mutually exclusive network modes */

}  /* end _autoudpfs_handler */
#endif

static void _autohdd_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, DEVROW_AUTO_HDD, &g_Config.m_NetworkFlags, SMS_DF_AUTO_HDD );  /* HDD APA */

}  /* end _autohdd_handler */

static int _start_device (  GUIMenu* apMenu, int ( *Start ) ( int )  ) {

 int retVal;

/* Rate-limit device starts to ONE per second. Device starts are heavyweight IOP
 * operations ( MMCE alone does a ~1-2 s blind busy-wait with no on-screen feedback ),
 * and SMS's software auto-repeat keeps re-firing CROSS while it is held -- so a user
 * mashing or holding the button stacked multiple start attempts back-to-back, each
 * re-entering the module load path while the previous one was still settling. A held
 * press now collapses to a single attempt per second; the extra firings are silently
 * swallowed ( no error, no menu rebuild ). g_Timer is milliseconds. */
 {
  static u64 s_LastStart = 0;

  if ( g_Timer - s_LastStart < 1000 ) return 1;

  s_LastStart = g_Timer;
 }

 if (   !(  retVal = Start ( 1 )  )   ) {

  GUI_Error ( STR_ERROR.m_pStr );
  GUIMenuSMS_UpdateStatus ( apMenu );

 } else {

/* Swallow the still-held confirm before the menu is rebuilt. _device_handler
 * parks the cursor on the first row, and SMS's software auto-repeat
 * ( QueryPad0 -> QueryPad1 ) would otherwise stream the held CROSS into it: when
 * Network Settings was the first row that ran straight down Edit IP config -> the
 * IP octet editor and rolled octet 1 ( 0..255 ), pinning the user there -- the
 * "starting MMCE dumps me in the network octet loop" report. Now it would keep
 * toggling the first autostart. MMCE is the worst offender because its
 * start does a ~1-2 s blind busy-wait ( SMS_IOP.c ) with no on-screen feedback,
 * so the user keeps holding X. Same drain idiom the rest of the GUI already
 * uses ( e.g. SMS_GUISMBrowser "swallow the press that opened us" ), bounded on
 * g_Timer ( milliseconds ) so a genuinely stuck pad can never hang the wait. */
  {
   u64 lEnd = g_Timer + 3000;
   while (  GUI_ReadButtons () && g_Timer < lEnd  );
  }
  GUI_MenuPopState ( apMenu );
  _device_handler ( apMenu, 0 );

 }  /* end else */

 return retVal;

}  /* end _start_device */

#ifdef BDM
static void _startsmb_handler ( GUIMenu* apMenu, int aDir ) {

/* Force the SMB branch of SMS_IOPStartNet ( it dispatches on SMS_DF_SMB ) regardless of
 * the Network Settings toggle. The mutation persists in g_Config on purpose -- same
 * behavior as toggling the protocol in Network Settings; config is NOT saved here. */
 g_Config.m_NetworkFlags |= SMS_DF_SMB;
 _start_device ( apMenu, SMS_IOPStartNet );

}  /* end _startsmb_handler */
#endif

static void _starthost_handler ( GUIMenu* apMenu, int aDir ) {

/* Mirror of _startsmb_handler: force the HOST ( ps2dev ) branch of SMS_IOPStartNet. */
 g_Config.m_NetworkFlags &= ~SMS_DF_SMB;
 _start_device ( apMenu, SMS_IOPStartNet );

}  /* end _starthost_handler */

static void _startusb_handler ( GUIMenu* apMenu, int aDir ) {

 _start_device ( apMenu, SMS_IOPStartUSB );

}  /* end _startusb_handler */

#ifdef BDM
static void _startmx4sio_handler ( GUIMenu* apMenu, int aDir ) {

 _start_device ( apMenu, SMS_IOPStartMX4SIO );

}  /* end _startmx4sio_handler */

static void _startata_handler ( GUIMenu* apMenu, int aDir ) {

 _start_device ( apMenu, SMS_IOPStartATA );

}  /* end _startata_handler */

static void _startilink_handler ( GUIMenu* apMenu, int aDir ) {

 _start_device ( apMenu, SMS_IOPStartILINK );

}  /* end _startilink_handler */

static void _startmce_handler ( GUIMenu* apMenu, int aDir ) {

 _start_device ( apMenu, SMS_IOPStartMMCE );

}  /* end _startmce_handler */

static void _startudpfs_handler ( GUIMenu* apMenu, int aDir ) {

 _start_device ( apMenu, SMS_IOPStartUDPFS );

}  /* end _startudpfs_handler */

/* Pair a DS3/DS4 to the USB Bluetooth dongle: write the dongle's MAC into the
 * controller that is currently plugged in via USB ( functionally identical to
 * sixpair / OPL's PAIR ). Afterwards the user unplugs USB and turns the controller
 * on -- it connects to the dongle wirelessly and the ds34bt poller picks it up.
 * Reuses the RPCs already bound at boot ( SMS_PadDS34Init ). */
static void _pairbt_handler ( GUIMenu* apMenu, int aDir ) {

 unsigned char lDongle[ 6 ], lPad[ 6 ];
 int           lDongleOK, lPadOK, lSetOK;

 /* Hold the ds34 RPC lock across the whole read/write sequence: the input poller is
  * busy on the SAME ds34usb client while the controller is plugged in for pairing,
  * and two threads on one non-reentrant SIF RPC client would corrupt each other. */
 SMS_PadDS34Lock ();
  lDongleOK = ds34bt_get_bdaddr ( lDongle );
  lPadOK    = lDongleOK ? ds34usb_get_bdaddr ( 0, lPad )    : 0;   /* also confirms a controller is plugged in by USB */
  lSetOK    = lPadOK    ? ds34usb_set_bdaddr ( 0, lDongle ) : 0;
 SMS_PadDS34Unlock ();

 if      ( !lDongleOK ) GUI_Error  ( "No Bluetooth dongle detected" );
 else if ( !lPadOK    ) GUI_Error  ( "Plug the controller in by USB, then pair" );
 else if ( !lSetOK    ) GUI_Error  ( "Pairing failed" );
 else                   GUI_Status ( "Paired. Unplug USB and turn the controller on to connect." );

}  /* end _pairbt_handler */
#endif

static void _starthdd_handler ( GUIMenu* apMenu, int aDir ) {

 _start_device ( apMenu, SMS_IOPStartHDD );

 if ( g_IOPFlags & SMS_IOPF_HDD ) s_fHDD = 1;

}  /* end _starthdd_handler */

static void _refresh_handler ( GUIMenu* apMenu, int aDir ) {

/* Re-probe / reconnect the devices that are already up, without a reboot:
 * re-scan the BDM mass slots ( USB / MX4SIO ) for hot-swapped drives, then
 * reconnect the network -- retry the SMB log-on a few times if it is active,
 * since a dropped session / brief link loss just needs a fresh connect over the
 * ( still-loaded ) TCP stack. On repeated failure the SMB error code tells you
 * whether it is the link ( CONN ) or the credentials ( LOGON ). */
#ifdef BDM
 SMS_IOPRefreshMass ();
#endif

 if ( g_IOPFlags & SMS_IOPF_SMB ) {

  int  lTry;
  int  lRes = -1;
  char lBuf[ 64 ];

  for ( lTry = 1; lTry <= 3 && lRes != 0; ++lTry ) {

   sprintf ( lBuf, "Reconnecting network... %d/3", lTry );
   GUI_Status ( lBuf );

   lRes = _smb_logon ();

   if ( lRes != 0 && lTry < 3 ) SMS_TimerWait ( 700 );

  }  /* end for */

  if ( lRes != 0 ) {

   sprintf ( lBuf, "Network reconnect failed ( err %d ) -- retry or reboot", g_SMBError );
   GUI_Error ( lBuf );

  }  /* end if */

 }  /* end if */

 GUI_MenuPopState ( apMenu );
 _device_handler  ( apMenu, 0 );

}  /* end _refresh_handler */

static char s_IP [ 4 ][ 4 ] __attribute__(   (  section( ".data" )  )   );
static char s_MSK[ 4 ][ 4 ] __attribute__(   (  section( ".data" )  )   );
static char s_GW [ 4 ][ 4 ] __attribute__(   (  section( ".data" )  )   );

static SMString s_StrIP[ 4 ] __attribute__(   (  section( ".data" )  )   ) = {
 { 0, s_IP[ 0 ] }, { 0, s_IP[ 1 ] }, { 0, s_IP[ 2 ] }, { 0, s_IP[ 3 ] }
};

static SMString s_StrMSK[ 4 ] __attribute__(   (  section( ".data" )  )   ) = {
 { 0, s_MSK[ 0 ] }, { 0, s_MSK[ 1 ] }, { 0, s_MSK[ 2 ] }, { 0, s_MSK[ 3 ] }
};

static SMString s_StrGW[ 4 ] __attribute__(   (  section( ".data" )  )   ) = {
 { 0, s_GW[ 0 ] }, { 0, s_GW[ 1 ] }, { 0, s_GW[ 2 ] }, { 0, s_GW[ 3 ] }
};

static void _update_ips ( void ) {

 int i;

 for ( i = 0; i < 4; ++i ) s_StrIP [ i ].m_Len = strlen ( s_IP [ i ] );
 for ( i = 0; i < 4; ++i ) s_StrMSK[ i ].m_Len = strlen ( s_MSK[ i ] );
 for ( i = 0; i < 4; ++i ) s_StrGW [ i ].m_Len = strlen ( s_GW [ i ] );

}  /* end _update_ips */

static void _set_ips ( char apDst[][ 4 ], char* apSrc ) {

 int i, j;

 for ( i = 0; i < 4; ++i ) {

  j = 0;

  for ( ; *apSrc && *apSrc != '.'; ++apSrc ) if ( j < 3 ) apDst[ i ][ j++ ] = *apSrc;

  apDst[ i ][ j ] = '\x00';

  if ( *apSrc ) ++apSrc;

 }  /* end for */

}  /* end _set_ips */

static void _editipc_handler ( GUIMenu* apMenu, int aDir ) {

 int           i = 0, j;
 GUIMenuState* lpState = GUI_MenuPushState ( apMenu );

 _set_ips ( s_IP,  g_pDefIP   );
 _set_ips ( s_MSK, g_pDefMask );
 _set_ips ( s_GW,  g_pDefGW   );

 _update_ips ();

 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_IPCMenu;
 lpState -> m_pLast  = &s_IPCMenu[ sizeof ( s_IPCMenu ) / sizeof ( s_IPCMenu[ 0 ] ) - 1 ];
 lpState -> m_pTitle = &STR_EDIT_IPCONFIG1;

 for ( j = 0; j < 4; ++j, ++i ) s_IPCMenu[ i ].m_IconRight = ( unsigned int )&s_StrIP [ j ];
 for ( j = 0; j < 4; ++j, ++i ) s_IPCMenu[ i ].m_IconRight = ( unsigned int )&s_StrMSK[ j ];
 for ( j = 0; j < 4; ++j, ++i ) s_IPCMenu[ i ].m_IconRight = ( unsigned int )&s_StrGW [ j ];

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _editipc_handler */

static void _cdvd_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, DEVROW_CDVD, &g_Config.m_NetworkFlags, SMS_DF_CDVD );

}  /* end _cdvd_handler */

static void _cdvd_spd_handler ( GUIMenu* apMenu, int aDir ) {

 int lSpeed = g_Config.m_CDVDSpeed + aDir;

 if ( lSpeed < 0 )
  lSpeed = 2;
 else if ( lSpeed > 2 ) lSpeed = 0;

 s_DevMenu[ DEVROW_CDVD_SPEED ].m_IconRight = ( unsigned int )s_Speeds[ g_Config.m_CDVDSpeed = lSpeed ];
 apMenu -> Redraw ( apMenu );
 CDVD_SetSpeed ();

}  /* end _cdvd_spd_handler */

static void _dirbtn_handler ( GUIMenu* apMenu, int aDir ) {

 g_Config.m_BrowserFlags ^= SMS_BF_DIRB;
 s_DevMenu[ DEVROW_DIRBTN ].m_IconRight = ( unsigned int )( g_Config.m_BrowserFlags & SMS_BF_DIRB ? &STR_REMOTE_CONTROL : &STR_GAMEPAD );
 apMenu -> Redraw ( apMenu );

 SMS_SetDirButtons ();

}  /* end _dirbtn_handler */

static void _roll_ip_number ( GUIMenu* apMenu, char* apStr, int aDir ) {

 int lNum = atoi ( apStr );

 lNum += aDir;

 if ( lNum < 0 )
  lNum = 255;
 else if ( lNum > 255 ) lNum = 0;

 sprintf ( apStr, "%d", lNum );

 _update_ips ();
 apMenu -> Redraw ( apMenu );

}  /* end _roll_ip_number */

static int s_Masks[ 9 ] = {
 0, 128, 192, 224, 240, 248, 252, 254, 255
};

static void _roll_mask_number ( GUIMenu* apMenu, char* apStr, int aDir ) {

 int i;
 int lNum = atoi ( apStr );

 for ( i = 0; i < 9; ++i ) if ( lNum == s_Masks[ i ] ) break;

 if ( i == 9 ) i = 0;

 i += aDir;

 if ( i < 0 )
  i = 8;
 else if ( i > 8 ) i = 0;

 sprintf ( apStr, "%d", s_Masks[ i ] );

 _update_ips ();
 apMenu -> Redraw ( apMenu );

}  /* end _roll_mask_number */

static void _ip1_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_ip_number ( apMenu, s_IP[ 0 ], aDir );

}  /* end _ip1_handler */

static void _ip2_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_ip_number ( apMenu, s_IP[ 1 ], aDir );

}  /* end _ip2_handler */

static void _ip3_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_ip_number ( apMenu, s_IP[ 2 ], aDir );

}  /* end _ip3_handler */

static void _ip4_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_ip_number ( apMenu, s_IP[ 3 ], aDir );

}  /* end _ip4_handler */

static void _nm1_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_mask_number ( apMenu, s_MSK[ 0 ], aDir );

}  /* end _nm1_handler */

static void _nm2_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_mask_number ( apMenu, s_MSK[ 1 ], aDir );

}  /* end _nm2_handler */

static void _nm3_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_mask_number ( apMenu, s_MSK[ 2 ], aDir );

}  /* end _nm3_handler */

static void _nm4_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_mask_number ( apMenu, s_MSK[ 3 ], aDir );

}  /* end _nm4_handler */

static void _gw1_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_ip_number ( apMenu, s_GW[ 0 ], aDir );

}  /* end _gw1_handler */

static void _gw2_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_ip_number ( apMenu, s_GW[ 1 ], aDir );

}  /* end _gw2_handler */

static void _gw3_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_ip_number ( apMenu, s_GW[ 2 ], aDir );

}  /* end _gw3_handler */

static void _gw4_handler ( GUIMenu* apMenu, int aDir ) {

 _roll_ip_number ( apMenu, s_GW[ 3 ], aDir );

}  /* end _gw4_handler */

static void _saveipc_handler ( GUIMenu* apMenu, int aDir ) {

 int  i, j;
 int  lRes;
 char lBuf[ 64 ];
 char lDir[ 14 ];
 char lDiag[ 48 ];
 int  lSts = 0;

 lDiag[ 0 ] = '\x00';   /* empty => every stage succeeded, no GUI_Error at the end */

 GUI_Status ( STR_SAVING_IPCONFIG.m_pStr );

 lBuf[ 0 ] = '\x00';

 for ( i = 0; i < 4; ++i ) {

  strcat ( lBuf, s_IP[ i ] );
  strcat ( lBuf, g_DotStr  );

 }  /* end for */

 j = strlen ( lBuf ) - 1;
 lBuf[ j   ] = '\x00';
 strcpy ( g_pDefIP, lBuf );
 lBuf[ j++ ] = '\n';

 for ( i = 0; i < 4; ++i ) {

  strcat ( lBuf, s_MSK[ i ] );
  strcat ( lBuf, g_DotStr  );

 }  /* end for */

 i = strlen ( lBuf ) - 1;

 lBuf[ i ] = '\x00';
 strcpy ( g_pDefMask, &lBuf[ j ] );
 lBuf[ i ] = '\n';
 j = i + 1;

 for ( i = 0; i < 4; ++i ) {

  strcat ( lBuf, s_GW[ i ] );
  strcat ( lBuf, g_DotStr  );

 }  /* end for */

 i = strlen ( lBuf ) - 1;

 lBuf[ i ] = '\x00';
 strcpy ( g_pDefGW, &lBuf[ j ] );
 lBuf[ i ] = '\n';

 strncpy ( lDir, g_pIPConf, 13 );
 lDir[ 13 ] = '\x00';

/* NO MEMORY CARD -> save beside the ELF instead of failing.
 *
 * Reported from a PSX (DESR-7100) running with no card fitted: the built-in default IP was
 * outside the tester's subnet, so UDPFS could never find a server -- and there was no way to
 * correct it, because both reading and writing IPCONFIG.DAT were pinned to mc0:. A card-less
 * console could not be given an address at all.
 *
 * The boot reader now looks in the CWD first (SMS_IOP.c), so writing there is what makes the
 * setting round-trip. Only taken when there is a usable CWD and no card: with a card present
 * the on-card SYS-CONF path stays authoritative, because that is where the PS2 browser and
 * every other title expect it, and quietly moving it would break those. */
#ifdef BDM
 if (  g_pBootDir[ 0 ] && !SMS_MCPresent ()  ) {

  char lIPCwd[ 128 ];

  if (  strlen ( g_pBootDir ) + 13 < sizeof ( lIPCwd )  ) {

   int lFD;

   strcpy ( lIPCwd, g_pBootDir );
   strcat ( lIPCwd, "IPCONFIG.DAT" );

   lFD = fioOpen ( lIPCwd, O_CREAT | O_WRONLY | O_TRUNC );

   if ( lFD >= 0 ) {

    int lLen = strlen ( lBuf );
    int lWr  = fioWrite ( lFD, lBuf, lLen );

    fioClose ( lFD );

    if ( lWr == lLen ) {
     GUI_Status ( STR_SAVING_IPCONFIG.m_pStr );
     return;
    }  /* end if */

    sprintf ( lDiag, "IPCONFIG: CWD write %d", lWr );

   } else sprintf ( lDiag, "IPCONFIG: CWD open %d", lFD );

   GUI_Error ( lDiag );
   return;

  }  /* end if */

 }  /* end if */
#endif

#ifdef BDM
 /* Gate on the fio-coherent card check ( SMS_MCPresent -> _mc_get_info, which now
  * falls back to a fio probe when libmc falsely reports the card absent on a udpfs
  * boot ) instead of a raw MC_GetInfo, which blocks the save on such boots. */
 lRes = SMS_MCPresent () ? 0 : -2;
#else
 MC_GetInfo ( g_MCSlot, 0, &lRes, &lRes, &lRes );
 MC_Sync ( &lRes );
#endif

 /* libmc on the modern iomanX + mcman stack does NOT see fio-created dirs, so a
  * MC_GetDir precheck returns inconsistent status and would wrongly skip the write.
  * Mirror SMS_SaveSMBInfo: fioMkdir, then write via fio coherently with how
  * SMS_IOP.c reads g_pIPConf at boot.
  *
  * The messages below now carry the NUMERIC return ( TEMP diagnostics ). The owner
  * reported only a paraphrase, "Saving IP error/failed", and these four stages are
  * four different bugs -- the literal string collapses them to one in a single run. */
 if ( lRes <= -2 ) sprintf ( lDiag, "IPCONFIG: no card (%d)", lRes );
 else {

/* fioMkdir is BEST EFFORT. It used to abort the whole save unless it returned 0 or -4,
  * on the assumption that -4 means EEXIST -- which is unverified against mcman ( libmc's
  * error space has no EEXIST; -4 is sceMcResNotEmpty ). mc0:/SYS-CONF normally already
  * exists ( Sony creates it ), so this always takes the already-exists branch on a real
  * card. Let fioOpen be the arbiter instead: if the dir really is missing, the open fails
  * and reports a real errno rather than a guess. */
  fioMkdir ( lDir );

  {
/* O_TRUNC: the sibling SMB save already does this ( SMS_Config.c:300 ). Without it,
   * writing a shorter config ( "192.168.1.1" ) over a longer one ( "192.168.001.100" )
   * left stale trailing bytes, and the boot parser ( SMS_IOP.c ) NUL-splits on whitespace
   * so the remnant landed in the mask/gw fields. Silent corruption, not the reported
   * error -- but a real defect on its own. */
   int lFD = fioOpen ( g_pIPConf, O_CREAT | O_WRONLY | O_TRUNC );

   if ( lFD < 0 ) sprintf ( lDiag, "IPCONFIG: open %d", lFD );
   else {

    int lWr;

    i    = strlen ( lBuf );
    lWr  = fioWrite ( lFD, lBuf, i );
    lSts = ( lWr == i );
    fioClose ( lFD );

    if ( !lSts ) sprintf ( lDiag, "IPCONFIG: write %d/%d", lWr, i );

   }  /* end else */

  }

 }  /* end else */

 if ( lDiag[ 0 ] ) GUI_Error ( lDiag );

 GUIMenuSMS_UpdateStatus ( apMenu );

}  /* end _saveipc_handler */

static void _usebg_handler ( GUIMenu* apMenu, int aDir ) {

 SMS_List*     lpList;
 SMS_ListNode* lpNode = SMS_ListFind (
  lpList = g_Config.m_pSkinList, g_Config.m_SkinName
 );

 if ( !lpNode ) lpNode = lpList -> m_pHead;

 if ( aDir > 0 ) {
  lpNode = lpNode -> m_pNext;
  if ( !lpNode ) lpNode = lpList -> m_pHead;
 } else {
  lpNode = lpNode -> m_pPrev;
  if ( !lpNode ) lpNode = lpList -> m_pTail;
 }  /* end else */

 strcpy (  g_Config.m_SkinName, _STR( lpNode )  );

 _update_skin ();

 GUI_Initialize ( 0 );
 GUIMenuSMS_UpdateStatus ( apMenu );

}  /* end _usebg_handler */

static void _sound_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 1, &g_Config.m_BrowserFlags, SMS_BF_SDFX );

}  /* end _sound_handler */

static void _sortfs_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 2, &g_Config.m_BrowserFlags, SMS_BF_SORT );

}  /* end _sortfs_handler */

static void _filter_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 3, &g_Config.m_BrowserFlags, SMS_BF_AVIF );

 g_pFileMenu -> HandleEvent ( g_pFileMenu, GUI_MSG_REFILL_BROWSER );

 GUI_Redraw ( GUIRedrawMethod_InitClearObj );
 GUIMenuSMS_UpdateStatus ( apMenu );

}  /* end _filter_handler */

static void _apply_hdd_filter ( GUIMenu* apMenu ) {

 if ( g_CMedia == 2 && g_CWD[ 0 ] == 'h' &&
                       g_CWD[ 1 ] == 'd' &&
                       g_CWD[ 2 ] == 'd' &&
                       g_CWD[ 3 ] == '0'
 ) {

  g_pFileMenu -> HandleEvent ( g_pFileMenu, GUI_MSG_REFILL_BROWSER );

  GUI_Redraw ( GUIRedrawMethod_InitClearObj );
  GUIMenuSMS_UpdateStatus ( apMenu );

 }  /* end if */

}  /* end _apply_hdd_filter */

static void _dsphdl_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 4, &g_Config.m_BrowserFlags, SMS_BF_HDLP );
 _apply_hdd_filter ( apMenu );

}  /* end _dsphdl_handler */

static void _hidesp_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 5, &g_Config.m_BrowserFlags, SMS_BF_SYSP );
 _apply_hdd_filter ( apMenu );

} /* end _hidesp_handler */

static void _usexh_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 6, &g_Config.m_BrowserFlags, SMS_BF_UXH );

}  /* end _usexh_handler */

static void _rotate_palette ( GUIMenu* apMenu, unsigned int* apVal, int aDir ) {

 int lVal = *apVal;

 lVal += aDir;

 if ( lVal == 17 )
  lVal = 1;
 else if ( lVal < 0 ) lVal = 16;

 *apVal = lVal;

 GUI_Redraw ( GUIRedrawMethod_InitClearObj );
 GUIMenuSMS_UpdateStatus ( apMenu );

}  /* end _rotate_palette */

static void _abclr_handler ( GUIMenu* apMenu, int aDir ) {

 _rotate_palette ( apMenu, &g_Config.m_BrowserABCIdx, aDir );

}  /* end _abclr_handler */

static void _ibclr_handler ( GUIMenu* apMenu, int aDir ) {

 _rotate_palette ( apMenu, &g_Config.m_BrowserIBCIdx, aDir );

}  /* end _ibclr_handler */

static void _txtclr_handler ( GUIMenu* apMenu, int aDir ) {

 int           lVal = g_Config.m_BrowserTxtIdx;
 u64           lColor;

 lVal += aDir;

 if ( lVal == 17 )
  lVal = 1;
 else if ( lVal < 0 ) lVal = 16;

 g_Config.m_BrowserTxtIdx = lVal;
 lColor                   = g_Palette[ g_Config.m_BrowserTxtIdx - 1 ];

 GSContext_SetTextColor (  1,   lColor                              );
 GSContext_SetTextColor (  2, ( lColor & 0x00FFFFFF ) | 0x20000000  );

 GUI_Redraw ( GUIRedrawMethod_InitClearObj );
 GUIMenuSMS_UpdateStatus ( apMenu );

}  /* end _txtclr_handler */

static void _sltxt_handler ( GUIMenu* apMenu, int aDir ) {

 int lVal = g_Config.m_BrowserSBCIdx;

 lVal += aDir;

 if ( lVal == 17 )
  lVal = 1;
 else if ( lVal < 0 ) lVal = 16;

 g_Config.m_BrowserSBCIdx = lVal;

 GSContext_SetTextColor ( 0, g_Palette[ g_Config.m_BrowserSBCIdx - 1 ] );

 GUI_Redraw ( GUIRedrawMethod_InitClearObj );
 GUIMenuSMS_UpdateStatus ( apMenu );

}  /* end _sltxt_handler */

static void _sclr_handler ( GUIMenu* apMenu, int aDir ) {

 _rotate_palette ( apMenu, &g_Config.m_BrowserSCIdx, aDir );

}  /* end _sclr_handler */

static void _exit_2_handler ( GUIMenu* apMenu, int aDir ) {
#ifndef EMBEDDED
 int lIndex = g_Config.m_BrowserFlags >> 28;

 lIndex += aDir;

 if ( lIndex < 0 )

  lIndex = 2;

 else if ( lIndex > 2 ) lIndex = 0;

 s_BrowserMenu[ 13 ].m_IconRight = ( unsigned int )s_ExitTo[ lIndex ];

 g_Config.m_BrowserFlags &= 0x0FFFFFFF;
 g_Config.m_BrowserFlags |= lIndex << 28;

 apMenu -> Redraw ( apMenu );
#endif  /* EMBEDDED */
}  /* end _exit_2_handler */

static void _pwrbtn_handler ( GUIMenu* apMenu, int aDir ) {
#ifndef EMBEDDED
 unsigned int lFlags = g_Config.m_BrowserFlags ^ SMS_BF_EXIT;

 s_BrowserMenu[ 12 ].m_IconRight = ( unsigned int )(  ( lFlags & SMS_BF_EXIT ) ? &STR_EXIT : &STR_POWER_OFF  );

 g_Config.m_BrowserFlags = lFlags;

 apMenu -> Redraw ( apMenu );
#endif  /* EMBEDDED */
}  /* end _exit_2_handler */

static void _autols_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 1, &g_Config.m_PlayerFlags, SMS_PF_SUBS );

}  /* end _autols_handler */

static void _opaqs_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 2, &g_Config.m_PlayerFlags, SMS_PF_OSUB );

}  /* end _opaqs_handler */

static void _dsbt_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 3, &g_Config.m_PlayerFlags, SMS_PF_TIME );

}  /* end _dsbt_handler */

static void _spdif_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 4, &g_Config.m_PlayerFlags, SMS_PF_SPDIF );

}  /* end _spdif_handler */

static void _pdw22_handler ( GUIMenu* apMenu, int aDir ) {

 unsigned int  lIconIdx;
 GUIMenuState* lpState = ( GUIMenuState* )( unsigned int )apMenu -> m_pState -> m_pTail -> m_Param;
 unsigned int* lpFlag  = &g_Config.m_PlayerFlags;
 int           lfShow;

 if ( *lpFlag & SMS_PF_PDW22 ) {
  *lpFlag &= ~SMS_PF_PDW22;
  lIconIdx = GUICON_OFF;
  lfShow   = 1;
 } else {
  *lpFlag |= SMS_PF_PDW22;
  lIconIdx = GUICON_ON;
  lfShow   = 0;
 }  /* end else */

 lpState -> m_pItems[ 5 ].m_IconRight = lIconIdx;

 GUIMenu_ShowItem ( apMenu, 4, lfShow );

 apMenu -> Redraw ( apMenu );

}  /* end _pdw22_handler */

static void _rotate_palette_2 ( GUIMenu* apMenu, int* apVal, int aDir ) {

 int lVal = *apVal;

 lVal += aDir;

 if ( lVal == 17 )
  lVal = 1;
 else if ( lVal < 0 ) lVal = 16;

 *apVal = lVal;

 apMenu -> Redraw ( apMenu );

}  /* end _rotate_palette_2 */

void _subclr_handler ( GUIMenu* apMenu, int aDir ) {

 _rotate_palette_2 ( apMenu, ( int * )( &g_Config.m_PlayerSCNIdx ), aDir );

}  /* end _subclr_handler */

void _subbclr_handler ( GUIMenu* apMenu, int aDir ) {

 _rotate_palette_2 ( apMenu, ( int * )( &g_Config.m_PlayerSCBIdx ), aDir );

}  /* end _subbclr_handler */

void _subiclr_handler ( GUIMenu* apMenu, int aDir ) {

 _rotate_palette_2 ( apMenu, ( int * )( &g_Config.m_PlayerSCIIdx ), aDir );

}  /* end _subiclr_handler */

void _subu_handler ( GUIMenu* apMenu, int aDir ) {

 _rotate_palette_2 ( apMenu, ( int * )( &g_Config.m_PlayerSCUIdx ), aDir );

}  /* end _subu_handler */

static void _sbclr_handler ( GUIMenu* apMenu, int aDir ) {

 _rotate_palette_2 ( apMenu, ( int * )( &g_Config.m_PlayerSBCIdx ), aDir );

}  /* end _sbclr_handler */

static void _vbclr_handler ( GUIMenu* apMenu, int aDir ) {

 _rotate_palette_2 ( apMenu, ( int * )( &g_Config.m_PlayerVBCIdx ), aDir );

}  /* end _vbclr_handler */

static void _clres_handler ( GUIMenu* apMenu, int aDir ) {

 if (   !(  g_Config.m_PlayerFlags & ( SMS_PF_C32 | SMS_PF_C16 )  )   )

  g_Config.m_PlayerFlags |= SMS_PF_C32;

 else if ( g_Config.m_PlayerFlags & SMS_PF_C32 ) {

  g_Config.m_PlayerFlags &= ~SMS_PF_C32;
  g_Config.m_PlayerFlags |=  SMS_PF_C16;

 } else g_Config.m_PlayerFlags &= ~( SMS_PF_C32 | SMS_PF_C16 );

 _update_pstr ( 0 );
 apMenu -> Redraw ( apMenu );

}  /* end _clres_handler */

static void _vol_handler ( GUIMenu* apMenu, int aDir ) {

 if ( aDir > 0 ) {

  if ( ++g_Config.m_PlayerVolume == 25 ) g_Config.m_PlayerVolume = 0;

 } else if ( g_Config.m_PlayerVolume ) --g_Config.m_PlayerVolume;

 _update_pstr ( 0 );
 apMenu -> Redraw ( apMenu );

}  /* end _vol_handler */

static void _salign_handler ( GUIMenu* apMenu, int aDir ) {

 g_Config.m_PlayerSAlign = (  ( g_Config.m_PlayerSAlign + 1 ) % 4  );

 _update_pstr ( 0 );
 apMenu -> Redraw ( apMenu );

}  /* end _salign_handler */

static void _suboff_handler ( GUIMenu* apMenu, int aDir ) {

 int lBottom = 32 + g_Config.m_SubVIncr;

 if ( aDir > 0 ) {

  if ( ++g_Config.m_PlayerSubOffset == 145 ) g_Config.m_PlayerSubOffset = lBottom;

 } else if ( g_Config.m_PlayerSubOffset > lBottom )

  --g_Config.m_PlayerSubOffset;

 else g_Config.m_PlayerSubOffset = 144;

 _update_pstr ( 0 );
 s_pSample -> Cleanup ( s_pSample );
 apMenu -> Redraw ( apMenu );
 _redraw_sample ();

}  /* end _suboff_handler */

static void _poff_handler ( GUIMenu* apMenu, int aDir ) {

 aDir *= 60000;

 _update_pstr ( aDir );
 apMenu -> Redraw ( apMenu );

}  /* end _poff_handler */

static void _sblen_handler ( GUIMenu* apMenu, int aDir ) {

 if ( aDir < 0 ) {

  g_Config.m_ScrollBarNum -= 16;
 
  if ( g_Config.m_ScrollBarNum == 16 ) g_Config.m_ScrollBarNum = 128;

 } else {

  g_Config.m_ScrollBarNum += 16;
 
  if ( g_Config.m_ScrollBarNum == 144 ) g_Config.m_ScrollBarNum = 32;

 }  /* end else */

 _update_pstr ( 0 );
 apMenu -> Redraw ( apMenu );

}  /* end _sblen_handler */

static void _sbpos_handler ( GUIMenu* apMenu, int aDir ) {

 switch ( g_Config.m_ScrollBarPos ) {

  default                     :
  case SMScrollBarPos_Top     : g_Config.m_ScrollBarPos = SMScrollBarPos_Bottom;   break;
  case SMScrollBarPos_Bottom  : g_Config.m_ScrollBarPos = SMScrollBarPos_Inactive; break;
  case SMScrollBarPos_Inactive: g_Config.m_ScrollBarPos = SMScrollBarPos_Top;      break;

 }  /* end switch */

 _update_pstr ( 0 );
 apMenu -> Redraw ( apMenu );

}  /* end _sbpos_handler */

static void _sfonth_handler ( GUIMenu* apMenu, int aDir ) {

 if ( aDir > 0 && g_Config.m_SubHIncr < 48 )

  ++g_Config.m_SubHIncr;

 else if ( aDir < 0 && g_Config.m_SubHIncr > -16 ) --g_Config.m_SubHIncr;

 _update_pstr ( 0 );
 s_pSample -> Cleanup ( s_pSample );
 apMenu -> Redraw ( apMenu );
 _redraw_sample ();

}  /* end _sfonth_handler */

static void _sfontv_handler ( GUIMenu* apMenu, int aDir ) {

 if ( aDir > 0 && g_Config.m_SubVIncr < 48 ) {

  int lDelta = 32 + g_Config.m_SubVIncr;

  ++g_Config.m_SubVIncr;

  if ( lDelta > g_Config.m_PlayerSubOffset ) g_Config.m_PlayerSubOffset = lDelta;

 } else if ( aDir < 0 && g_Config.m_SubVIncr > -16 ) --g_Config.m_SubVIncr;

 _update_pstr ( 0 );
 s_pSample -> Cleanup ( s_pSample );
 apMenu -> Redraw ( apMenu );
 _redraw_sample ();

}  /* end _sfontv_handler */

static void _enter_sample ( GUIMenu* apMenu ) {

 void* lpActiveNode = g_pActiveNode;

 GUI_AddObject (  STR_SAMPLE.m_pStr, s_pSample = _create_sample ()  );
 g_pActiveNode = lpActiveNode;
 apMenu -> Redraw ( apMenu );
 _redraw_sample ();

}  /* end _enter_sample */

static void _leave_sample ( GUIMenu* apMenu ) {

 void* lpActiveNode = g_pActiveNode;

 GUI_DeleteObject ( STR_SAMPLE.m_pStr );
 g_pActiveNode = lpActiveNode;
 apMenu -> Redraw ( apMenu );

}  /* end _leave_sample */

static void _smbcs_handler ( GUIMenu* apMenu, int aDir ) {

 SMS_List*     lpList;
 SMS_ListNode* lpNode = SMS_ListFind (
  lpList = g_Config.m_pMBFList, g_Config.m_MBFName
 );

 if ( !lpNode ) lpNode = lpList -> m_pHead;

 if ( aDir > 0 ) {
  lpNode = lpNode -> m_pNext;
  if ( !lpNode ) lpNode = lpList -> m_pHead;
 } else {
  lpNode = lpNode -> m_pPrev;
  if ( !lpNode ) lpNode = lpList -> m_pTail;
 }  /* end else */

 strcpy (  g_Config.m_MBFName, _STR( lpNode )  );

 _update_mbf ();

 apMenu -> Redraw ( apMenu );

}  /* end _smbcs_handler */

static char s_DispHBuff[ 5 ] __attribute__(   (  section( ".data" ), aligned( 1 )  )   );
static char s_DispWBuff[ 5 ] __attribute__(   (  section( ".data" ), aligned( 1 )  )   );
static char s_SynP1Buff[ 7 ] __attribute__(   (  section( ".data" ), aligned( 1 )  )   );
static char s_SynP2Buff[ 7 ] __attribute__(   (  section( ".data" ), aligned( 1 )  )   );
static char s_SynP3Buff[ 7 ] __attribute__(   (  section( ".data" ), aligned( 1 )  )   );

static SMString  s_StrAdvH __attribute__(   (  section( ".data" )  )   ) = { 0, s_DispHBuff };
static SMString  s_StrAdvW __attribute__(   (  section( ".data" )  )   ) = { 0, s_DispWBuff };
static SMString  s_StrSyP1 __attribute__(   (  section( ".data" )  )   ) = { 0, s_SynP1Buff };
static SMString  s_StrSyP2 __attribute__(   (  section( ".data" )  )   ) = { 0, s_SynP2Buff };
static SMString  s_StrSyP3 __attribute__(   (  section( ".data" )  )   ) = { 0, s_SynP3Buff };
static SMString* s_ClrRes[ 2 ] __attribute__(   (  section( ".data" )  )   ) = {
 &STR_32_BIT, &STR_16_BIT
};

static void _update_advs ( void ) {

 int   lIdx = GS_VMode2Index (  GS_Params () -> m_GSCRTMode  );
 int   lH, lW;
 short lSP1, lSP2, lSP3;
 
 lW   = g_Config.m_DispWH[ lIdx ][ 0 ];
 lH   = g_Config.m_DispWH[ lIdx ][ 1 ];
 lSP1 = g_Config.m_SyncPar[ lIdx ][ 0 ];
 lSP2 = g_Config.m_SyncPar[ lIdx ][ 1 ];
 lSP3 = g_Config.m_SyncPar[ lIdx ][ 2 ];
 
 sprintf ( s_DispHBuff, "%d", lH   );
 sprintf ( s_DispWBuff, "%d", lW   );
 sprintf ( s_SynP1Buff, "%d", lSP1 );
 sprintf ( s_SynP2Buff, "%d", lSP2 );
 sprintf ( s_SynP3Buff, "%d", lSP3 );

 s_StrAdvH.m_Len = strlen ( s_DispHBuff );
 s_StrAdvW.m_Len = strlen ( s_DispWBuff );
 s_StrSyP2.m_Len = strlen ( s_SynP2Buff );
 s_StrSyP3.m_Len = strlen ( s_SynP3Buff );

 if ( lSP1 ) {
  s_StrSyP1.m_Len  = strlen ( s_SynP1Buff );
  s_StrSyP1.m_pStr = s_SynP1Buff;
 } else {
  s_StrSyP1.m_Len  = STR_AUTO.m_Len;
  s_StrSyP1.m_pStr = STR_AUTO.m_pStr;
 }  /* end else */

}  /* end _update_advs */

static void _advset_handler ( GUIMenu* apMenu, int aDir ) {

 GUIMenuState* lpState = GUI_MenuPushState ( apMenu );

 _update_advs ();

 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_AdvDispMenu;
 lpState -> m_pLast  = &s_AdvDispMenu[ sizeof ( s_AdvDispMenu ) / sizeof ( s_AdvDispMenu[ 0 ] ) - 1 ];
 lpState -> m_pTitle = &STR_ADVANCED_SETTINGS1;

 s_AdvDispMenu[ 0 ].m_IconRight = ( unsigned int )&s_StrAdvH;
 s_AdvDispMenu[ 1 ].m_IconRight = ( unsigned int )&s_StrAdvW;
 s_AdvDispMenu[ 2 ].m_IconRight = ( unsigned int )s_ClrRes[ g_Config.m_ColorDepth ];
 s_AdvDispMenu[ 3 ].m_IconRight = ( unsigned int )&s_StrSyP1;
 s_AdvDispMenu[ 4 ].m_IconRight = ( unsigned int )&s_StrSyP2;
 s_AdvDispMenu[ 5 ].m_IconRight = ( unsigned int )&s_StrSyP3;

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _advset_handler */

/* The 720p ceiling used to be 1190 -- below the 1216 that SMS_Config.c ships as the 720p
 * DEFAULT. The very first press of X/O on "Display width" in 720p therefore clamped the
 * width down to 1190, and the default could never be selected again ( PSX-Place report,
 * SCPH-50004 ). Inherited unchanged from 2.9 rev4. The ceiling is now the default itself,
 * not 1280: the 720p framebuffer already takes ~3.3 MB of the GS's 4 MB ( see
 * GS_DisplayOffsets in SMS_GS_1.c ), so growing it past the shipped size risks the font /
 * icon VRAM collision that broke 1080i text. Every default must lie inside [ min, max ];
 * tools/test_display_limits.py checks that for all eight modes. */
static void _dispw_handler ( GUIMenu* apMenu, int aDir ) {

 static int s_lMin[ 8 ] __attribute__(   (  section( ".data" )  )   ) = { 640, 640, 640, 640, 1090, 1400, 640, 640 };
 static int s_lMax[ 8 ] __attribute__(   (  section( ".data" )  )   ) = { 720, 720, 720, 720, 1216, 1830, 720, 720 };

 int lIdx = GS_VMode2Index (  GS_Params () -> m_GSCRTMode  );
 int lWidth;

 lWidth = g_Config.m_DispWH[ lIdx ][ 0 ] + aDir;

 if ( lWidth > s_lMax[ lIdx ] )
  lWidth = s_lMax[ lIdx ];
 else if ( lWidth < s_lMin[ lIdx ] )
  lWidth = s_lMin[ lIdx ];

 g_Config.m_DispWH[ lIdx ][ 0 ] = lWidth;

 _update_advs ();

 apMenu -> Redraw ( apMenu );

}  /* end _dispw_handler */

static void _disph_handler ( GUIMenu* apMenu, int aDir ) {

 static int s_lMin[ 8 ] __attribute__(   (  section( ".data" )  )   ) = { 416, 448, 480, 512, 630,  790, 480, 480 };
 static int s_lMax[ 8 ] __attribute__(   (  section( ".data" )  )   ) = { 480, 576, 576, 592, 830, 1024, 576, 576 };

 int lIdx = GS_VMode2Index (  GS_Params () -> m_GSCRTMode   );
 int lHeight;

 lHeight = g_Config.m_DispWH[ lIdx ][ 1 ] + aDir;

 if ( lHeight > s_lMax[ lIdx ] )
  lHeight = s_lMax[ lIdx ];
 else if ( lHeight < s_lMin[ lIdx ] )
  lHeight = s_lMin[ lIdx ];

 g_Config.m_DispWH[ lIdx ][ 1 ] = lHeight;
 g_Config.m_PAR[ lIdx ]         = ( float )lHeight / 480.0F;

 _update_advs ();

 apMenu -> Redraw ( apMenu );

}  /* end _disph_handler */

static void _dispc_handler ( GUIMenu* apMenu, int aDir ) {

 g_Config.m_ColorDepth = ( g_Config.m_ColorDepth + aDir ) & 1;

 s_AdvDispMenu[ 2 ].m_IconRight = ( unsigned int )s_ClrRes[ g_Config.m_ColorDepth ];

 apMenu -> Redraw ( apMenu );

}  /* end _dispc_handler */

short g_MaxSyncVal[ 8 ] __attribute__(   (  section( ".data" )  )   ) = {
 263, 312, 525, 576, 750, 563, 525, 500
};

static void SMS_INLINE _verify_sp ( int anIdx, short* apVal ) {

 if ( apVal[ 0 ] < 0 )
  apVal[ 0 ] = g_MaxSyncVal[ anIdx ];
 else if ( apVal[ 0 ] > g_MaxSyncVal[ anIdx ] )
  apVal[ 0 ] = 0;

}  /* end _verify_sp */

static void _update_sp ( GUIMenu* apMenu, int aDir, int anIdx ) {

 short* lpVal;
 short  lVal;
 int    lIdx = GS_VMode2Index (  GS_Params () -> m_GSCRTMode  );

 lpVal  = &g_Config.m_SyncPar[ lIdx ][ anIdx ];
 lVal   = *lpVal;
 lVal  += aDir;
 *lpVal = lVal;

 _verify_sp ( lIdx, lpVal );
 _update_advs ();

 apMenu -> Redraw ( apMenu );

}  /* end _update_sp */

static void _disps1_handler ( GUIMenu* apMenu, int aDir ) {

 _update_sp ( apMenu, aDir, 0 );

}  /* end _disps1_handler */

static void _disps2_handler ( GUIMenu* apMenu, int aDir ) {

 _update_sp ( apMenu, aDir, 1 );

}  /* end _disps2_handler */

static void _disps3_handler ( GUIMenu* apMenu, int aDir ) {

 _update_sp ( apMenu, aDir, 2 );

}  /* end _disps2_handler */

static void _apply_handler ( GUIMenu* apMenu, int aDir ) {

 GUI_MenuPopState ( apMenu );

 _reinitialize ( apMenu );

}  /* end _apply_handler */

static char     s_MP3DPS[ 3 ] __attribute__(   (  section( ".data" ), aligned( 1 )  )   );
static SMString s_StrMP3DPS = { 0, s_MP3DPS };

static void _mp3_update_dps ( void ) {

 sprintf ( s_MP3DPS, g_pPercDStr, g_Config.m_MP3AutoPar );
 s_StrMP3DPS.m_Len = strlen ( s_MP3DPS );

}  /* end _mp3_update_dps */

static void _mp3_handler ( GUIMenu* apMenu, int aDir ) {

 GUIMenuState* lpState = GUI_MenuPushState ( apMenu );

 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_MP3Menu;
 lpState -> m_pLast  = &s_MP3Menu[ sizeof ( s_MP3Menu ) / sizeof ( s_MP3Menu[ 0 ] ) - 1 ];
 lpState -> m_pTitle = &STR_MP3_SETTINGS;

 _mp3_update_dps ();

 s_MP3Menu[ 0 ].m_IconRight = g_Config.m_PlayerFlags & SMS_PF_ANIM  ? GUICON_ON : GUICON_OFF;
 s_MP3Menu[ 1 ].m_IconRight = g_Config.m_PlayerFlags & SMS_PF_RAND  ? GUICON_ON : GUICON_OFF;
 s_MP3Menu[ 2 ].m_IconRight = g_Config.m_PlayerFlags & SMS_PF_REP   ? GUICON_ON : GUICON_OFF;
 s_MP3Menu[ 3 ].m_IconRight = g_Config.m_PlayerFlags & SMS_PF_ASD   ? GUICON_ON : GUICON_OFF;
 s_MP3Menu[ 4 ].m_IconRight = ( unsigned int )&s_StrMP3DPS;

 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _mp3_handler */

static void _aadsp_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 0, &g_Config.m_PlayerFlags, SMS_PF_ANIM );

}  /* end _aadsp_handler */

static void _mp3_rand_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 1, &g_Config.m_PlayerFlags, SMS_PF_RAND );

}  /* end _mp3_rand_handler */

static void _mp3_repeat_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 2, &g_Config.m_PlayerFlags, SMS_PF_REP );

}  /* end _mp3_repeat_handler */

static void _mp3_asd_handler ( GUIMenu* apMenu, int aDir ) {

 _switch_flag ( apMenu, 3, &g_Config.m_PlayerFlags, SMS_PF_ASD );

}  /* end _mp3_asd_handler */

static void _mp3_adp_handler ( GUIMenu* apMenu, int aDir ) {

 g_Config.m_MP3AutoPar = ( char )g_Config.m_MP3AutoPar + aDir;

 if (  ( char )g_Config.m_MP3AutoPar > 99  )
  g_Config.m_MP3AutoPar = 1;
 else if (  ( char )g_Config.m_MP3AutoPar < 1  ) g_Config.m_MP3AutoPar = 99;

 _mp3_update_dps ();

 apMenu -> Redraw ( apMenu );

}  /* end _mp3_adp_handler */

static void _build_net_menu ( GUIMenu* );

static void _netprot_handler ( GUIMenu* apMenu, int aDir ) {

 if ( g_IOPFlags & SMS_IOPF_SMBINFO ) {

  SMS_ListNode* lpNode = apMenu -> m_pState -> m_pTail;
  GUIMenuState* lpState = ( GUIMenuState* )( unsigned int )lpNode -> m_Param;

  if ( g_Config.m_NetworkFlags & SMS_DF_SMB )
   g_Config.m_NetworkFlags &= ~SMS_DF_SMB;
  else g_Config.m_NetworkFlags |= SMS_DF_SMB;

  lpState -> m_pCurr -> m_IconRight = ( unsigned int )(  ( g_Config.m_NetworkFlags & SMS_DF_SMB ) ? &STR_SMB_CIFS : &STR_PS2DEV_HOST );

  _build_net_menu ( apMenu );
  apMenu -> Redraw ( apMenu );

 }  /* end if */

}  /* end _netprot_handler */

static void _update_opmode ( GUIMenu* apMenu ) {

 SMString*     lpStr;
 int           lMode   = ( g_Config.m_NetworkFlags >> 8 ) & 3;
 GUIMenuState* lpState = ( GUIMenuState* )( unsigned int )apMenu -> m_pState -> m_pTail -> m_Param;

 switch ( lMode ) {

  case 0 : lpStr = &STR_AUTONEGOTIATION; break;
  case 1 : lpStr = &STR_AUTOMATIC;       break;
  default: lpStr = &STR_MANUAL;          break;

 }  /* end switch */

 lpState -> m_pLast = lMode == 2 ? &s_NetMenu[ lpState -> m_Count - 1 ]
                                 : &s_NetMenu[ lpState -> m_Count - 3 ];

 s_NetMenu[ lpState -> m_Count - 3 ].m_IconRight = ( unsigned int )lpStr;

}  /* end _update_opmode */

static void _update_dup_mode ( GUIMenu* apMenu ) {

 GUIMenuState* lpState = ( GUIMenuState* )( unsigned int )apMenu -> m_pState -> m_pTail -> m_Param;

 s_NetMenu[ lpState -> m_Count - 2 ].m_IconRight = ( unsigned int )( g_Config.m_NetworkFlags & SMS_DF_HALF ? &STR_HALF : &STR_FULL );

}  /* end _update_dup_mode */

static void _update_standard ( GUIMenu* apMenu ) {

 GUIMenuState* lpState = ( GUIMenuState* )( unsigned int )apMenu -> m_pState -> m_pTail -> m_Param;

 s_NetMenu[ lpState -> m_Count - 1 ].m_IconRight = ( unsigned int )( g_Config.m_NetworkFlags & SMS_DF_10 ? &STR_10_BASE_T : &STR_100_BASE_TX );

}  /* end _update_standard */

static void _build_net_menu ( GUIMenu* apMenu ) {

 int           lIdx    = 2;
 GUIMenuState* lpState = ( GUIMenuState* )( unsigned int )apMenu -> m_pState -> m_pTail -> m_Param;

 lpState -> m_Count = 6;

 /* The "SMB servers..." entry is ALWAYS listed so SMB is reachable and
  * configurable from scratch, even with no SMS.smb file and no servers yet
  * (i.e. independent of SMS_DF_SMB / SMS_IOPF_SMBINFO). */
 s_NetMenu[ 2 ].m_Type        = 0;
 s_NetMenu[ 2 ].m_pOptionName = &STR_SMB_SERVER;
 s_NetMenu[ 2 ].m_IconLeft    = 0;
 s_NetMenu[ 2 ].m_IconRight   = 0;
 s_NetMenu[ 2 ].Handler       = _smbsrv_handler;
 s_NetMenu[ 2 ].Enter         = NULL;
 s_NetMenu[ 2 ].Leave         = NULL;
 ++lIdx;

 s_NetMenu[ lIdx   ].m_Type        = MENU_ITEM_TYPE_TEXT;
 s_NetMenu[ lIdx   ].m_pOptionName = &STR_OPERATING_MODE;
 s_NetMenu[ lIdx   ].m_IconLeft    = 0;
 s_NetMenu[ lIdx   ].m_IconRight   = 0;
 s_NetMenu[ lIdx   ].Handler       = _opmode_handler;
 s_NetMenu[ lIdx   ].Enter         = NULL;
 s_NetMenu[ lIdx++ ].Leave         = NULL;

 s_NetMenu[ lIdx   ].m_Type        = MENU_ITEM_TYPE_TEXT;
 s_NetMenu[ lIdx   ].m_pOptionName = &STR_DUPLEX_MODE;
 s_NetMenu[ lIdx   ].m_IconLeft    = 0;
 s_NetMenu[ lIdx   ].m_IconRight   = 0;
 s_NetMenu[ lIdx   ].Handler       = _duplex_handler;
 s_NetMenu[ lIdx   ].Enter         = NULL;
 s_NetMenu[ lIdx++ ].Leave         = NULL;

 s_NetMenu[ lIdx ].m_Type        = MENU_ITEM_TYPE_TEXT;
 s_NetMenu[ lIdx ].m_pOptionName = &STR_STANDARD;
 s_NetMenu[ lIdx ].m_IconLeft    = 0;
 s_NetMenu[ lIdx ].m_IconRight   = 0;
 s_NetMenu[ lIdx ].Handler       = _standt_handler;
 s_NetMenu[ lIdx ].Enter         = NULL;
 s_NetMenu[ lIdx ].Leave         = NULL;

 _update_opmode ( apMenu );
 _update_dup_mode ( apMenu );
 _update_standard ( apMenu );

}  /* end _build_net_menu */

static void _network_handler ( GUIMenu* apMenu, int aDir ) {

 GUIMenuState* lpState = GUI_MenuPushState ( apMenu );

 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_NetMenu;
 lpState -> m_pTitle = &STR_NETWORK_SETTINGS1;

 s_NetMenu[ 1 ].m_IconRight = ( unsigned int )(  ( g_Config.m_NetworkFlags & SMS_DF_SMB ) ? &STR_SMB_CIFS : &STR_PS2DEV_HOST );

 _build_net_menu ( apMenu );
 GUIMenuSMS_UpdateStatus ( apMenu );
 apMenu -> Redraw ( apMenu );

}  /* end _network_handler */

static void _opmode_handler ( GUIMenu* apMenu, int aDir ) {

 int lMode = ( g_Config.m_NetworkFlags >> 8 ) & 3;

 lMode += aDir;

 if ( lMode < 0 )
  lMode = 2;
 else if ( lMode > 2 )
  lMode = 0;

 lMode <<= 8;

 g_Config.m_NetworkFlags &= 0xFFFFFCFF;
 g_Config.m_NetworkFlags |= lMode;

 _update_opmode ( apMenu );

 apMenu -> Redraw ( apMenu );

}  /* end _opmode_handler */

static void _duplex_handler ( GUIMenu* apMenu, int aDir ) {

 g_Config.m_NetworkFlags ^= SMS_DF_HALF;

 _update_dup_mode ( apMenu );

 apMenu -> Redraw ( apMenu );

}  /* end _duplex_handler */

static void _standt_handler ( GUIMenu* apMenu, int aDir ) {

 g_Config.m_NetworkFlags ^= SMS_DF_10;

 _update_standard ( apMenu );

 apMenu -> Redraw ( apMenu );

}  /* end _standt_handler */

extern void _smb_menu ( GUIMenu* );

static void _smbsrv_handler ( GUIMenu* apMenu, int aDir ) {

 _smb_menu ( apMenu );

}  /* end _smbsrv_handler */

GUIObject* GUI_CreateMenuSMS ( void ) {

 GUIMenu*      retVal  = ( GUIMenu* )GUI_CreateMenu ();
 GUIMenuState* lpState;

 retVal -> m_Color      = 0x78301010UL;
 retVal -> HandleEvent  = GUIMenuSMS_HandleEvent;
 retVal -> Redraw       = GUIMenuSMS_Redraw;
 retVal -> m_pActiveObj = g_pActiveNode;
 retVal -> m_pState     = SMS_ListInit ();

 lpState = GUI_MenuPushState ( retVal );
 lpState -> m_pItems =
 lpState -> m_pFirst =
 lpState -> m_pCurr  = s_SMSMenu;
 lpState -> m_pLast  = &s_SMSMenu[ sizeof ( s_SMSMenu ) / sizeof ( s_SMSMenu[ 0 ] ) - 1 ];
 lpState -> m_pTitle = &STR_SMS_MENU;

 GUIMenuSMS_UpdateStatus ( retVal );
 _setup_dimensions (  ( GUIMenu* )retVal  );

 s_fHDD = 0;

 return ( GUIObject* )retVal;

}  /* end GUI_CreateMenuSMS */
