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

Longer controls exposed two concrete motion-compensation defects:

1. Chroma vector derivation used shifts/low bits of the luma vector. MPEG-1/2
   4:2:0 requires signed division by two toward zero, which differs for negative
   odd vectors. Luma and chroma now carry separate coordinates within the
   existing 40-byte motion layout. The MMI path handles a chroma start that
   advances into the second transferred macroblock row.
2. Both put and average chroma XY routines set the one-byte neighbor shift
   before aligning their first row to the requested X coordinate. Moving that
   shift after the initial alignment fixes an independent bilinear-prediction
   error, including for nonnegative vectors.

The [FFmpeg MPEG motion implementation](https://github.com/FFmpeg/FFmpeg/blob/n9.0.2/libavcodec/mpegvideo_motion.c)
provides an independent signed-vector reference. Neither fix changes IPU IDCT,
color-conversion coefficients or the converter's GOP defaults.

The new native [motion oracle](../../tools/mpeg_motion_probe/README.md) includes
the actual vector setup, transfers reference blocks through the production SPR
DMA routine, and executes the actual MMI interpolation. Baseline `1422165` fails
the first negative-vector control (-17,-17); a separate nonnegative control
fails at (2,2). The corrected decoder passes 12,250 cases and 2,822,400 scalar
pixel comparisons, including frame/field and put/average modes. The host
production-vector control passes 171,610 cases under ASan/UBSan. Native layout
checks preserve the assembly offsets and the existing GP data footprint.

### Unchanged-bitstream decoder comparisons

Environment: PCSX2 2.9.147 with isolated settings, host filesystem output and
the null renderer; pinned GCC 3.2.3 probes; FFmpeg 9.0.2 host reference. The source
SHA-256 is `e412c2332ba448eab546bd6dafdc96988cf2d23173df548faeb1ff1f1ab80858`.
Two 150-frame intervals start at 0 and 30 seconds. These use the actual MPEG-2
converter profile with audio disabled, explicit upscaling allowed for a 640x480
canvas, zero B-frames, and GOP 50/15. Explicit upscaling matches the historical
canvas; current no-upscale sizing would choose 480x360 for this 640x360 source.
Each baseline/fix pair decodes identical elementary-stream bytes, and each
GOP variant has its own independently decoded RGB24 reference.

| Offset / GOP | ES SHA-256 | Bytes |
| --- | --- | ---: |
| 0 s / 50 | 64e8ca438ed137272bc609a0299b10317cc896d8ba9f7b9bf2db8d90c67d2d16 | 1,713,410 |
| 0 s / 15 | 819b72e4fc4dae191513f4ba52af06327360e4c5100163b6d2ef879acccd7d0f | 1,722,136 |
| 30 s / 50 | d35a69d0e1ee331d99712ef3bad4d982ef69f11b560a5318b434ae42ee6eec21 | 1,369,045 |
| 30 s / 15 | 09d10b1250540bbd36993fe758c5040635af0afd2b22d5a04ee4a93c05093798 | 1,492,063 |

| Offset / GOP | Mode | Mean absolute channel error, before -> after | Maximum channel error, before -> after |
| --- | ---: | ---: | ---: |
| 0 s / 50 | 32 | 5.7285 -> 1.9786 | 255 -> 41 |
| 0 s / 50 | 16 | 6.6345 -> 3.2783 | 255 -> 40 |
| 0 s / 15 | 32 | 3.7268 -> 1.9504 | 255 -> 43 |
| 0 s / 15 | 16 | 4.7966 -> 3.2656 | 255 -> 40 |
| 30 s / 50 | 32 | 2.5779 -> 1.3095 | 146 -> 39 |
| 30 s / 50 | 16 | 3.9499 -> 3.2171 | 146 -> 48 |
| 30 s / 15 | 32 | 1.6054 -> 1.2128 | 106 -> 24 |
| 30 s / 15 | 16 | 3.3078 -> 3.1700 | 115 -> 34 |

All 16 baseline/candidate runs capture 150 frames with two DMA transfers.
The first GOP 50 interval's 32-bit channel differences above 32 levels fall
from 5,650,343 to 5,019; the second falls from 904,114 to 112. Residual errors
include host/IPU color conversion, chroma reconstruction and RGB555 quantization;
the test does not establish which residual comes from each stage.

The earlier GOP 15 experiment reduced the accumulated error before the decoder
fix, but it did not identify these defects. The GOP defaults remain unchanged
to preserve the current compression/seek tradeoff. These measurements do not
prove that every reporter color symptom has the same cause, and cannot validate
the player's full GS path, device timing or physical-console behavior.

## Remaining validation at this checkpoint

- All 28 host scripts pass. Recheck the pinned player variants and GP checks
  after the decoder correction; the initial USB-only variants already passed.
- Consolidate input hashes/per-frame metrics and add MPEG-1 and interlaced/B-frame
  bitstream controls in addition to the native interpolation oracle.
- Verify current-head CI and the manifest of its downloadable player artifact.
- Keep physical-console testing on hold until the user resumes it. Do not close
  the reporter issues based on host or emulator results alone.
