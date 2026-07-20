#include "SMS_IOP.h"

#ifdef BDM
#include <fileXio_rpc.h>

/* Must match SMAPCTL_DEVCTL_STOP in iop/SMSUdpfs/smap/src/smap_ctl.c. */
#define SMS_UDPFS_DEVCTL_STOP 0
#endif

/* The legacy exit implementations live in large, unrelated translation units.
 * Linker wrapping keeps this UDPFS-only lifecycle hook centralized and covers:
 *   - normal Exit to the PS2 browser ( SMS_IOPReset );
 *   - Exit to another ELF ( SMS_EExec );
 *   - console power-off ( SMS_IOPowerOff ).
 */
static int s_StopAttempted;

static void _stop_udpfs_once(void)
{
#ifdef BDM
    int result;

    if (s_StopAttempted || !SMS_IOPNetOwnedByUDPFS())
        return;

    s_StopAttempted = 1;
    SMS_ExitCrumb(3, "UDPFS stop");

    result = fileXioDevctl("smapctl:", SMS_UDPFS_DEVCTL_STOP, NULL, 0, NULL, 0);
    if (result < 0)
        SMS_ExitCrumb(3, "UDPFS stop fail");
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
