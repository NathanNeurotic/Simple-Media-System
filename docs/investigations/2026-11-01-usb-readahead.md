# USB read-ahead for issue #24 (2026-11-01)

**Status: NOT TESTED on console.** Host tests and PS2 builds pass. The
mechanism below is inferred from code, not measured on hardware.

## Suspected cause

SMS 2.9rev4 streamed with two asynchronous fio buffers (`STIO_FillStm`): the IOP
read the next buffer while the EE parsed the current one.

The fork's mass:/fileXio path (`m_fXio`) reads synchronously into one buffer,
bounded to 64 KiB per refill for driver-confirmed USB. While the parser consumes
that buffer the IOP is idle, and the next read only starts once the main thread
(priority 64, the same as the decoders) is scheduled again. The read and the
parse no longer overlap, which fits "worse than 2.9rev4 on USB" in #24.

## Change

For files whose BDM driver-name ioctl returns `usb`, `STIO_Stream` starts one
persistent EE helper thread at priority `SMS_THREAD_PRIORITY - 2`:

- The existing stream buffer is split into up to four 64 KiB slots (192 KiB
  gives three). The helper keeps every free slot queued with the same blocking
  `fileXioRead` calls, so the read-size bound from #35 is unchanged.
- Free/Full counting semaphores hand slots between the helper and `Fill`. The
  global fileXio NOWAIT mode is never used, and the fileXio client already
  serializes calls, so seeks and other I/O stay safe.
- Only one file reads ahead at a time. Seek, pause-time streaming, buffer
  reallocation and close stop the helper first and wait for any in-flight read.
- MX4SIO, MMCE, SMB, UDPFS and unconfirmed BDM devices are unchanged. Any
  semaphore or thread setup failure falls back to the existing `STIO_Fill`.

## Verification done

- `tools/test_stio_usb_readahead.py`: 12 cases (continuity, overlap, seeks,
  stop/restart, mid-file errors, single owner, non-USB, setup failures, teardown
  with a read in flight, EOF at slot boundaries, buffer reallocation) under
  ASan/UBSan, a DIAG build and ThreadSanitizer. Planted bugs (missing stop,
  early slot release, missing end flag, wrong slot index) are all caught.
- All host tests in `compilation.yml` pass. PS2 builds (standard, `SMS_DIAG=1`,
  `BDM=0`) compile with no new warnings.

## Console A/B test

Use the same USB file that stutters, on the same stick and port.

| Build | What to record |
|---|---|
| Current rolling | Does it stutter? Where (time in file)? |
| This PR, standard | Same file, same spot |
| This PR, `SMS_DIAG=1` | `[sms-io]` lines: `refill_ms`, `max_refill_ms`, read count |
| Rolling, `SMS_DIAG=1` (optional) | Same lines, for comparison |

Also worth one pass each: seeking back and forth, pause/resume, and playing a
second file after the first, to confirm the helper stops and restarts cleanly.
A file on MX4SIO, MMCE or SMB should behave exactly as on rolling.
