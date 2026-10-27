# MPEG-1/2 decoder color control

This small ELF exercises SMS's actual libmpeg/IPU path without the demuxer,
player queues, GS texture upload or display output. It writes five frames to
`host:`. A successful emulator run is not physical-console or reporter acceptance.
Use an elementary stream of at most 2 MiB; the compressed input is held in RAM.

Generate a reproducible progressive control in this directory:

```sh
ffmpeg -f lavfi -i testsrc2=size=160x128:rate=25 -frames:v 8 -c:v mpeg2video -bf 0 -g 6 -f mpeg2video probe.m2v
make COLOR16=0
make COLOR16=1
ffmpeg -i probe.m2v -frames:v 5 -f rawvideo -pix_fmt rgb24 reference.rgb
```

Use the same ps2dev v1.0 toolchain as the player. Separate object directories
prevent a change in COLOR16 from reusing the other mode's objects. Run each ELF
with host filesystem support and retain its log and output. Copy `probe.m2v` and
the reference into `obj0/` and `obj1/` if keeping each run's inputs together.

```sh
python compare.py obj0/decoded.rgba reference.rgb --width 160 --height 128
python compare.py obj1/decoded16.rgb16 reference.rgb --width 160 --height 128 --mode 16
```

The raw data is **16x16 macroblock ordered**, not a linear image. The comparator
untiles it and expands RGB555 for 16-bit output. Use the aligned dimensions from
the sequence log; for a non-aligned source, supply its actual dimensions with
`--crop-width` / `--crop-height`. Compare the same frames and bitstream. Chroma
upsampling and RGB555 quantization can produce differences at edges; a nonzero
maximum error alone is not a color-defect reproduction.

PCSX2's [documented CLI](https://pcsx2.net/docs/advanced/cli/) supports a separate
data path, log file and ELF launch. Keep the test configuration separate from the
user's configuration. On the tested Windows build, the specified data path
contains a nested `PCSX2/` directory. This repository includes no BIOS files.

Generated streams, binaries, objects and pixel captures are ignored. Record the
player/probe source revision, input hash, mode, decoder dimensions, emulator
version or real-console model, and full final status line with each result.
