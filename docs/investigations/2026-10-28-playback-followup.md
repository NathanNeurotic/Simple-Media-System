# Playback follow-up: confirmed USB reads and longer decoder controls

This continuation starts from merged master `26c3d2e898e734d0dc18ffdadac933be096f71eb`.
It addresses the remaining USB-read and GOP experiments without claiming that
the reported console playback defects have passed acceptance.

## Implemented storage changes

`STIO_InitFileContext` queries `bdmfs_fatfs` ioctl2 command 3 on a successfully
opened `mass` file. Only the packed driver-name return value `usb\0` enables
64 KiB reads, with one 64 KiB synchronous refill. The browser's mass-unit icon
mask is not used: newly mounted devices can inherit the global MX4SIO flag even
when they use another transport.

Unknown, unsupported and non-USB mass drivers keep 4 KiB requests and a 32 KiB
refill limit. MMCE and SMB retain that conservative behavior. UDPFS retains its
independent 64 KiB requests and existing refill behavior; legacy fio is unchanged.
Diagnostic summaries include both request chunk and refill limit. Allocation
failure for the stored path now follows the existing cleanup path instead of
passing a null pointer to `strcpy`.

The query is supported by the committed `irx/bdmfs_fatfs.irx.xz`, whose ioctl2
switch case 3 returns the mounted block device's driver-name word. Upstream ABI
references are [the shared ioctl definitions](https://github.com/ps2dev/ps2sdk/blob/master/common/include/usbhdfsd-common.h),
[the FAT driver](https://github.com/ps2dev/ps2sdk/blob/master/iop/fs/bdmfs_fatfs/src/fs_driver.c)
and [the USB mass driver](https://github.com/ps2dev/ps2sdk/blob/master/iop/usb/usbmass_bd/src/usb_mass.c).

`tools/test_stio_device_reads.py` exercises the actual production open, refill
and destruction code with controlled device calls. It covers driver selection,
conservative fallback, legacy routing, byte continuity, refill bounds, a single
query per open, failed open and all four allocation failures. The USB selection
and path-allocation controls failed before the corresponding production fixes.
Existing refill, diagnostics and stream-failure fixtures include the private
refill-limit field. These are host controls; USB timing and console acceptance
remain unmeasured.

## Existing audio probe disposition

The old scratch audio-probe concern is already addressed in current master.
`_fill_audio_parameters` restores `m_CurPos`, checks codec-specific minimum
headers and short reads, and uses a checked heap buffer. The existing
`tools/test_mpeg_ps_probe.py` passed during this continuation. No duplicate
production audio patch is needed for that historical scratch result.

## Decoder investigation checkpoint

The tracked `tools/mpeg_color_probe` now supports 1 to 600 raw decoder frames
and consecutive DMA transfers for inputs up to 8 MiB. Its new host feed control
passes checks for QWC bounds, exact addresses, byte continuity and EOF. The
optional NumPy series comparator reports per-frame errors and signed RGB bias.
Independent layout controls passed for RGBA and RGB555, a known red-channel
error and truncated input rejection.

Longer GOP 50 and GOP 15 inputs have been generated from the reporter's Samsung
source at two offsets. Native builds and emulator comparisons are still in
progress at this checkpoint. The proposed GOP-default change is not included:
the claim that prediction drift causes the reported red/green artifacts still
requires evidence. A successful isolated decoder run cannot validate the
player's GS path, storage timing or physical-console behavior.

## Remaining validation at this checkpoint

- Finish the full host workflow, pinned GCC 3.2.3 player variants and GP checks.
- Run the longer GOP controls, retain exact input hashes and per-frame results,
  and update this report with the measured outcome.
- Verify current-head CI and the manifest of its downloadable player artifact.
- Keep physical-console testing on hold until the user resumes it. Do not close
  the reporter issues based on host or emulator results alone.
