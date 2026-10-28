# Video Playback

SMS turns a bone-stock PlayStation 2 into a video player. Point it at a movie file on any storage device — a burned disc, a USB stick, an MX4SIO microSD card, the internal HDD, SMB or UDPFS — press one button, and it plays. This page covers every video format SMS understands, how to open a file, the on-screen player controls, how the decoding works under the hood, and how to get the smoothest playback out of the console.

ℹ SMS browses raw files — there is no special folder tree

 SMS reads the **ordinary filesystem** of whatever device you give it and plays media *wherever the files sit*. Put your movies in any folder you like; SMS shows them in the file browser and plays them in place. There is no ISO, no rip step, and no mandatory folder layout — just files. Browse to a video, press `✕`, and watch.

## Supported video formats

SMS recognises a video by its container and codec, not merely its extension. The heavy lifting is done by a set of FFmpeg-derived decoders that have been hand-optimised for the PS2's Emotion Engine. The table below lists what plays and the file extensions SMS looks for.

| Container | Supported video codec | Extensions |
| --- | --- | --- |
| **AVI** | MPEG-4 Part 2 / ASP (DivX, Xvid and Microsoft MPEG-4 v3 tags) | `.avi` `.divx` `.xvid` |
| **MP4 / ISO-BMFF** | MPEG-4 Part 2 (`mp4v`) | `.mp4` |
| **MPEG program stream** | MPEG-1 / MPEG-2 | `.mpg` `.mpeg` |

Modern video codecs are not supported

 **H.264/AVC, H.265/HEVC, VP9 and AV1 are not decoded.** An MP4 carrying one of those video codecs may still play its AAC soundtrack, which looks like successful audio-only playback, but that does not mean the video codec is supported. WMV video is also not registered; `.wma` is a standalone Windows Media *audio* route.

