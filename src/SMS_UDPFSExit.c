#include "SMS_IOP.h"

#ifdef BDM
#include <fileXio_rpc.h>
#include <iopcontrol.h>
#include <kernel.h>
#include <sbv_patches.h>
#include <sifrpc.h>

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
static int s_RpcInitCount;
static int s_DiagPhase;
static int s_FlushCount;
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

void __real_sceSifInitRpc(int mode);
void __wrap_sceSifInitRpc(int mode)
{
    if (!s_StopAttempted) {
        __real_sceSifInitRpc(mode);
        return;
    }

    ++s_RpcInitCount;

    /* Call 1 is SMS_IOPReset's pre-reset SifInitRpc at E04. */
    if (s_RpcInitCount == 2) {
        SMS_ExitCrumb(30, "RPC2 enter");
        __real_sceSifInitRpc(mode);
        SMS_ExitCrumb(31, "RPC2 done");
        s_DiagPhase = 1;
        return;
    }

    if (s_RpcInitCount == 3) {
        SMS_ExitCrumb(44, "RPC3 enter");
        __real_sceSifInitRpc(mode);
        SMS_ExitCrumb(45, "RPC3 done");
        s_DiagPhase = 8;
        return;
    }

    __real_sceSifInitRpc(mode);
}

int __real_sbv_patch_enable_lmb(void);
int __wrap_sbv_patch_enable_lmb(void)
{
    int result;

    if (s_DiagPhase == 1) {
        SMS_ExitCrumb(32, "LMB1 enter");
        result = __real_sbv_patch_enable_lmb();
        SMS_ExitCrumb(33, "LMB1 done");
        s_DiagPhase = 2;
        return result;
    }

    if (s_DiagPhase == 8) {
        SMS_ExitCrumb(46, "LMB2 enter");
        result = __real_sbv_patch_enable_lmb();
        SMS_ExitCrumb(47, "LMB2 done");
        s_DiagPhase = 9;
        return result;
    }

    return __real_sbv_patch_enable_lmb();
}

int __real_sbv_patch_disable_prefix_check(void);
int __wrap_sbv_patch_disable_prefix_check(void)
{
    int result;

    if (s_DiagPhase == 2) {
        SMS_ExitCrumb(34, "Prefix1 enter");
        result = __real_sbv_patch_disable_prefix_check();
        SMS_ExitCrumb(35, "Prefix1 done");
        s_DiagPhase = 3;
        return result;
    }

    if (s_DiagPhase == 9) {
        SMS_ExitCrumb(48, "Prefix2 enter");
        result = __real_sbv_patch_disable_prefix_check();
        SMS_ExitCrumb(49, "Prefix2 done");
        s_DiagPhase = 10;
        return result;
    }

    return __real_sbv_patch_disable_prefix_check();
}

int __real_sbv_patch_fileio(void);
int __wrap_sbv_patch_fileio(void)
{
    int result;

    if (s_DiagPhase == 3) {
        SMS_ExitCrumb(36, "Fileio enter");
        result = __real_sbv_patch_fileio();
        SMS_ExitCrumb(37, "Fileio done");
        s_DiagPhase = 4;
        return result;
    }

    return __real_sbv_patch_fileio();
}

void __real_FlushCache(int operation);
void __wrap_FlushCache(int operation)
{
    if (s_DiagPhase == 4 && s_FlushCount < 2) {
        SMS_ExitCrumb(s_FlushCount == 0 ? 38 : 40,
                      s_FlushCount == 0 ? "Flush0 enter" : "Flush2 enter");
        __real_FlushCache(operation);
        SMS_ExitCrumb(s_FlushCount == 0 ? 39 : 41,
                      s_FlushCount == 0 ? "Flush0 done" : "Flush2 done");
        ++s_FlushCount;
        if (s_FlushCount == 2)
            s_DiagPhase = 5;
        return;
    }

    __real_FlushCache(operation);
}

int __real_SifIopSync(void);
int __wrap_SifIopSync(void)
{
    int result;

    if (s_DiagPhase != 5)
        return __real_SifIopSync();

    if (!s_Sync2Announced) {
        SMS_ExitCrumb(42, "Sync2 enter");
        s_Sync2Announced = 1;
    }

    result = __real_SifIopSync();
    if (result) {
        SMS_ExitCrumb(43, "Sync2 done");
        s_DiagPhase = 6;
    }

    return result;
}
