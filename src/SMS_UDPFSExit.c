#include "SMS_IOP.h"

#ifdef BDM
#include <fileXio_rpc.h>
#include <iopcontrol.h>
#include <sifrpc.h>

/* Must match SMAPCTL_DEVCTL_STOP in iop/SMSUdpfs/smap/src/smap_ctl.c. */
#define SMS_UDPFS_DEVCTL_STOP 0
/* SMS normally uses this full IOP reboot configuration. */
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
static int s_RpcInitCount;
static int s_AfterRpc2;
static int s_Sync2Announced;

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
        SMS_ExitCrumb(29, "Reset full");
        return __real_SifIopReset(SMS_UDPFS_RESET_ARGS, mode);
    }
#endif
    return __real_SifIopReset(arg, mode);
}

void __real_SifInitRpc(int mode);
void __wrap_SifInitRpc(int mode)
{
    if (!s_StopAttempted) {
        __real_SifInitRpc(mode);
        return;
    }

    ++s_RpcInitCount;

    /* Call 1 is SMS_IOPReset's pre-reset SifInitRpc at E04.  E10 covered every
     * operation through E11, so split the two later RPC calls explicitly. */
    if (s_RpcInitCount == 2) {
        SMS_ExitCrumb(30, "RPC2 enter");
        __real_SifInitRpc(mode);
        SMS_ExitCrumb(31, "RPC2 done");
        s_AfterRpc2 = 1;
        return;
    }

    if (s_RpcInitCount == 3) {
        SMS_ExitCrumb(34, "RPC3 enter");
        __real_SifInitRpc(mode);
        SMS_ExitCrumb(35, "RPC3 done");
        return;
    }

    __real_SifInitRpc(mode);
}

int __real_SifIopSync(void);
int __wrap_SifIopSync(void)
{
    int result;

    if (!s_AfterRpc2)
        return __real_SifIopSync();

    if (!s_Sync2Announced) {
        SMS_ExitCrumb(32, "Sync2 enter");
        s_Sync2Announced = 1;
    }

    result = __real_SifIopSync();
    if (result) {
        SMS_ExitCrumb(33, "Sync2 done");
        s_AfterRpc2 = 0;
    }

    return result;
}
