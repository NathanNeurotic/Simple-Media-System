# SMS documentation audit — 2026-10-27

The maintained manual, repository entry points and native-wiki mirror were
reviewed against player source `4a6959a709b9679352f964fd412a69e539ef49ae`
(PR #30 merged), the converter source, release packaging and the open issue
reports. This records a documentation audit; it adds no new physical-console
acceptance evidence.

The existing Pages deployment publishes `master:/docs`. Its 24 existing article
routes are retained and six guides are added. `docs-site/content/` now owns the
article bodies; `docs-site/manifest.json` owns navigation and metadata. A single
stdlib Python generator produces 30 Pages articles, 378 searchable page/section
records and 32 wiki files (30 articles, sidebar and footer). Native GitHub wiki
publication remains a separate normal Git push, described in
[the maintainer workflow](../../docs-site/README.md).

## Material corrections

| Subject | Corrected guidance and evidence boundary |
| --- | --- |
| Release selection | July's `v3.0.0-rev1` stable tag predates October work. Rolling follows master; unmerged PR candidates require their exact Actions artifact. Preserve `build-info.json`, source/head SHA, run and hashes. The application version string alone is insufficient. |
| Current reports | Nine reports remain open at the audit date. Source fixes, host regressions, target builds and controlled emulator probes do not establish affected-console acceptance. Poster-grid browsing remains unimplemented. |
| Files and tracks | Keep each file below 4 GiB even on exFAT. Eight stream-table slots do not mean eight retained streams in every container: MOV/MP4 and MPEG-PS reserve a terminator and retain at most seven. The converter permits video plus six audio tracks. |
| Performance | The 1024-pixel-per-axis acceptance limit is not a real-time guarantee. High bitrate, frame rate, audio workload and source throughput still matter. USB stutter is not declared fixed by synchronous-read or frame-ownership checks. |
| Configuration | Remountable launch-folder configuration includes memory-card boots. `SMS.smb` follows `SMS.cfg`; `IPCONFIG.DAT` has its own card/launch-folder lookup. Asset and skin fallback paths are documented explicitly. |
| Startup and drivers | Optional autostart flags default off, but SMS can start/remount its boot source to load configuration. A visible boot source does not prove saved autostart. MX4SIO/MMCE exclusion concerns this implementation's SIO2 driver ownership. Changing flags does not unload an active driver. |
| Networks and i.LINK | HOST, SMB and UDPFS stack changes require an IOP restart/relaunch; they are not seamless live switching. i.LINK uses SBP2/SCSI and requires a console with the physical port. Historical enclosure success is not a universal bridge qualification. |
| Languages and skins | The external language file contains 311 strings. The pack includes 19 named variants plus template and guide. The menu toggles English/external; it does not enumerate every translation. Single-byte menu text and subtitle encoding are distinct. HD modes deliberately use a gradient because of VRAM limits. |
| Conversion and subtitles | Host PASS/WARN results have bounded structural scope. Burned-in text is separate from external text and existing AVI DXSB bitmap subtitles. PCM bitrate is part of the storage budget; community profiles and A/V offsets retain attribution and are not universal fixes. |
| Builds | Preserve the existing EE GCC 3.2.3/image contract. IOP rebuild tools can differ; the embedded `.irx.xz` files are real build inputs. Keep shell-specific instructions and clean between build variants. A pinned image does not guarantee identical bytes across every environment. |

## Article coverage and owning source

The paths below identify the owning implementation areas, not evidence that each
device or media combination was retested on hardware during this audit.

| Route | Reviewed content and primary evidence |
| --- | --- |
| `index.html` | Scope, quick start, format/size/stream limits; `include/SMS_Container.h`, `src/SMS_Player.c`, container implementations and release manifests. |
| `getting-started.html` | First file, source enabling, boot-source exception and report path; `src/SMS_IOP.c`, `src/SMS_GUIMenu.c`. |
| `install.html` | ELF variants, current downloads, settings locations and safe installation; release workflows, `src/SMS_Config.c`. |
| `releases.html` | Stable/rolling/PR distinction, bundle identities, diagnostics, converter/language/test-media assets; `.github/workflows/`. |
| `video.html` | Codec/container acceptance, dimensions, tracks, speed limits, retained community BAT profile; `src/SMS_Container*.c`, `src/SMS_Player.c`, `tools/sms_converter/`. |
| `audio.html` | Audio formats, playlists, shuffle and performance; `src/SMS_Codec*.c`, `src/SMS_GUIMenu.c`, playlist and player paths. |
| `pictures.html` | Baseline JPEG, navigation, large-image limits and cautious performance wording; JPEG codec/display source. |
| `subtitles.html` | External text, encoding, controls, font lookup and AVI DXSB distinction; subtitle source, `src/SMS_Config.c`, `src/SMS_ContainerAVI.c`. |
| `converter.html` | Current presets, multi-audio ceiling, burn-in, validation scope and package identity; `tools/sms_converter/` and converter workflows. |
| `controls.html` | Browser/playback/picture/controller actions and context; `src/SMS_GUIMenu.c`, player, input and picture event handlers. |
| `devices.html` | Source names, startup, exclusion and per-file limit; `src/SMS_IOP.c`, `src/SMS_Config.c`, file contexts. |
| `usb.html` | USB path, filesystem, startup, bandwidth and unchanged-file diagnosis; IOP/file-context source and #24/#29. |
| `mx4sio.html` | microSD path, activation, filesystem and driver ownership; IOP/BDM integration and prior snapshot. |
| `hdd.html` | ATA/exFAT and PFS distinctions, startup, model requirements and size limits; IOP/config/file-context source. |
| `ilink.html` | Physical-port requirement, SBP2/SCSI transport, retained historical enclosure report; i.LINK IOP integration. |
| `mmce.html` | Device path, startup, launch-folder settings, SIO2 exclusion and file limits; MMCE IOP/config integration. |
| `cd-dvd.html` | Data discs, UDF/ISO, CDDA and historical media guidance; `iop/SMSCDVD/SMSCDVD.c`, `SMSCDVD_UDFS.c`, CDDA code. |
| `network.html` | UDPFS server direction, configuration and protocol restart behavior; `src/SMS_IOP.c`, UDPFS file context. |
| `smb.html` | SMS.smb location, network settings, legacy SMB requirements and troubleshooting; `src/SMS_Config.c`, `src/SMS_IOP.c`, SMB integration. |
| `settings.html` | Menu groups, persistence, defaults and boot-source exception; `src/SMS_GUIMenu.c`, `src/SMS_Config.c`. |
| `configuration.html` | Exact config/SMB/IP/asset paths and fallback order; `SMS_ConfigSetCWD`, config save/probe logic and IOP boot remount. |
| `themes.html` | Skin locations, validation/repaint, artwork and deliberate HD gradient; config/GUI/GS source and #27. |
| `languages.html` | String count, named pack entries, encodings, menu toggle and font lookup; language source, translation pack and `TRANSLATING.txt`. |
| `wiki.html` | Complete guide map, source ownership and native wiki publication; manifest and generated mirror. |
| `known-issues.html` | Dated #10/#20/#22/#23/#24/#25/#26/#27/#29 status, tested layers and remaining gates; issue reports and PR #30 investigation records. |
| `troubleshooting.html` | Config/skin/language/media/USB cases and exact-build report checklist; owning source above and linked issue evidence. |
| `building.html` | EE/IOP contracts, variants, compressed modules, host checks and release conditions; Makefile, scripts and workflows. |
| `developers.html` | Architecture, test ownership, handoff and evidence boundaries; repository source, host scripts and PR #30 records. |
| `credits.html` | Original author/community, fork contributors, theme/artwork provenance, retained community profile and licenses; existing credits/source history. |
| `legacy-manual.html` | Historical DocBook and alternate documentation retained as historical references; `docs/src/`, `docs/pitrz/`. |

Repository entry points reviewed alongside the articles: `README.md`,
`CHANGELOG.md`, `MX4SIO_SOLUTION.md`, `patches/README.md`, the three `docs/README*`
files, `docs/translation/TRANSLATING.txt`, and the search-index compatibility
command. The archived manual, original artwork, downloads and language strings
are preserved. The original README and changelog tails compare exactly with the
source baseline after normalizing platform line endings:

- README historical tail SHA-256: `8d2673d11e2b7849ea9ac4eccab612c4499ad4a8a0ae509b0957482bc04c17ac`.
- Changelog historical tail SHA-256: `b1e281f71e7747ceeb3ec02c25779a22d60c8c317eef7ceef6f25c4cb529c40c`.

## Presentation and maintenance

The shared black/cyan PS2 theme is derived from NathanNeurotic/Launchstation,
with existing SMS jellyfish artwork. The shell has one maintained source,
section navigation, a right-hand contents list, accessible local search,
mobile navigation, scrollable tables, focus indicators and optional cursor
orbs. No runtime CDN, external font, analytics or hosted search is introduced.
Reduced-motion preferences disable decoration by default. All old article
filenames are retained; existing explicit section IDs and the old automatic
heading-ID algorithm are retained where the content still has those sections.

`python docs-site/build.py --check` validates generated bytes, duplicate IDs,
local links/fragments, search targets and icon classes. The documentation
workflow runs that check, parses browser scripts and uploads the self-contained
manual. The compatibility Node command invokes the same generator. Native wiki
sync requires the expected remote, the checkout root and a clean worktree; it
copies only generated names and never deletes unrelated wiki pages or pushes.

Browser checks cover desktop and 390px/320px layouts, menu alignment and
scrolling, keyboard search/selection, Escape and table overflow. Publication
must additionally verify the Pages deployment, live guide/search routes and the
independently pushed wiki. A successful local preview or Actions artifact alone
does not establish those publication steps.

The current external-link pass received HTTP 200 for 32 targets. The new
repository `wiki/` directory is checked locally until its first merge; the
documented localhost preview is intentionally not a public endpoint. The
owner's pre-existing README attachment and PS2-Home returned HTTP 403 to the
automated check, so their availability is not established by that result.
Those existing references were preserved.

## Remaining engineering acceptance

Use the live issues and [known-issues guide](../known-issues.html) for the current
status. The next playback investigation still needs the unchanged reporter
files and exact build identity on the affected setup, with independent checks
for color, USB stutter, MP4 timing/stall, skin persistence, shuffle and converter
behavior. Keep host, PS2 build, controlled emulator and physical-console results
separate. This documentation change does not close any report or add poster-grid
functionality.