Audio inside a video (the soundtrack) is decoded by the same engine that plays standalone music files, so a movie can carry an MP3, AAC, AC3, or other supported audio track. For the full audio picture — including playlists and standalone music — see [Audio & Playlists](https://nathanneurotic.github.io/Simple-Media-System/audio.html).

## Exact implementation limits

These are hard limits visible in the current source, not estimates:

| Property | Limit | What it means |
| --- | --- | --- |
| Frame dimensions | **Width ≤ 1024 and height ≤ 1024** | SMS rejects the file if either dimension is greater than 1024. |
| MPEG-1/2 frame rate | 23.976, 24, 25, 29.97, 30, 50, 59.94 or 60 fps | The MPEG sequence-header rate table, including MPEG-2 extensions. |
| Compressed video packet/sample | **2,097,088 bytes** | The 2 MiB packet ring minus its mandatory 64-byte allocation overhead. |
| Streams/tracks | **Eight table slots; MP4/MPEG-PS retain at most seven usable streams** | The container-wide stream table includes video, audio and subtitle tracks. |
| Individual file size | **smaller than 4 GiB** | File sizes, positions and seeks use 32-bit addressing. |

1024×1024 is an acceptance ceiling, not a speed guarantee

 The player has no universal maximum bitrate or generic MPEG-4 frame-rate rejection. A file that passes the dimension check can still be too demanding for real-time playback. Work varies with frame rate, B-frames, QPel/GMC, interlacing, motion complexity, audio decoding, device throughput and fragmentation. Large frames may also switch automatically from 32/24-bit to dithered 16-bit GS textures to fit video memory.

## Previously reported console profiles

Earlier project documentation records these fixtures as having played on a real PS2, including transport checks. This audit did not rerun that hardware matrix or qualify the October changes on the affected consoles. The generation script defines the inputs; CI generation alone is not a playback result.

| Container / codec | Resolution / rate | Encoded profile | Result |
| --- | --- | --- | --- |
| AVI / Xvid + MP3 | 640×480, 30 fps | 1.5 Mbit/s video; no B-frames; GOP 30; 128 kbit/s MP3 | Passed |
| MPEG-2 PS + MP2 | 640×480, 30 fps | 2.5 Mbit/s video; GOP 15; 192 kbit/s MP2 | Passed |
| MPEG-1 PS + MP2 | 640×480, 30 fps | 1.15 Mbit/s video; GOP 15; 192 kbit/s MP2 | Passed |
| MP4 / `mp4v` + AAC | 640×480, 30 fps | 1.5 Mbit/s video; no B-frames; GOP 30; 48 kHz stereo AAC at 128 kbit/s | Passed |
| MP4 / `mp4v` + AAC | 320×240, 30 fps | 600 kbit/s video; no B-frames; GOP 30; 48 kHz stereo AAC at 128 kbit/s | Passed |
| MP4 / H.264 or H.265 + supported audio | Tested negative cases | Unsupported video codec | No video, as expected; supported audio can still play |

This table is the real-time **confirmed baseline**, not the full accepted range. Higher dimensions up to the 1024-pixel-per-axis source limit remain valid inputs but have not been qualified as universally smooth.

## Playing a video

Opening a movie is a three-key affair: pick the device, walk into the folder, press play.

### Pick the device

SMS opens on the **device menu** — the top row of device icons. Use **Left** / **Right** to highlight the device your movie lives on (a `massN:` device for USB / MX4SIO / HDD-exFAT / i.LINK, an `mmce0:` card, the internal `hdd0:`, a CD/DVD, `smb:` or `udpfs:`). If the device you want is not listed, it may not be started yet — see [Devices Overview](https://nathanneurotic.github.io/Simple-Media-System/devices.html) for the Autostart toggles and the "Start <device> now" option.

### Enter the file list

Press **Down** to drop into the **file menu** (the middle browser). Move with **Up** / **Down**, page through long folders with **L1** / **L2**, open a folder with `✕`, and step back up one level with `△` (folder → parent → device menu). The status line at the bottom shows the current path on the left and a clock on the right.

### Press play

Highlight the video file and press `✕`. SMS reads the file straight from the device and starts playback. When the movie ends — or when you stop it — you land back in the browser exactly where you were.

ℹ Playing from MX4SIO

 This fork's headline feature is smooth video playback from an **MX4SIO** microSD adapter, which earlier SMS builds could not do reliably. It works exactly like any other device: pick the MX4SIO device, browse to your movie, press `✕`. See the [MX4SIO](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html) page for setup.

## Player controls

While a video is playing, the pad drives the player directly:

| Button | Action |
| --- | --- |
| **Select** | Pause. Press it again — or **Start** — to resume. |
| **Start** | Resume from pause; during normal playback it opens the in-player menu. |
| `△` Triangle | Stop playback and return to the file browser |
| **Left** / **Right** | Seek backward / forward |
| **Up** / **Down** | Volume up / down |
| `✕` Cross | Show or hide the on-screen time display |
| `□` Square | Cycle the aspect / pan&scan mode — eight in all. The chosen mode is remembered in your settings. |
| **L1** / **R1** | Pan the picture left / right (useful once you have zoomed with Square) |
| **L1 + R1** | Reset pan and zoom back to default |
| `○` Circle | Cycle the sync adjusters: **A/V sync** → **subtitle sync** (only when a subtitle is loaded) → off. |
| **L2** / **R2** | With an adjuster selected by Circle, shift it by −10 / +10 ms — the fix for audio or subtitles that drift out of step. |
| **L2 + R2** | Reset the selected sync adjuster to zero |

Cross, Circle, L2 and R2 are video-only. During audio-only playback those buttons do nothing, and Left/Right seek within an ordinary track; M3U transport can move between playlist entries.

If the movie has an external subtitle file alongside it, SMS displays it over the picture — see [Subtitles](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html) for how subtitle files are matched and styled.

ℹ Videos do not chain

 When a video ends you go back to the file browser. This is deliberate: the file after a video is usually unrelated, so auto-advancing would be a surprise. Only **audio** continues into the next track of the folder — see [Audio & Playlists](https://nathanneurotic.github.io/Simple-Media-System/audio.html). To watch several videos back to back, highlight the folder and choose **Play all → Video**.

### Play a video folder in order or shuffled

Highlight a folder and press `○` Circle to open **Play all**, then choose **Video** to play its videos back to back. For shuffled playback, first enable **Randomize playlist** under [Player settings → MP3 settings](https://nathanneurotic.github.io/Simple-Media-System/settings.html#mp3-settings). This setting also controls video folders, despite living in the MP3 sub-menu. Turn it off to keep the folder's normal play order.

## How SMS decodes video

SMS earns its "good video on a stock PS2" reputation by splitting the work between the Emotion Engine and a dedicated piece of PS2 hardware — the **IPU** (Image Processing Unit).

- **Decoders derived from FFmpeg.** SMS's video and audio decoders started life as parts of the FFmpeg project and were then heavily reworked for the PS2 — moving to a packed image format so the decoder produces macroblocks in the IPU's native layout, and making aggressive use of the scratchpad and DMA. The original author measured roughly a 300% speed-up in video decoding versus stock FFmpeg from these changes.
- **The IPU does the colour conversion.** Turning decoded frames into a displayable picture (the colour-space conversion, YUV → RGB) is handed to the IPU, which runs it as an interrupt-driven process *in parallel* with the main CPU still decoding the next frames. Because that step is offloaded, the Emotion Engine is free to spend its cycles decoding — which is exactly why a plain, unmodified PS2 can play good-resolution movies at a good frame rate with no HDD and no extra hardware.

Performance depends on the file

 The PS2 is a fixed console, so how smoothly a movie plays comes down to how demanding the file is. Resolution, frame rate and coding tools affect decoder work; bitrate also affects storage demand but is not a complete measure of decode complexity. A demanding encode can push the console past real-time, especially from a slower or fragmented device. Start from a hardware-confirmed profile when making a new encode.

## Encoding tips

If you are making files for SMS rather than playing existing ones, a few choices make the difference between flawless playback and a slideshow.

You do not have to do any of this by hand. The [Media Converter](https://nathanneurotic.github.io/Simple-Media-System/converter.html) is a desktop app that applies these settings for you and only offers formats SMS can play — the presets below are what it produces. Use it unless you specifically want to drive FFmpeg yourself.

- **Start from a tested profile.** 640×480 at 30 fps with 1.5 Mbit/s MPEG-4 Part 2 video is confirmed in both AVI/Xvid + MP3 and MP4/`mp4v` + AAC. Lower dimensions or bitrate provide more headroom.
- **Use a supported codec, not just a familiar extension.** For MP4, select MPEG-4 Part 2 (`mp4v`), not the much more common H.264 or H.265 encoders.
- **Avoid expensive tools when compatibility matters most.** B-frames, QPel and GMC are implemented but add work. The hardware-confirmed fixtures use no B-frames.
- **Defragment on slower devices.** On a device with limited read throughput, a heavily fragmented file forces the console to seek constantly, which can starve the decoder. Copying the file back onto a freshly-formatted card or drive so it lands contiguously gives the smoothest reads, which helps most on the slowest media.
- **Add subtitles as a separate file.** Use external text subtitles when you want to switch or restyle them; put a matching `.srt` / `.sub` next to the movie. Converter burn-in is available when permanent text is preferred — details on the [Subtitles](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html) page.

The exact command used for the confirmed 640×480 MP4 profile is:

```
ffmpeg -i INPUT.mp4 -c:v mpeg4 -vtag mp4v -b:v 1500k -bf 0 -g 30 \
  -s 640x480 -c:a aac -b:a 128k -ar 48000 -ac 2 \
  -movflags +faststart OUTPUT.mp4
```

### Community 960×544 MPEG-2 profile

tktagmedia reported a successful 44-minute MX4SIO playback using the higher-resolution MPEG-2/MP2 approach. The [preserved Windows drag-and-drop script](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/tools/tktagmedia-recommended-dragndrop-converter.bat) scales to 960×540, pads to 960×544, forces a 16:9 display aspect, uses two passes at 3 Mbps (3.4 Mbps maxrate), GOP 15, two B-frames and 224 kbps/48 kHz stereo MP2.

This is a reported profile, not a universal default or a confirmed fix for the open USB/color reports. The script does not cap the source frame rate at 30 fps, and its forced aspect is unsuitable for every source. Start with a short sample and compare the same file/settings on your console. A 60 ms `V/A -00:060` workaround is not a universal six-frame NTSC correction; use [the sync controls](https://nathanneurotic.github.io/Simple-Media-System/controls.html#playback) only as needed.

## Limitations

SMS is a stock-console player, and a few historic limits come with that. Knowing them up front saves a lot of "why won't this play" head-scratching.

- **Codec support is deliberately narrow.** There are no H.264, H.265, VP9, AV1 or WMV video decoders.
- **Interlaced is supported, but not part of the current fixture baseline.** The MPEG-4 decoder contains interlaced field/DCT paths, and MPEG-2 supports frame- and field-encoded interlaced video. The published test profiles above are progressive.
- **The source ceiling and the real-time ceiling are different.** SMS accepts each dimension through 1024, but no single bitrate or resolution below that is guaranteed for every codec-tool and storage combination.
- **Large files remain 32-bit addressed.** exFAT removes FAT32's filesystem limit, but the SMS file context still limits an individual file to less than 4 GiB.

<details><summary>Why can a stock PS2 play &quot;good-resolution movies&quot; at all?</summary>

Because SMS never asks the Emotion Engine to do the whole job. It offloads colour-space conversion to the PS2's IPU as an interrupt-driven, parallel process, and it re-shaped the FFmpeg decoder so its output lands in exactly the format the IPU wants — cutting the traffic between CPU and memory and freeing the main processor to keep decoding. That parallelism, plus scratchpad-and-DMA optimisation, is what let the original author benchmark peak frame rates well above real-time on standard-definition DivX. It is the same architecture this fork still relies on — the fork's work includes parser, streaming and frame-lifetime changes as well as access to more devices. See [the current issue map](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) for the limits of validation.

</details>

## Where to next

- [Subtitles SRT / SUB over your video](https://nathanneurotic.github.io/Simple-Media-System/subtitles.html)
- [Audio & Playlists Soundtracks, music, M3U](https://nathanneurotic.github.io/Simple-Media-System/audio.html)
- [MX4SIO Play movies from microSD](https://nathanneurotic.github.io/Simple-Media-System/mx4sio.html)
- [USB The easiest source](https://nathanneurotic.github.io/Simple-Media-System/usb.html)
- [Devices Overview Start the device you need](https://nathanneurotic.github.io/Simple-Media-System/devices.html)
- [Troubleshooting When a file won't play](https://nathanneurotic.github.io/Simple-Media-System/troubleshooting.html)

Code/fixture references: [decoder registration](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/src/SMS_Codec.c), [stream table](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/include/SMS_Container.h), [fixture definitions](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/tools/gen-test-media.sh). See [current acceptance gaps](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html).

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/video.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/video.html)
