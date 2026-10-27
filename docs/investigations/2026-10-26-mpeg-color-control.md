# Decoder-only color control for #20 and #23

This investigation did not reproduce or fix the reported saturated red/green
patches. It adds a repeatable way to separate raw MPEG-1/2 decoding from demux,
player buffers, GS upload and physical video output before choosing another
color change. The forced BT.601/range converter transform was already rejected
by reporter testing and remains reverted.

## Reproduction and identity

- Probe: tracked `tools/mpeg_color_probe/`, using this repository's unchanged
  `src/libmpeg.c` and `src/libmpeg_core.S` (identical to `a99a1a2`).
- Toolchain: existing `ps2dev/ps2dev:v1.0`, legacy EE GCC, two separate object
  directories for COLOR16=0 and COLOR16=1.
- Input: eight progressive 160x128/25 fps `testsrc2` frames, MPEG-2, `-bf 0 -g 6`.
  The probe captures five frames. The exact generation/reference commands are
  in [the probe README](../../tools/mpeg_color_probe/README.md).
- Input SHA-256:
  `89d985392fed2f2b69a90789fe7723b95eed90c6f7a6b9a25ea75472625b2948`.
- Emulator: local PCSX2 2.9.52, separate data path/config and isolated firmware
  working copy. The user's normal emulator configuration was not modified.
  No firmware is included in Git or the player artifact.
- Both final rebuilt probe ELFs completed with `[sms-color] exit=0 frames=5`.
  Logs: ignored `build/color-investigation/final0.log` and `final1.log`.
  Raw pixel files: ignored `tools/mpeg_color_probe/obj0/decoded.rgba` and
  `obj1/decoded16.rgb16`; reference: ignored
  `build/color-investigation/reference.rgb`.

The raw decoder output is **16x16 macroblock ordered**. Viewing it as a flat
image produces false block/color artifacts. `compare.py` untiles it and expands
RGB555 when needed before comparing the same five frames to FFmpeg RGB24.

## Results

| Output | Bytes | Mean absolute channel error (0-255) | Maximum channel error | Channel samples with error >32 |
| --- | ---: | ---: | ---: | ---: |
| 32-bit RGB(A) | 409,600 | 1.352021484375 | 167 | 2,762 |
| 16-bit RGB555, expanded | 204,800 | 3.6519401041666666 | 168 | 2,782 |

These are comparison measurements, not an acceptance threshold. Chroma
upsampling at sharp edges and 5-bit quantization explain why a nonzero maximum
does not by itself identify the reported defect. Inspection of the untiled
first frame showed no large persistent colored patch on this control.

Inspection also found that `_prepare_ipu_context` selects the texture format
and calls the codec's `SMS_HWC_Init` before starting video decoding. The
suggested initial 32/16-bit framebuffer mismatch is therefore not established
by this path. No speculative change to color coefficients, decoder patch
addresses or framebuffer format was made.

## New reporter evidence and pending continuation control

The reporter has now supplied paired player recordings and converter logs in
[#20's follow-up](https://github.com/NathanNeurotic/Simple-Media-System/issues/20#issuecomment-6433977603)
and [#23's follow-up](https://github.com/NathanNeurotic/Simple-Media-System/issues/23#issuecomment-6433809833).
Both describe conversion with the `c942e1c` Windows converter and playback with
the `c942e1c` player versus SMS 2.9 REV4. The attached logs confirm MPEG-2 at
2,500 kbit/s, 640x480 padded YUV420P, 30/1 fps, no B-frames, GOP 50 and stereo
48 kHz MP2 at 192 kbit/s. Encoding and validation completed successfully; the
filter contains no forced matrix/range transform. These logs do not include
source probe metadata, output hashes or the converted MPG bytes. The paired
recordings therefore are not yet a verified same-byte comparison; they have
not been analyzed frame by frame in this follow-up.

The existing 160x128 probe covers only **80 macroblocks**. The reported 640x480
output has **1,200**: `_MPEG_CSCImage` in `src/libmpeg_core.S` limits its first
color-conversion batch to 1,023, then `_mpeg_dmac_handler` handles the remainder.
The successful small probe did not exercise that continuation. A useful next
control is 512x480 (**960 macroblocks**) versus 640x480, each in both 16-bit and
32-bit modes, using the reported frame rate/GOP/B-frame settings.

The following synthetic controls and their PS2/emulator runs **have not been
executed** in this follow-up. Generate from the repository root, then run each
elementary stream through the existing probe workflow in an isolated build;
preserve each input, ELF, log and pixel capture separately:

```sh
mkdir -p build/color-continuation
for probe_width in 512 640; do
  ffmpeg -y -f lavfi -i "testsrc2=size=${probe_width}x480:rate=30" \
    -frames:v 8 -an -c:v mpeg2video -b:v 2500k -r 30/1 -bf 0 -g 50 \
    -pix_fmt yuv420p -f mpeg2video "build/color-continuation/${probe_width}.m2v"
  ffmpeg -y -i "build/color-continuation/${probe_width}.m2v" -frames:v 5 \
    -f rawvideo -pix_fmt rgb24 "build/color-continuation/${probe_width}.rgb"
done
```

If both dimensions pass the decoder-only comparison, repeat with the same
elementary streams remuxed into MPEG-PS through the full player to include
packet queues, GS upload and display. A size-dependent failure would narrow
the investigation; it would not alone prove the continuation is its cause.
No decoder source change or color fix is established by this audit.

## What remains

Obtain the exact converted failing MPG and source metadata; converter logs
are now available in the follow-ups above.
First host-decode that same MPG, then compare identical bytes in 2.9rev4 and
the candidate with fixed console video mode, 16/32-bit setting, cable/display
and transport. Use the probe on an extracted elementary stream as a separate
control, recording whether timestamps/sequence changes/interlacing matter.
Keep byte identity when extracting; do not re-encode away the failure.

The probe excludes the full demuxer, player packet queues, framebuffer upload
chains, GS display, physical PS2 IPU and device timing. A successful synthetic
progressive emulator decode neither establishes full-player correctness nor
rules out a content-dependent decoder defect. Issues #20/#23 remain open.
