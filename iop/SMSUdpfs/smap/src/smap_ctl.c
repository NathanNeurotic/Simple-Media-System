#include <dev9.h>
#include <errno.h>
#include <iomanX.h>
#include <io_common.h>
#include <smapregs.h>
#include <thbase.h>

#include "main.h"
#include "xfer.h"

#define DEV9_SMAP_ALL_INTR_MASK (SMAP_INTR_EMAC3 | SMAP_INTR_RXEND | SMAP_INTR_TXEND | SMAP_INTR_RXDNV | SMAP_INTR_TXDNV)
#define SMAPCTL_STOP_TIMEOUT_MS 2000
#define SMAPCTL_DEVCTL_STOP     0

extern struct SmapDriverData SmapDriverData;

static int s_Stopped;

/*
 * Stop the private UDPFS SMAP without cutting DEV9 power underneath an active
 * transfer.  The regular SMS exit immediately resets the IOP after this call,
 * so the goal is quiescence rather than a restartable driver teardown.
 *
 * Ordering matters:
 *  1. prevent another UDPFS receive callback from being selected;
 *  2. mask new SMAP interrupts;
 *  3. wait for any FIFO DMA already in flight to finish;
 *  4. suspend the sole SMAP worker so it cannot re-enable interrupts;
 *  5. mask once more and disable the MAC.
 */
static int _smap_stop_sync(void)
{
    volatile u8 *smap_regbase = SmapDriverData.smap_regbase;
    volatile u8 *emac3_regbase = SmapDriverData.emac3_regbase;
    int i, result;

    if (s_Stopped)
        return 0;
    if (SmapDriverData.Dev9IntrEventFlag <= 0 || SmapDriverData.IntrHandlerThreadID <= 0)
        return -ENODEV;

    SmapDriverData.NetDevStopFlag = 1;
    smap_register_rx_callback(NULL, 0);
    dev9IntrDisable(DEV9_SMAP_ALL_INTR_MASK);

    for (i = 0; i < SMAPCTL_STOP_TIMEOUT_MS; i++) {
        if (!(SMAP_REG8(SMAP_R_TXFIFO_CTRL) & SMAP_TXFIFO_DMAEN) &&
            !(SMAP_REG8(SMAP_R_RXFIFO_CTRL) & SMAP_RXFIFO_DMAEN))
            break;
        DelayThread(1000);
    }
    if (i == SMAPCTL_STOP_TIMEOUT_MS)
        return -ETIMEDOUT;

    result = SuspendThread(SmapDriverData.IntrHandlerThreadID);
    if (result < 0)
        return result;

    /* The worker may have re-enabled the mask at the end of the iteration that
     * was already running when shutdown began.  It is suspended now, so this
     * second mask is final. */
    dev9IntrDisable(DEV9_SMAP_ALL_INTR_MASK);
    SMAP_EMAC3_SET32(SMAP_R_EMAC3_MODE0, 0);

    SmapDriverData.LinkStatus        = 0;
    SmapDriverData.SmapIsInitialized = 0;
    SmapDriverData.SmapDriverStarted = 0;
    SmapDriverData.NetDevStopFlag    = 0;
    s_Stopped = 1;

    return 0;
}

