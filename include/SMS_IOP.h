/*
#     ___  _ _      ___
#    |    | | |    |
# ___|    |   | ___|    PS2DEV Open Source Project.
#----------------------------------------------------------
# (c) 2006 Eugene Plotnikov <e-plotnikov@operamail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
#
*/
#ifndef __SMS_IOP_H
#define __SMS_IOP_H

#define SMS_IOPF_DEV9     0x00000001
#define SMS_IOPF_HDD      0x00000002
#define SMS_IOPF_NET      0x00000004
#define SMS_IOPF_USB      0x00000008
#define SMS_IOPF_RMMAN    0x00000010
#define SMS_IOPF_RMMAN2   0x00000020
#define SMS_IOPF_SMB      0x00000040
#define SMS_IOPF_SMBINFO  0x00000080
#define SMS_IOPF_SMBLOGIN 0x00000100
#define SMS_IOPF_NET_UP   0x00000200
#define SMS_IOPF_EURO     0x00000400
#define SMS_IOPF_DEV9_IS  0x00000800
#define SMS_IOPF_UMS      0x00001000
#define SMS_IOPF_MX4SIO   0x00002000
#define SMS_IOPF_ATA      0x00004000
#define SMS_IOPF_ILINK    0x00008000
#define SMS_IOPF_MMCE     0x00010000
/* SMS_IOPReset modes. Historically this argument was a bare 0/1 "afExit" flag; the third
 * value was added for the in-place network-stack switch and the two originals keep their
 * exact meaning, so every existing call site is unaffected.
 *   BOOT    reloads the IOP kernel via s_pUDNL, loads sio2man.
 *   EXEC    the "Exit to -> EXEC0/EXEC1" path: short reset, skips sio2man ( handing over ).
 *   SWITCH  mid-session: short reset like EXEC ( a live bus master must not meet the long
 *           UDNL reload ), but KEEPS sio2man because SMS keeps running afterwards. */
#define SMS_IOPRESET_BOOT    0
#define SMS_IOPRESET_EXEC    1
#define SMS_IOPRESET_SWITCH  2

#define SMS_IOPF_DS34USB  0x00020000
#define SMS_IOPF_DS34BT   0x00040000
#define SMS_IOPF_UDPFS    0x00080000

/* In-place network-stack switch ( R3Z's mechanic ): reboots the IOP without relaunching SMS.
 * Returns 1 if it actually switched, 0 when no stack was live and the mode is simply an
 * autostart preference for the next launch. BDM only. */
int SMS_IOPNetSwitch ( unsigned int );

#define SMS_SIF_CMD_SMB_CONNECT    0
#define SMS_SIF_CMD_USB_CONNECT    1
#define SMS_SIF_CMD_USB_DISCONNECT 2
#define SMS_SIF_CMD_PRINTF         3
#define SMS_SIF_CMD_NETWORK        4
#define SMS_SIF_CMD_POWEROFF       5

extern unsigned int g_IOPFlags;
extern int          g_CfgLate;
extern unsigned int g_Mx4sioMask;
extern unsigned int g_AtaMask;
extern unsigned int g_IlinkMask;
extern unsigned int g_MmceFlags;
extern unsigned int g_UdpfsFlags;

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

void SMS_IOPReset            ( int                                    );
/* DIAGNOSTIC: latch an "E<nn> <what>" breadcrumb on the GS ( EE-side only, so it
 * survives SifExitRpc and the IOP reset ). Names the call ABOUT to run, so the last
 * code left on screen is the one that blocked. See the note in SMS_IOP.c. */
void SMS_ExitCrumb           ( int, const char*                       );
void SMS_IOPInit             ( void                                   );
void SMS_IOPSetSifCmdHandler (  void ( *apFunc ) ( void* ), int aCmd  );
int  SMS_IOPStartNet         ( int                                    );
int  SMS_IOPStartUSB         ( int                                    );
int  SMS_IOPStartMX4SIO      ( int                                    );
int  SMS_IOPStartATA         ( int                                    );
int  SMS_IOPStartILINK       ( int                                    );
int  SMS_IOPStartMMCE        ( int                                    );
int  SMS_IOPStartDS34USB     ( int                                    );
int  SMS_IOPStartDS34BT      ( int                                    );
int  SMS_IOPStartUDPFS       ( int                                    );
int  SMS_IOPNetOwnedBySMB    ( void                                   );
int  SMS_IOPNetOwnedByUDPFS  ( void                                   );
unsigned int SMS_IOPNetLiveMode ( void                             );
int  SMS_IOPStartHDD         ( int                                    );
void SMS_IOPRefreshMass      ( void                                   );
void SMS_IOPSetXLT           ( void                                   );
int  SMS_IOCtl               ( const char*, int, void*                );
void SMS_IOPowerOff          ( void                                   );
int  SMS_HDDMount            ( const char*, const char*, int          );
int  SMS_HDDUMount           ( const char*, const char*               );

int SMS_IOPQueryTotalFreeMemSize ( void );
int SMS_IOPDVDVInit              ( void );

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif  /* __SMS_IOP_H */
