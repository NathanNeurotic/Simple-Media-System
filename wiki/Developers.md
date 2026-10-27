# Contributing & Architecture

SMS combines an EE application, an FFmpeg-derived media pipeline, IPU/GS rendering and embedded IOP drivers. Keep compatibility changes narrow and tie claims to a reproducible file, build and device.

## Repository map

| Path | Responsibility |
| --- | --- |
| `src/`, `include/` | EE player, UI, config, container parsers, codecs, file contexts and rendering. |
| `iop/`, `irx/` | IOP module sources and embedded/compressed modules. Their build tools and import ABIs can differ from the EE application's toolchain. |
| `images/`, `theme/`, `fonts/` | Embedded artwork and fonts. A user `.smi` skin does not replace every icon. |
| `tools/test_*.py` | Actual-source host regressions and controlled failure tests. |
| `tools/mpeg_color_probe/` | Explicit decoder/frame-pool controls with documented scope. |
| `tools/sms_converter/` | Python converter, Qt compatibility layer, output validator and FFmpeg tests. |
| `docs-site/`, `docs/`, `wiki/` | Maintained article sources, published static site and generated native-wiki mirror. |

## Media and ownership boundaries

The file context supplies bytes to a container probe/parser. Supported streams feed packet rings; decoder threads publish frame pointers or audio data to the renderer. MPEG-1/2 reuses RGB buffers only after renderer completion; size changes wait before replacing allocated frames. Shutdown distinguishes allocated pointer slots from notification counts and respects subtitle ownership transfers.

When changing this path, test failure cleanup, partial reads, stalled input, queue reservations, seek/reset, pause/resume, sequence changes and final stop. Preserve packet alignment and limits. A test that replaces storage or display with a stub does not validate the replaced subsystem.

## Build and test

Use the [existing GCC build instructions](https://nathanneurotic.github.io/Simple-Media-System/building.html). The application uses `ps2dev/ps2dev:v1.0`, EE GCC 3.2.3; this documentation update does not migrate it. Standard, packed, DIAG and BDM=0 compatibility builds have different purposes.

```
python3 tools/test_mpeg_ps_probe.py
python3 tools/test_mpeg12_frame_lifetime.py
python3 tools/test_player_video_drain.py
python3 tools/test_mov_parser.py
python3 tools/irx_imports.py --assert dev9=1.1 irx/udpfs_smap.irx.xz irx/ata_bd.irx.xz
```

Host C harnesses need a supported host compiler and sanitizer runtime; the MOV parser fixture test also needs FFmpeg/FFprobe. The [compilation workflow](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/.github/workflows/compilation.yml) lists all 17 current host regressions. Do not submit a physical-console program merely to check whether the console is online.

## IOP and network changes

SMS embeds a dev9 v1.1 provider. IOP imports bind by library/major and ordinal, so an apparently successful module load can still bind the wrong functions. Run the ABI gate on the compressed bytes actually embedded. Keep local changes in [UDPFS attribution/build notes](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/iop/SMSUdpfs/ATTRIBUTION.md) and [SMB module notes](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/iop/smbman-sms/README.md).

SMB/HOST and UDPFS own different Ethernet stacks. Changing modes requires the explicit IOP restart/relaunch boundary; restarting can invalidate storage mounts. Avoid changing global fileXio async mode without a demonstrated ownership and cancellation design.

## Converter development

Keep modern PySide6 and Win7 PySide2 paths consistent. Preserve Python 3.8 compatibility in shared code and retain the Win7 bootloader pins. The [Windows FFmpeg setup action](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/.github/actions/setup-ffmpeg-windows/action.yml) uses a retained, SHA-checked fallback archive; refresh that pin before September 2028. Do not turn an encoder/host-package fault into a PS2 SDK defect.

Run unit, real-FFmpeg multitrack/worker and preset tests appropriate to a converter change. PASS/WARN/FAIL describe inspected output constraints, not playback certification. Bounded packet scanning must disclose incomplete inspection.

## Documentation and wiki workflow

Edit `docs-site/content/*.html`, the manifest and shared assets, then generate both the static site and the Markdown mirror:

```
python docs-site/build.py
python docs-site/build.py --check
python -m http.server 8000 --directory docs
```

GitHub Pages continues publishing `master:/docs`; a new hosting service is unnecessary. Search also loads from a local script, allowing the generated site to work without a search service. Review [the maintainer workflow](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/README.md) before adding routes, changing shared navigation or syncing the native wiki.

## Useful contributions

Start with the [open issue map](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html). Include the trigger, exact media identity, before/after result, test limits and relevant logs. Preserve unrelated dirty work and historical manuals. For format or performance claims, identify the codec settings and transport, not just the extension or filename.

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/developers.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/developers.html)
