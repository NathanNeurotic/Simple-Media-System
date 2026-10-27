# MPEG color controls and frame-buffer ownership for #20 and #23

The raw decoder controls did not reproduce the reported saturated red/green
patches. A subsequent delayed-consumer experiment reproduced a player-codec
buffer ownership defect and now has a tested fix candidate. That result does
not establish acceptance of the reporter's exact files. The forced BT.601/range
converter transform was already rejected by reporter testing and remains reverted.

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

## New reporter evidence and completed continuation control

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

The following controls were generated and run on 2026-10-27 using FFmpeg
`n8.1.3-9-g29e619e767-20260930`, the existing GCC toolchain and the isolated
PCSX2 2.9.52 setup. Input, ELF, log and capture files were retained separately
under ignored `build/color-continuation/`:

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

| Raw decoder control | Macroblocks | Exit / frames | Mean absolute channel error |
| --- | ---: | --- | ---: |
| 512x480, 32-bit | 960 | 0 / 5 | 1.2013096788194444 |
| 512x480, 16-bit | 960 | 0 / 5 | 3.5627208116319444 |
| 640x480, 32-bit | 1,200 | 0 / 5 | 1.083361545138889 |
| 640x480, 16-bit | 1,200 | 0 / 5 | 3.4660401475694442 |

Input SHA-256 values:

- 512x480: `e8826fa1ba33066781a4af49b60238c016d1824dded71e2faa5e89dbbe9fdc39`.
- 640x480: `6ba28c1a7e61749883ffc7b68abebcd340b7abafacdf5d9a05ef00bd22b9767a`.

All four decoded the same five frames as the host reference. The larger case
exercised the conversion continuation successfully in emulation. This does not
rule out a content-dependent or physical-console fault, but supplies no basis
for changing the continuation code or color coefficients on this control.

## Reproduced frame-pool ownership defect and fix candidate

`SMS_MPEG12.c` cycled through three RGB buffers and called `MPEG_Picture` before
checking whether the target frame was still queued or uploading. A free output
pointer-queue slot does not prove the referenced pixel buffer is free. The
existing MPEG-4 path already waits for `m_FrameType < 0`, which the GIF completion
handler publishes after upload. MPEG-1/2 omitted that ownership check. Its
sequence-size callback also destroyed every RGB buffer without draining owners.

The candidate waits for the selected allocated frame before decoding and waits
for all allocated frames before a size-changing reallocation. Initial NULL
buffers are skipped, so first-sequence allocation remains possible. It uses the
existing player thread priority and renderer completion flag. No color matrix,
IPU conversion command or storage protocol was changed.

`tools/test_mpeg12_frame_lifetime.py` compiles the actual callback/decode functions
under ASan/UBSan with controlled renderer/decoder stubs. At baseline `8547573`,
separate tests fail because the decoder targets an owned frame and resize frees
an owned frame. The candidate passes delayed-release, resize-drain, first
allocation, same-size sequence, EOF and pool-wrap cases. Target disassembly
confirms the completion flag is reloaded on every wait iteration.

The new `POOL=1` probe then exercises the actual codec, ring buffer, libmpeg,
IPU conversion and kernel scheduling in PCSX2. Its consumer has the same thread
priority as the producer and holds each RGB buffer for 1,000 H-SYNC ticks before
capturing/releasing it. Framebuffer construction and GS upload are RAM/test
replacements. Baseline and candidate use identical probe code and the 640x480
input above. Both complete all five frames:

| Codec source / output | Targets still owned | Held buffers whose hash changed | Exit | Mean absolute channel error |
| --- | ---: | ---: | ---: | ---: |
| `8547573`, 32-bit | 2 | 2 | 1 | 4.123024956597222 |
| Candidate, 32-bit | 0 | 0 | 0 | 1.083361545138889 |
| `8547573`, 16-bit | 2 | 2 | 1 | 6.344892144097222 |
| Candidate, 16-bit | 0 | 0 | 0 | 3.4660401475694442 |

This is an observed overwrite of held pixel buffers. The candidate restores
the same five-frame output as the raw decoder controls under this scheduling
stress. It is a concrete lifetime fix, while causation of the reporter's exact
color symptoms still needs their file and affected-console test.

Final pool-control ELF SHA-256 values and logs are retained alongside each
capture in `build/color-continuation/pool-{baseline,fixed}/mode{0,1}/`.
The final stress logs use the 1,000-tick alarm; earlier exploratory logs that
used voluntary yields alone are not the final delayed-consumer result.

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
rules out a content-dependent decoder defect. Test the ownership candidate with
the actual player, including pause/resume, seek, stop and sequence-size changes;
the change deliberately constrains producer/render scheduling. Issues #20/#23
remain open for that acceptance work.
