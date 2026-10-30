# Media Converter

Convert modern source files into SMS's supported video and audio formats. The converter checks output structure and limits; final playback still depends on the file, player build and storage source.

[Download Windows, Windows 7 or Linux builds](https://nathanneurotic.github.io/Simple-Media-System/releases.html#whats-in-a-release). The October controls are in rolling development builds and are absent from the July stable player/package.

## Prepare a file for SMS

1. Install FFmpeg and FFprobe from a trusted distribution, or let the application's setup helper locate them. A Windows executable does not guarantee they are installed. Builds need the selected encoders; text burn-in also requires the subtitles/libass filter.
2. Add a source file, choose an output folder and start with a conservative preset. Keep a short sample for your first console test.
3. Use Advanced settings only for deliberate changes. The settings badge tells you whether settings differ from the preset; it is not a hardware certification badge.
4. Read the conversion/validation result and retain the log if anything fails. Test the output in SMS before converting a large library.

H.264, H.265, VP9 and AV1 are not SMS video decoders. An MP4 container may hold either supported `mp4v` or unsupported modern video; conversion changes the codec, not just the filename.

## Preset choices

| Preset family | Default target | Tradeoff |
| --- | --- | --- |
| Xvid-compatible AVI | 640×480, 30 fps; 1.5 Mbps video + 128 kbps MP3. | Software MPEG-4 decode with hardware-assisted color conversion. A useful starting profile; content and transport can still cause stutter. |
| MPEG-2 / MPEG-1 PS | 640×480, 30 fps; 2.5 Mbps MPEG-2 or 1.15 Mbps MPEG-1 + 192 kbps MP2. | IPU-assisted MPEG path. Compare it with AVI on your setup; neither guarantees smoother playback for every source. |
| MPEG-4 MP4 | 640×480 / 1.5 Mbps, or 320×240 / 600 kbps; 30 fps + 128 kbps AAC. | MPEG-4 Part 2, with the MP4 index front-loaded using `+faststart`. See the [remaining MP4 report](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html#mp4). |
| High Quality — Fast Storage | 10 Mbps Xvid, GOP 6, 48 kHz stereo PCM16; keeps source SD dimensions. | High storage demand; console acceptance is pending. Stereo PCM adds 1.536 Mbps before container overhead. |
| MP3 / Ogg Vorbis / AAC music | 192 kbps, 48 kHz stereo. | Lossy audio output. Video and embedded cover art are dropped. |
| FLAC music | FLAC with the source sample rate/channel count retained when possible. | Lossless compression does not restore a lossy source or guarantee unchanged samples after format conversion. SMS's FLAC path requires mono/stereo and outputs 16-bit PCM; read validation results for your input. |

MPEG-1/2 presets use GOP 15; ordinary MPEG-4 presets use GOP 50 and the high-quality Xvid preset uses GOP 6. All default to zero B-frames. The generated historical test fixtures use different GOP values, so a fixture result is not proof of every converter setting.

## Tracks, subtitles and advanced controls

### Scaling mode and output size

The target width and height define a bounding box or output canvas according to **Scaling mode**. For a 1280×720 widescreen source and a 640×480 target, the modes produce these results:

| Mode | Output in this example | Picture treatment |
| --- | --- | --- |
| Fit inside dimensions | 640×360 | Keeps the full picture and its display proportions. Adds no borders; the encoded frame can be smaller than the target. |
| Letterbox to exact dimensions | 640×480 | Keeps the full picture inside the canvas. Adds black borders above and below this widescreen source. |
| Crop to fill | 640×480 | Keeps picture proportions, fills the canvas and removes the excess edges. This example loses picture at the left and right. |
| Stretch | 640×480 | Fills the canvas by changing picture proportions. Circles and faces can become narrower or wider. |

The default is letterbox with **Allow upscaling** off. When upscaling is disabled, a small input can also reduce the canvas for letterbox, crop and stretch. For example, a square-pixel 320×180 source with a requested 640×480 target produces 320×180 with Fit and a 240×180 canvas with the other three modes. Enable upscaling when you deliberately want that small source enlarged to the requested canvas. Final dimensions are even for the supported encoders.

Normal scaling converts anamorphic sources to square pixels using their display aspect ratio. A 720×480 source tagged with a 32:27 sample aspect ratio therefore has a 16:9 picture, and fits 640×360 rather than being treated as a square-pixel 3:2 image. The source-SD option below has precedence for eligible sources and preserves their pixel aspect.

Loading a preset restores the default scaling mode and disables upscaling. Changing either setting makes the settings badge custom. These controls determine encoded geometry; final TV presentation still depends on SMS's display and aspect settings.

### One audio track or all source audio

**Source audio track** selects a zero-based source index. **Keep all source audio tracks** encodes all selected source audio to the configured audio codec in AVI, MPEG-PS or MP4. At most **six audio tracks plus video** are allowed; excess tracks produce a settings error. Tracks are re-encoded, not copied bit for bit. Language/title metadata are preserved where supported by the container.

The player has its own stream limits and codec/container mappings. Validate actual audio switching in SMS; a desktop muxer accepting a track does not prove that the PS2 reader can use it.

### Text-subtitle burn-in

Select an external subtitle or a zero-based embedded **text** subtitle index. **None** disables embedded selection; **Clear** removes the selected input. One subtitle is rendered into the video pixels. It cannot subsequently be disabled or restyled in SMS. Bitmap subtitle burn-in and switchable embedded-text output are not implemented by this control.

For switchable text, retain a separate matching SRT/SUB/TXT sidecar and use SMS's [subtitle reader](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html). The existing AVI DXSB bitmap-subtitle reader is a different player feature.

### Keep source SD dimensions

**Keep source SD resolution** retains source dimensions up to 720×576, rounded down to even values when needed, and preserves the source display aspect through its sample aspect ratio. It takes precedence over the selected scaling mode for eligible sources: choosing Stretch or Crop does not reshape an SD source while this option is enabled. Larger sources use the selected scaling mode and target. It is enabled by default only for the high-quality preset. Check display aspect and pixel aspect on your TV. Choosing this option alone does not establish hardware playback speed.

### Two-pass encoding

The app implements two-pass video encoding: analysis followed by an encode using the recorded statistics. It can improve bitrate allocation, at the cost of another processing pass. It does not cap every instantaneous packet or eliminate source/transport bottlenecks. More than two passes is not exposed by this application.

### Keyframe interval and B-frames

GOP/keyframe interval controls how far the player may need to decode from an earlier keyframe after a seek. At 30 fps, GOP 15 is about 0.5 seconds and GOP 50 about 1.67 seconds. The shorter MPEG-1/2 default is a mitigation candidate for the [reported color changes](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html#colors); re-convert existing files to test it. The exact cause and affected-console acceptance remain unconfirmed. B-frames are a separate prediction tool. Start with zero B-frames when isolating decode load; do not substitute one setting for the other.

## Understand PASS, WARN and FAIL

The validator checks codecs/container mappings, dimensions, frame-rate constraints, track counts, file size and observed packet sizes across audio/video streams. Video packets must fit 2,097,088 bytes and audio packets 524,224 bytes. It stops an output approaching the 32-bit file limit; keep each file below 4 GiB.

- **PASS**: the inspected output passed the host checks. It is not a console playback result.
- **WARN**: an issue needs attention or inspection was incomplete. The packet scan is bounded for memory/time; an incomplete scan keeps the output and cannot certify unobserved packets.
- **FAIL**: a checked constraint failed, such as an observed oversized packet. Read the reason before retrying.

Invalid input selections report a settings error before encoding. A real application exception can produce a crash log. Keep the preset, changed settings, source/output codec information and exact build when reporting it; do not treat every successful conversion as proof that an open player regression is fixed.

## Windows 7 package

The Win7 executable uses Python 3.8, PySide2/Qt5 and a compatible PyInstaller bootloader. The modern package uses Qt6. Current-runner tests verify packaging and logic, not operation on a live Windows 7 installation. The Windows FFmpeg fallback is digest-pinned; the pin must be refreshed before its archive retention expires. See [developer notes](https://nathanneurotic.github.io/Simple-Media-System/developers.html#converter).

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/converter.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/converter.html)
