# Converter file-size review — 2026-10-30

Reviewed against master `34000cf5b38b382389850d005eb958a1d049b91b`, after
PRs #34–#42 merged. This review changes converter behavior and file-size
guidance. It does not change the player's file APIs or establish new console
acceptance for the nine open reports.

## Demonstrated mismatch

The converter's size monitor and validator used a threshold ten MiB below
4 GiB, and the queue displayed 3.8/4.0 GiB warnings. A structurally valid output
between 2 and approximately 3.99 GiB could therefore pass the size check.

That is not a portable seekable-file target for the current player:

- `STIO_Seek` in `src/SMS_FileContext.c` passes its unsigned position to
  `fioLseek` or `fileXioLseek`. File-size discovery also uses these 32-bit APIs;
  this path does not call their 64-bit counterparts.
- The bundled SMB driver demonstrates the signed-offset consequence directly:
  `smb_lseek` in `iop/smbman-sms/src/smb_fio.c` accepts `int pos` and passes it
  to `smb_lseek64` as a signed value. Positions at or above `0x80000000` do not
  arrive as the intended positive absolute positions.
- MOV's normal `ReadPacket` path seeks to sample positions, and AVI's index
  and playback code also performs absolute seeks. This is relevant to ordinary
  playback, not only a user's seek command.

These source facts support a conservative portable maximum of
`0x7fffffff` bytes (2,147,483,647 bytes, just below 2 GiB). They do not prove
that every driver or sequential-only path rejects every larger file. exFAT's
filesystem capacity does not widen the APIs SMS calls.

## Converter behavior

The validator uses the exact portable maximum. The active-encode monitor has
a separate ten-MiB headroom threshold below that maximum. Projected-size
messages use the largest queue item, distinguish the warning and stop
thresholds, and do not treat several small outputs as one large file.

Monitoring is checked between FFmpeg progress reads; it is not a byte-accurate
write cap. Post-conversion validation remains necessary when an output grows
or finishes between checks. Projected bitrate sizes omit some muxing overhead.
The final size check prevents an oversized completed output from being
promoted even when the active monitor did not observe the crossing.

The maintained manual and README use the same portable target. Historical
OpenDML/changelog reports are retained as history; they do not qualify every
current storage path.

## Validation scope

The focused converter regression exercises boundary sizes without allocating
multi-GiB media, plus the real queue-warning and worker control paths. It runs
with both modern and Windows 7 converter unit jobs. The ordinary converter,
multi-track, geometry and real-FFmpeg preset suites remain the integration
checks. Host results establish the exercised policy and control flow, not
physical-console playback near the boundary.
