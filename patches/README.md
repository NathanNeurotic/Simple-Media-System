# IRX patches

`mx4sio_bd-sms.patch` is applied to the PS2SDK source `iop/sio/mx4sio_bd/` before
building the `mx4sio_bd.irx` that this project embeds (committed input: `irx/mx4sio_bd.irx.xz`). It is
kept here so the embedded binary is reproducible and reviewable.

See `../MX4SIO_SOLUTION.md` for the full root-cause analysis. In short the patch:

- **batches** multi-block reads (`SPISD_MAX_BATCH`) and advances the sector on
  partial reads,
- replaces the no-timeout `WaitEventFlag` completion wait with a bounded
  `PollEventFlag` **watchdog**, and
- **bounds every SIO2 "transfer complete" busy-wait** (`mx_sio2_xfer_done`),
  including the ones inside the DMA ISR, so a stalled transfer can never lock the
  IOP.

It also adds `I_PollEventFlag` to the module's `imports.lst`.

## Rebuild

```sh
# in a ps2sdk source checkout:
patch -p1 < mx4sio_bd-sms.patch
cd iop/sio/mx4sio_bd && PS2SDKSRC=<ps2sdk-root> make   # -> irx/mx4sio_bd.irx
# Preserve module/toolchain provenance, then recompress the result as
# this project's irx/mx4sio_bd.irx.xz and rebuild in a clean build directory.
# A loose .irx does not replace the committed compressed input.
```

Normal player builds use the committed compressed modules. See the [current build/ABI notes](https://nathanneurotic.github.io/Simple-Media-System/developers.html#drivers) before changing imports or substituting SDK binaries.
