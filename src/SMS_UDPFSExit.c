#include "SMS_IOP.h"

#ifdef BDM
#include <fileXio_rpc.h>

/* Must match SMAPCTL_DEVCTL_STOP in iop/SMSUdpfs/smap/src/smap_ctl.c. */
#define SMS_UDPFS_DEVCTL_STOP 0
/* SMS normally uses this full IOP reboot configuration.  The older empty-arg
 * UDPFS workaround reaches BOOTEND on hardware but leaves the rebooted IOP
 * without the RPCINIT handshake that SMS immediately requires. */
#define SMS_UDPFS_RESET_ARGS "rom0:UDNL rom0:EELOADCNF"
#endif

/* The legacy exit implementations live in large, unrelated translation units.
 * Linker wrapping keeps this UDPFS-only lifecycle hook centralized and covers:
 *   - normal Exit to the PS2 browser ( SMS_IOPReset );
 *   - Exit to another ELF ( SMS_EExec );
 *   - console power-off ( SMS_IOPowerOff ).
 */
static int s_StopAttempted;
static int s_UseFullResetOnce;

static void _stop_udpfs_once(void)
{
#ifdef BDM
    int result;

    if (s_StopAttempted || !SMS_IOPNetOwnedByUDPFS())
        return;

    s_StopAttempted = 1;
    SMS_ExitCrumb(3, "UDPFS stop");

    result = fileXioDevctl("smapctl:", SMS_UDPFS_DEVCTL_STOP, NULL, 0, NULL, 0);
    if (result < 0) {
        SMS_ExitCrumb(3, "UDPFS stop fail");
    } else {
        /* Once SMAP is quiescent, restore SMS's normal full reboot.  Keep this
         * one-shot so unrelated or later empty resets retain their semantics. */
        s_UseFullResetOnce = 1;
    }
#endif
}

void __real_SMS_IOPReset(int afExit);
void __wrap_SMS_IOPReset(int afExit)
{
    if (afExit)
        _stop_udpfs_once();
    __real_SMS_IOPReset(afExit);
}

void __real_SMS_EExec(char *path);
void __wrap_SMS_EExec(char *path)
{
    _stop_udpfs_once();
    __real_SMS_EExec(path);
}

void __real_SMS_IOPowerOff(void);
void __wrap_SMS_IOPowerOff(void)
{
    _stop_udpfs_once();
    __real_SMS_IOPowerOff();
}

int __real_SifIopReset(const char *arg, int mode);
int __wrap_SifIopReset(const char *arg, int mode)
{
#ifdef BDM
    if (s_UseFullResetOnce && arg != NULL && arg[0] == '\0') {
        s_UseFullResetOnce = 0;
        return __real_SifIopReset(SMS_UDPFS_RESET_ARGS, mode);
    }
#endif
    return __real_SifIopReset(arg, mode);
}
