# MPEG-1/2 decoder color control

This small ELF exercises SMS's actual libmpeg/IPU path without the demuxer,
player queues, GS texture upload or display output. It writes five frames to
`host:`. A successful emulator run is not physical-console or reporter acceptance.
The raw decoder control accepts an elementary stream of at most 8 MiB. The
compressed input and an aligned copy are held in RAM; the feed uses consecutive
DMA transfers of at most 65,535 quadwords each, respecting the 16-bit QWC limit.

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

## Longer GOP controls

Set `CAPTURE_FRAMES` between 1 and 600 to capture multiple GOPs, and use `INPUT`
to keep generated elementary streams outside the source directory. Nondefault
frame counts use separate object directories; specify a unique `OUT` when
comparing different inputs or source revisions. For example:

```sh
make COLOR16=0 CAPTURE_FRAMES=150 INPUT=/work/control-g50.m2v OUT=/work/control-g50-32
ffmpeg -i /work/control-g50.m2v -frames:v 150 -f rawvideo -pix_fmt rgb24 /work/reference.rgb
python compare_series.py /work/control-g50-32/decoded.rgba /work/reference.rgb --width 640 --height 480 --frames 150 --output /work/errors.json
```

`compare_series.py` requires NumPy and reports per-frame absolute errors and
signed RGB bias after untile, as well as aggregate error. Match the encoded
stream, dimensions, frame order and capture count. Record actual picture types
with FFprobe: encoder scene cuts can insert I pictures before the maximum GOP
length. Compare GOP variants against their own independently decoded references,
and relate errors to the number of pictures since the last I picture before
attributing a difference to prediction drift. The host feed check is
`python tools/test_mpeg_color_feed.py`; it checks DMA limits and continuity, not
IPU execution or color correctness.

## Delayed-consumer frame-pool control

`make POOL=1 COLOR16=0` or `make POOL=1 COLOR16=1` builds `pool0/probe.elf` or
`pool1/probe.elf`. Use a stream with at least eight frames, such as the 640x480
control in the investigation report, and compare the five captured frames with
the same `compare.py` command. This separate pool control still captures five
frames and retains its original one-transfer input limit of 1,048,560 bytes.

This control includes the actual `SMS_MPEG12.c`, ring buffer and libmpeg/IPU
code. A producer runs the codec, while a consumer at the same player thread
priority retains each output buffer for 1,000 H-SYNC ticks (about 64 ms at NTSC)
before capturing and releasing it. The consumer's frame hash must remain stable
through that hold. The codec wrapper also counts entries that would target a
renderer-owned buffer. It tracks libmpeg's first/next-picture function changes
without changing the decoder algorithm.

The expected final marker is
`[sms-pool] exit=0 frames=5 produced=5 overwrites=0 changed=0`. Compare baseline
and candidate in separate source copies with identical input and probe code.
Build modes have separate object directories, but switching the codec source
still requires a rebuild. The host regression is
`python tools/test_mpeg12_frame_lifetime.py`.

The pool control replaces framebuffer construction and GS upload with RAM
buffers and a delayed consumer. It reproduces ownership failures, not the
complete player's display, demuxing, device timing or the reporter's exact file.
