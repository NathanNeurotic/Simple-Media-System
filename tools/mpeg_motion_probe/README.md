# Native MPEG motion-compensation oracle

This ELF includes the actual `src/libmpeg.c` reference setup and links the actual
`libmpeg_core.S`. It transfers reference macroblocks to scratchpad with the
production DMA routine, runs the MMI luma/chroma interpolation functions, and
compares every result to an independent scalar bilinear predictor.

```sh
make OUT=/work/motion-fixed
```

Use the player's pinned `ps2dev/ps2dev:v1.0` toolchain. Run `probe.elf` in an
isolated PCSX2 data path with EE console logging, or on an explicitly authorized
console. It logs a mismatch and stops at the first failing pixel. The successful
final marker is:

```text
[sms-motion] exit=0 cases=12250 pixels=2822400 failures=0
```

Cases cover every pair of signed vectors from -17 through 17, frame and field
prediction, both source/destination field placements and put/average operations.
The data spans macroblock boundaries and exercises both Cb and Cr. Coordinates
are interior to the reference image; this does not test malformed out-of-frame
vectors, bitstream parsing, IPU reconstruction, GS display or storage timing.
The output's `pixels` count includes checked luma and chroma samples.

Use `VECTOR_MIN=0` with a unique `OUT` for a nonnegative-vector control.
`DECODER_SOURCE` and `DECODER_CORE` can select saved baseline source files;
keep the baseline C file beside its matching `libmpeg_internal.h` and
`libmpeg.h`. Rebuild in separate output directories for each source/mode.
Against baseline `1422165`, the full range fails on (-17,-17) chroma prediction,
and the nonnegative range independently fails on (2,2) chroma XY alignment.

The host companion `python tools/test_mpeg_motion_vectors.py` checks production
reference setup against independently derived coordinates under sanitizers,
with only the MMI clamp replaced by scalar code. It does not execute DMA/MMI;
the native oracle supplies that evidence. Compile-time layout checks preserve
the 40-byte motion structure and its assembly offsets.