static int ctl_init(iomanX_iop_device_t *d) { (void)d; return 0; }
static int ctl_deinit(iomanX_iop_device_t *d) { (void)d; return 0; }
static int ctl_format(iomanX_iop_file_t *f, const char *a, const char *b, void *c, int d) { (void)f; (void)a; (void)b; (void)c; (void)d; return -EIO; }
static int ctl_open(iomanX_iop_file_t *f, const char *n, int fl, int m) { (void)f; (void)n; (void)fl; (void)m; return -EIO; }
static int ctl_close(iomanX_iop_file_t *f) { (void)f; return -EIO; }
static int ctl_read(iomanX_iop_file_t *f, void *b, int s) { (void)f; (void)b; (void)s; return -EIO; }
static int ctl_write(iomanX_iop_file_t *f, void *b, int s) { (void)f; (void)b; (void)s; return -EIO; }
static int ctl_lseek(iomanX_iop_file_t *f, int o, int w) { (void)f; (void)o; (void)w; return -EIO; }
static int ctl_ioctl(iomanX_iop_file_t *f, int c, void *d) { (void)f; (void)c; (void)d; return -EIO; }
static int ctl_remove(iomanX_iop_file_t *f, const char *n) { (void)f; (void)n; return -EIO; }
static int ctl_mkdir(iomanX_iop_file_t *f, const char *n, int m) { (void)f; (void)n; (void)m; return -EIO; }
static int ctl_rmdir(iomanX_iop_file_t *f, const char *n) { (void)f; (void)n; return -EIO; }
static int ctl_dopen(iomanX_iop_file_t *f, const char *n) { (void)f; (void)n; return -EIO; }
static int ctl_dclose(iomanX_iop_file_t *f) { (void)f; return -EIO; }
static int ctl_dread(iomanX_iop_file_t *f, iox_dirent_t *d) { (void)f; (void)d; return -EIO; }
static int ctl_getstat(iomanX_iop_file_t *f, const char *n, iox_stat_t *s) { (void)f; (void)n; (void)s; return -EIO; }
static int ctl_chstat(iomanX_iop_file_t *f, const char *n, iox_stat_t *s, unsigned int m) { (void)f; (void)n; (void)s; (void)m; return -EIO; }
static int ctl_rename(iomanX_iop_file_t *f, const char *a, const char *b) { (void)f; (void)a; (void)b; return -EIO; }
static int ctl_chdir(iomanX_iop_file_t *f, const char *n) { (void)f; (void)n; return -EIO; }
static int ctl_sync(iomanX_iop_file_t *f, const char *d, int fl) { (void)f; (void)d; (void)fl; return -EIO; }
static int ctl_mount(iomanX_iop_file_t *f, const char *a, const char *b, int fl, void *arg, int len) { (void)f; (void)a; (void)b; (void)fl; (void)arg; (void)len; return -EIO; }
static int ctl_umount(iomanX_iop_file_t *f, const char *n) { (void)f; (void)n; return -EIO; }
static s64 ctl_lseek64(iomanX_iop_file_t *f, s64 o, int w) { (void)f; (void)o; (void)w; return -EIO; }
static int ctl_devctl(iomanX_iop_file_t *f, const char *n, int cmd, void *arg, unsigned int arglen, void *buf, unsigned int buflen)
{
    (void)f; (void)n; (void)arg; (void)arglen; (void)buf; (void)buflen;
    return cmd == SMAPCTL_DEVCTL_STOP ? _smap_stop_sync() : -EINVAL;
}
static int ctl_symlink(iomanX_iop_file_t *f, const char *a, const char *b) { (void)f; (void)a; (void)b; return -EIO; }
static int ctl_readlink(iomanX_iop_file_t *f, const char *n, char *b, unsigned int l) { (void)f; (void)n; (void)b; (void)l; return -EIO; }
static int ctl_ioctl2(iomanX_iop_file_t *f, int c, void *d, unsigned int dl, void *r, unsigned int rl) { (void)f; (void)c; (void)d; (void)dl; (void)r; (void)rl; return -EIO; }

static iomanX_iop_device_ops_t s_CtlOps = {
    ctl_init, ctl_deinit, ctl_format, ctl_open, ctl_close, ctl_read, ctl_write,
    ctl_lseek, ctl_ioctl, ctl_remove, ctl_mkdir, ctl_rmdir, ctl_dopen,
    ctl_dclose, ctl_dread, ctl_getstat, ctl_chstat, ctl_rename, ctl_chdir,
    ctl_sync, ctl_mount, ctl_umount, ctl_lseek64, ctl_devctl, ctl_symlink,
    ctl_readlink, ctl_ioctl2
};

static const char s_CtlName[] = "smapctl";
static iomanX_iop_device_t s_CtlDevice = {
    s_CtlName,
    IOP_DT_CHAR | IOP_DT_FSEXT,
    1,
    "UDPFS SMAP exit control",
    &s_CtlOps
};

int smap_ctl_init(void)
{
    return AddDrv(&s_CtlDevice);
}
