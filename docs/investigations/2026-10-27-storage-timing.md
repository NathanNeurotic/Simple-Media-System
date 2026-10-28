# Storage timing controls for issue #24

The latest #24 report compares player `c942e1c` with SMS 2.9rev4: USB still
stutters, while MX4SIO improves. It predates the merged frame-ownership fixes.
There is no measured basis yet for another global buffer increase or a change
to asynchronous fileXio operations.

## What the diagnostic build measures

Build with `make SMS_DIAG=1 all`, or use the DIAG ELF in a successful Actions
bundle. Preserve that bundle's `build-info.json` and the input MPG's SHA-256.
Capture EE standard output through the tester's usual ps2link/terminal setup.
The new counters print on file/queue destruction, after playback stops; they
do not print, write a log file or perform additional RPCs during a refill.
The existing on-screen E70-stage breadcrumbs remain available, but they do not
show these timing counters. A console without output capture cannot supply
this measurement yet.

A file close emits:

```
[sms-io] path=... chunk=4096 calls=... bytes=... read_ms=... max_read_ms=... errors=... refills=... refill_ms=... max_refill_ms=...
```

- `calls`, `bytes`, `read_ms`, `max_read_ms` and `errors` describe native reads
  inside **synchronous STIO fills**. Errors count negative device results, not
  zero-byte EOF. `chunk` is the configured maximum per fileXio read; zero on the
  legacy non-BDM path does not mean zero-byte requests.
- `refills`, `refill_ms` and `max_refill_ms` cover the whole synchronous batch.
  The BDM 4 KiB path still makes at most eight native reads per refill. UDPFS
  still uses its own read sizing.
- Counters include probing, synchronous startup fills, seeks, and subsequent
  refills for that FileContext. Legacy asynchronous prefetch/completion reads
  are outside these counters. Compare matched playback sessions, not the raw
  totals of BDM and legacy asynchronous paths as if coverage were identical.
- Time uses the existing SMS timer's **8 ms resolution**. Short reads may record
  zero; maxima and totals have quantization error. Values wrap modulo 32 bits.
  A zero-time observation is not evidence of infinite throughput.

Each final queue release emits:

```
[sms-queue] name=video-packets buffer=... capacity=... waits=... wait_ms=... max_wait_ms=...
```

Names distinguish audio/video packet input from decoded audio/video output.
`waits` counts consumer semaphore waits with a nonzero measured duration;
`wait_ms` and `max_wait_ms` summarize that blocking time. This excludes the
optional WaitCB and the queue's internal lock. **It is not an underrun count**:
startup, pause, seek, EOF, decoder scheduling and intentionally empty queues
can all cause a wait. Compare storage and queue results together, using the
same playback actions and elapsed duration.

All counters, clock samples and report strings compile out of ordinary builds.
The diagnostics do not change buffer sizes, chunk limits, RPC mode or queue
semaphore operations.

## Matched device experiment

1. Keep the failing generated MPG unchanged. Record its SHA-256, file size,
   encoder log, video/audio parameters, device model/filesystem and console.
2. Test the merged player first to establish whether #24 remains reproducible.
   Keep exact build identity with the result. MX4SIO compatibility is not a
   2.9rev4 comparison because that build does not support it.
3. On each supported device, play the same interval from a cold open, with a
   fixed duration and no controls; stop once. Capture the file and queue lines.
   Repeat a fixed seek/pause/resume sequence as a separate control.
4. Compare file read/batch latency with packet-queue blocking and observed
   stutter. Record when stutter occurred; whole-session totals cannot correlate
   a single missed frame to a particular read.
5. Only then choose USB-specific request size/read-ahead, queue scheduling or
   decoder/renderer work. Preserve the MX4SIO 4 KiB workaround and UDPFS sizing
   unless a separate device control justifies changing them.

Host regression `tools/test_io_diagnostics.py` checks accounting, EOF/error
classification, low-word timer wrap and preservation of queue wrap behavior.
Existing STIO regressions cover release byte continuity and error recovery.
Neither supplies physical-device throughput or console acceptance.
