# Open issues: investigation and fix triage

Date: 2026-11-11. Base: master `c275476`. Working branch: `fix/open-issues-2026-11`.

Every open issue (#10, #20, #22 to #27, #29) and six subsystems (file I/O,
player core, containers and codecs, GUI/boot/config, CI/docs/release, dead
code) were investigated read-only against `c275476`. Each finding below was
then given to an independent checker whose job was to refute the diagnosis and
the proposed fix. This note is the work list: one entry per finding, with its
check result and its status on the branch.

Nothing here was tested on a console. "Reproduced on host" means the defect was
shown with the production C compiled on a PC (the `tools/test_*.py` method) or
with real media run through the parser; it does not mean a reporter confirmed
it. "Proposed fix" is the investigator's suggestion, not necessarily what was
committed: where the check disputed it, the committed change differs.

## Picking this up

- Status values: `open` (not started), `fixed <sha>` (committed on the branch),
  `skipped: <reason>`. Entries are ticked when fixed.
- Host tests need Linux (the sources take `_WIN32` paths under mingw). On
  Windows run them in WSL Debian: `setarch -R python3 tools/test_x.py`
  (`setarch -R` is needed for the ThreadSanitizer test). The CI list is in
  `.github/workflows/compilation.yml`.
- PS2 builds: `ps2dev/ps2dev:v1.0` in Docker, `make all pack`, `make BDM=0 all`
  and `make SMS_DIAG=1 all`, as CI does.
- Baseline at `c275476`: all 44 CI host tests pass; `make all pack` gives a
  1,810,056-byte `SMS.elf`.
- The AI-attribution checker from PR #54 (script and workflow) is deliberately
  not being brought to master.


## #29 MPEG-2 program stream freezes while opening

- [ ] **Seeks at or above 2 GiB land on end-of-file on BDM devices (signed 32-bit fileXioLseek): MPEG-PS tail scan spun forever at the reporter's build, still breaks duration and seeking at HEAD**
  - Ref `i29-mpegps-freeze#1`; issue #29; severity high; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_FileContext.c`, `tools/test_file_skip.py`, `tools/test_stio_device_reads.py`, `tools/test_stio_stream_failure.py`, `tools/test_stio_usb_readahead.py`
  - Proposed fix: In STIO_Seek (BDM branch) replace the fileXio call with: if ( lpPriv -> m_fXio ) lPos = aPos > 0x7FFFFFFFU ? ( unsigned int )fileXioLseek64 ( lpPriv -> m_FD, aPos, SEEK_SET ) : ( unsigned int )fileXioLseek ( lpPriv -> m_FD, aPos, SEEK_SET ); and in STIO_PfStop prefix the existing fileXioLseek line with: if ( apCtx -> m_CurPos > 0x7FFFFFFFU ) fileXioLseek64 ( ( ( STIOFilePrivate* )apCtx -> m_pData ) -> m_FD, apCtx -> m_CurPos, SEEK_SET );

- [ ] **MPEG-PS probe re-walks ~1024 headers for every later video packet when no sequence header is found: rejected or cut .mpg/.vob files read 2-7 GB before the probe returns (regression from 39d762f)**
  - Ref `i29-mpegps-freeze#2`; issue #29; severity medium; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerMPEG_PS.c`, `tools/test_mpeg_ps_probe.py`
  - Proposed fix: Walk a video stream ID once. In SMS_GetContainerMPEG_PS add `int lBadVideo = 0;`; right after the known-stream loop (after line 711) add `if ( lStartCode == lBadVideo ) goto next;`; after the `_fill_video_parameters` call (line 807) add `if ( !lfParams ) lBadVideo = lStartCode;`. Audio retry is untouched. Probe I/O and accept/reject results then match 7b172d0 on every test file.

- [ ] **MPEG-PS duration is computed from INT64_MIN when the last 128 KiB contains no PTS**
  - Ref `i29-mpegps-freeze#3`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerMPEG_PS.c`
  - Proposed fix: After the min/max loop, if no tail PTS was found (lDTS still below lPTS) do not derive a duration from it: e.g. `if ( lDTS < lPTS ) { lDTS = lPTS; apCont -> m_Flags &= ~SMS_CONT_FLAGS_SEEKABLE; }`. This exact guard was not built or tested, and how the player treats a non-seekable PS container was not checked - verify before applying. Finding 1 removes the common trigger.

- [ ] **Files of 4 GiB or more get a wrapped size (low 32 bits of the length) on BDM devices**
  - Ref `i29-mpegps-freeze#4`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_FileContext.c`
  - Proposed fix: Optional: read the size with fileXioLseek64 ( lFD, 0, SEEK_END ) on the fileXio path and clamp anything above 0xFFFFFFFF to 0xFFFFFFFF so the first 4 GiB stays playable and seekable (requires finding 1). Not built or tested; leave as documented limitation if the owner prefers.


## #24 pause, seek and resume

- [ ] **Rewind (LEFT) near or down to the start of a file exits playback instead of restarting from the beginning**
  - Ref `i24-pause-seek-freeze#1`; issue #24; severity medium; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check pending; status: open
  - Files: `src/SMS_PlayerControl.c`
  - Proposed fix: In PlayerControl_Scroll: 1. After line 652 (`lTime += lIncr;`) add `if ( lTime < lpCont -> m_StartTime ) lTime = lpCont -> m_StartTime;`. 2. Lines 670-686: keep the `if ( !lSize ) { retVal = 0; break; }` branch, then set `lFilePos = lpFileCtx -> m_CurPos;` immediately after it, and change the terminal test from `lTime == 0LL` to `lTime == lpCont -> m_StartTime`. Drop the later duplicate assignment at line 686.

- [ ] **c275476 regression: forward scrollbar seek on AVI/MP4 stops playback when no keyframe exists at or after the target**
  - Ref `i24-pause-seek-freeze#2`; issue #24; severity medium; reproduced on host: yes; independent check: diagnosis check pending; fix check pending; status: open
  - Files: `src/SMS_PlayerControl.c`, `tools/test_player_scrollbar_seek.py`
  - Proposed fix: Keep the forward preference but fall back to the previous keyframe. In PlayerControl_ScrollBar replace `if ( lpCont -> Seek ( lpCont, s_Player.m_VideoIdx, lDir, lPos ) ) {` with `if ( lpCont -> Seek ( lpCont, s_Player.m_VideoIdx, lDir, lPos ) || ( lDir > 0 && lpCont -> Seek ( lpCont, s_Player.m_VideoIdx, -1, lPos ) ) ) {`. AVI and MOV _Seek return before mutating any cursor on this failure, so the retry is safe.

- [ ] **MPEG-PS seek accepts non-I pictures: picture_coding_type is read from the wrong bits**
  - Ref `i24-pause-seek-freeze#3`; issue none; severity low; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check pending; status: open
  - Files: `src/SMS_ContainerMPEG_PS.c`
  - Proposed fix: In _Seek change the picture test to `( ( File_GetUInt ( lpFileCtx ) >> 11 ) & 7 ) == 1`.

- [ ] **Pause semaphores can keep stale counts after a seek when a renderer was not parked**
  - Ref `i24-pause-seek-freeze#4`; issue #24; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_Player.c`
  - Proposed fix: In _terminate_threads, after the `if ( lThread.status != 0x10 ) { ... }` block (all four workers are stopped there), drain both semaphores: `while ( PollSema ( s_SemaPauseAudio ) >= 0 ); while ( PollSema ( s_SemaAckPause ) >= 0 );`. This only helps the seek path; a stale ack on plain pause/resume without a seek would need the handshake itself changed.


## #24 storage throughput (MX4SIO stutter)

- [ ] **MX4SIO / non-USB mass: playback still reads with a same-priority synchronous 4 KiB RPC chain; extend the existing read-ahead helper to all mass: files**
  - Ref `i24-storage-throughput#1`; issue #24; severity high; reproduced on host: no; independent check: defect confirmed, proposed fix disputed; status: open
  - Files: `src/SMS_FileContext.c`, `tools/test_stio_usb_readahead.py`, `tools/test_stio_device_reads.py`, `tools/test_stio_stream_failure.py`, `docs/investigations/2026-11-01-usb-readahead.md`, `docs/investigations/2026-10-27-storage-timing.md`, `docs/known-issues.html`
  - Proposed fix: In src/SMS_FileContext.c only: (1) rename STIOFilePrivate.m_fUSB (line 1271) to m_fMass and set it for every fileXio file whose name starts with "mass" and is not udpfs; keep the BDM driver-name ioctl solely to choose 64 KiB chunk/fill for USB (lines 2013-2027, 2066). (2) In STIO_PfStart (1666-1715) replace USB_RD_CHUNK with `lSlot = lpPriv -> m_FillLimit`: gate becomes `if ( !lpPriv -> m_fMass || s_PF.m_pOwner != NULL ) return 0;

- [ ] **Converter MPEG-1/2 presets set only an average bitrate (no VBV cap), so a '>4000 kbps' file can peak well above what the storage path sustains**
  - Ref `i24-storage-throughput#2`; issue #24; severity low; reproduced on host: no; independent check: defect confirmed, proposed fix disputed; fix check: confirmed; status: open
  - Files: `tools/sms_converter/ffmpeg_utils.py`, `tools/sms_converter/config.py`
  - Proposed fix: Do not change anything on this evidence alone. First measure: convert the issue's source clip with the MPEG-2 preset at 4000-6000 kbps and print per-second video bitrate with ffprobe (packet sizes summed per second). If peaks exceed roughly 1.5x nominal, add `-maxrate` (about 1.25x the chosen bitrate) and a matching `-bufsize` (about 1 s of maxrate) to the mpeg1video/mpeg2video branch only. This belongs to whoever owns the converter work;


## #20 / #23 MPEG-2 colour

- [ ] **#23: fluctuating chroma is an inherited 2.9rev4 motion-compensation bug already fixed on master after the reporter's last test; no remaining decoder defect found, reporter retest required**
  - Ref `i20-23-mpeg-colour#1`; issue #23; severity medium; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check: refuted; status: open
  - Files: `src/libmpeg.c`, `src/libmpeg_core.S`, `include/libmpeg_internal.h`
  - Proposed fix: No decoder code change. Keep #23 open and ask the reporter to re-convert the same source with the current converter and replay on a current player build; state plainly that 2.9rev4 shows the same artefact, that two chroma motion-compensation defects were fixed after the tested build and verified bit-exact against FFmpeg on host for prediction-only macroblocks, and that console acceptance is still unverified.

- [ ] **#20: no fork-specific or remaining host-reproducible cause for the green cast; same post-test fixes apply, root cause not established without the reporter's file**
  - Ref `i20-23-mpeg-colour#2`; issue #20; severity medium; reproduced on host: no; independent check: diagnosis check: refuted; fix check: refuted; status: open
  - Files: `src/libmpeg.c`, `src/libmpeg_core.S`, `src/SMS_MPEG12.c`
  - Proposed fix: No code change justified. Keep #20 open; request a retest on a current build with a freshly converted file (GOP 15 preset, post-PR #35 player) and a short problematic MPG attachment. Do not present PR #35 or the GOP change as a confirmed fix for #20.

- [ ] **Assembly half of the PR #35 chroma fixes has no regression test; existing host tests only cover the C fields**
  - Ref `i20-23-mpeg-colour#3`; issue #23; severity low; reproduced on host: yes; independent check: diagnosis check: refuted; fix check: confirmed; status: open
  - Files: `tools/test_mpeg_motion_vectors.py`, `src/libmpeg_core.S`
  - Proposed fix: Optional, only if the owner wants the assembly pinned: add the ~150-line R5900 subset interpreter and the random differential part of the harness (T1 only) as a tools/ test that runs after the EE build in CI, feeding it the .text of obj/libmpeg_core.o (ee-objcopy -O binary -j .text) and its ee-nm symbol list. Skip the FFmpeg stream oracle in CI. If that is judged too heavy for the repo, leave as is and rely on the PCSX2 probe.

- [ ] **Investigation note says converter GOP defaults are unchanged, but the MPEG-1/2 presets now use GOP 15**
  - Ref `i20-23-mpeg-colour#4`; issue #23; severity low; reproduced on host: yes; independent check: defect confirmed, proposed fix disputed; status: open
  - Files: `docs/investigations/2026-10-28-playback-followup.md`, `tools/sms_converter/config.py`
  - Proposed fix: One-sentence correction in the investigation note (or a dated addendum) stating that commit 23d1693 changed the MPEG-1/2 presets to GOP 15, and that reporter retests therefore change both GOP and decoder unless the file is kept constant.


## #10 MP4 slow open and stall

- [ ] **MP4 video esds is never parsed: stsd video entry is read 2 bytes short, so the MPEG-4 VOL seed never runs**
  - Ref `i10-mp4-stall#1`; issue #10; severity high; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check pending; status: open
  - Files: `src/SMS_ContainerMOV.c`, `src/SMS_MPEG4.c`, `tools/test_mov_parser.py`
  - Proposed fix: In _mov_read_stsd, video branch, directly after `lpCodecCtx -> m_BitsPerSample = File_GetShortBE ( apFileCtx );` (line 348) add one line: `File_Skip ( apFileCtx, 2 );` (colour table id).

- [ ] **MOV packet interleave is chosen by millisecond-rounded DTS only, causing backward seeks that restart USB read-ahead (every video frame in Rev1, 0.5-1.2 per second on master)**
  - Ref `i10-mp4-stall#2`; issue #10; severity medium; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check pending; status: open
  - Files: `src/SMS_ContainerMOV.c`, `src/SMS_FileContext.c`
  - Proposed fix: One condition in _ReadPacket: replace `if ( lDTS < lBestDTS ) {` with `if ( !lpSample || ( lDTS - lBestDTS < SMS_TIME_BASE && lBestDTS - lDTS < SMS_TIME_BASE ? lpCurSample -> m_Pos < lpSample -> m_Pos : lDTS < lBestDTS ) ) {` i.e. when two candidates are within 1 s, take the one stored first in the file, otherwise keep DTS order (same rule libavformat's mov demuxer uses on seekable input; AVI in SMS already reads in pure file order).

- [ ] **Regression vs Rev1: edit-list gate rejects whole MP4/M4A files (AAC priming > 1 frame, zero last stts delta, trimmed copies)**
  - Ref `i10-mp4-stall#3`; issue none; severity medium; reproduced on host: yes; independent check: diagnosis check pending; status: open
  - Files: `src/SMS_ContainerMOV.c`, `tools/test_mov_parser.py`
  - Proposed fix: Smallest change: delete the line `if ( !_mov_edit_fits ( lpStm, lpMyStm, lpMOV ) ) return 0;` and the then-unused _mov_edit_fits (about 30 lines removed). The per-track mapping already applied (m_TimeOffset = empty edit - m_EditStart) keeps A/V aligned for any single edit. Trialled in scratch h/mov_fix4.c: all five rejected fixtures open; 2112-sample priming yields audio first PTS -44 ms;

- [ ] **B-frame MP4: video packets carry composition PTS while MPEG4_Decode stamps the frame it outputs with the current packet's PTS**
  - Ref `i10-mp4-stall#4`; issue #10; severity low; reproduced on host: no; independent check: diagnosis check pending; fix check pending; status: open
  - Files: `src/SMS_ContainerMOV.c`, `src/SMS_MPEG4.c`, `tools/test_mov_packets.py`
  - Proposed fix: In _ReadPacket, after m_DTS and m_PTS are set, for video use the decode timestamp: `if ( lpStm -> m_pCodec -> m_Type == SMS_CodecTypeVideo ) lpPkt -> m_PTS = lpPkt -> m_DTS;`. With the usual one-frame reorder delay the frame output at packet k has presentation time equal to that packet's edit-adjusted DTS, so stamps become exact; for files without B-frames DTS equals PTS already. Not trialled.

- [ ] **PAL 2:2 pulldown forces the video time base to 1/25, which is wrong for MP4 video whose time base is not one frame per tick**
  - Ref `i10-mp4-stall#5`; issue #10; severity low; reproduced on host: no; independent check: diagnosis check: confirmed; fix check: confirmed; status: open
  - Files: `src/SMS_Player.c`
  - Proposed fix: Scale the existing video time base instead of replacing it, using the same ratio the audio loop below already applies: new Num/Den = old Num * lRate / ( old Den * lBase * 25 ) reduced (for AVI this is exactly 1/25), or simply skip the pulldown when lpVideoStm -> m_TimeBase.m_Num * lRate != lpVideoStm -> m_TimeBase.m_Den * lBase (time base is not one frame per tick). The skip variant is one condition.


## #27 skin and language

- [ ] **Saved skin and saved display mode are never applied when the late config load comes from the memory-card fallback (every HDD/pfs boot, phantom mass:, unreachable udpfs)**
  - Ref `i27-skin-language#1`; issue #27; severity medium; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check: confirmed; status: open
  - Files: `src/main.c`, `src/SMS_IOP.c`
  - Proposed fix: Record that a config was loaded after the first paint instead of inferring it from SMS_ConfigOnFS(). (a) src/SMS_IOP.c after line 50 'unsigned int g_IOPFlags;' add 'int g_CfgLate;'. (b) src/SMS_IOP.c:1559 'SMS_LoadConfig ();' -> 'g_CfgLate = SMS_LoadConfig ();'. (c) src/SMS_IOP.c:1581 (fallback branch) 'SMS_LoadConfig ();' -> 'g_CfgLate = SMS_LoadConfig () && SMS_ConfigOnFS ();' (card-only fallback was already applied by the early load;

- [ ] **_bgTexUnsafe treats SD canvas widths 641..720 as HD: skin, wallpaper and splash vanish when the user widens the picture**
  - Ref `i27-skin-language#2`; issue #27; severity medium; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check: confirmed; status: open
  - Files: `src/SMS_GUIDesktop.c`, `tools/test_skin_loading.py`, `src/SMS_GUIMenuSMS.c`
  - Proposed fix: src/SMS_GUIDesktop.c:151: 'return g_GSCtx.m_Width > 720; /* SD modes are user-adjustable 640..720 ( _dispw_handler ); only 720p ( >= 1090 ) / 1080i ( >= 1400 ) collide */' and correct the sentence in the comment at 147-148. One token.

- [ ] **Boot-time video-mode rescue (SELECT+R1 = NTSC, SELECT+R2 = PAL) is undone on USB/MMCE boots**
  - Ref `i27-skin-language#3`; issue none; severity medium; reproduced on host: no; independent check: diagnosis check: confirmed; status: open
  - Files: `src/SMS_GUI.c`, `src/main.c`, `src/SMS_IOP.c`
  - Proposed fix: Keep the override outside g_Config: in src/SMS_GUI.c add a file-scope 'int g_BootVMode = GSVideoMode_Default;', set it where lVideoMode is set today (lines 749/754, or simply assign after the loop at 772), and in src/main.c right after 'SMS_IOPInit ();' add 'if ( g_BootVMode != GSVideoMode_Default ) g_Config.m_DisplayMode = g_BootVMode;'. The existing guard then re-inits into the same mode already on screen (or skips). About 3 lines.

- [ ] **Boot splash (fade in, 1 s hold, fade out) replays on every full desktop rebuild; upstream's first-render-only gate was lost**
  - Ref `i27-skin-language#4`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_GUIDesktop.c`
  - Proposed fix: src/SMS_GUIDesktop.c:612: 'if ( !s_Init ) _DrawSplash ();'. One token; gives the dead variable its upstream purpose back.

- [ ] **Late-loaded picture offsets, display width/height, colour depth and charset are not applied at boot on USB/MMCE when mode is Default and no skin is set**
  - Ref `i27-skin-language#5`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/main.c`, `src/SMS_GUI.c`, `src/SMS_IOP.c`
  - Proposed fix: After the g_CfgLate change above, simplify the guard to 'if ( g_CfgLate ) GUI_Initialize ( 0 );' (drop the mode/skin clause). Land it only together with the splash-once fix, otherwise every USB boot with a saved config shows the splash twice.

- [ ] **cdrom/host/SMB boots: late config reload clears SMS_BF_UDFL without re-running the locale init, so the Language row reports a valid pack as bad**
  - Ref `i27-skin-language#6`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_IOP.c`, `src/SMS_Config.c`, `src/SMS_GUIMenuSMS.c`
  - Proposed fix: src/SMS_IOP.c after line 1582 'GUI_SetColors ();' add 'SMS_LocaleInit ();' and 'SMS_LocaleSet ();' (mirrors 1561-1562). Two lines; included in the private copy that builds.

- [ ] **Skin / MBF names longer than 63 characters overflow g_Config.m_SkinName / m_MBFName**
  - Ref `i27-skin-language#7`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_EE.c`, `src/SMS_GUIMenuSMS.c`, `include/SMS_Config.h`
  - Proposed fix: src/SMS_EE.c: change 'if ( lLen > 4 )' to 'if ( lLen > 4 && lLen < 68 )' so a name that cannot fit (63 chars + extension) is simply not listed. One condition, covers both skins and MBF fonts.

- [ ] **DrawSkin places the skin at 0x4000 - size without checking it clears the framebuffer; a large skin (e.g. 1024x768) overlaps the SD framebuffer**
  - Ref `i27-skin-language#8`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_GUIDesktop.c`
  - Proposed fix: Optional hardening, one condition: 'if ( lTexSize >= 0x4000 || 0x4000 - lTexSize < g_GSCtx.m_VRAMPtr ) {' (m_VRAMPtr is the first free block after the framebuffer, and after the font in 16-bit mode). This exact bound would also make the width heuristic in DrawSkin unnecessary, but the one-token '> 720' fix is the smaller change.

- [ ] **Legacy lng/ directory: all 19 packs are rejected by the current loader, and its README tells users to install them**
  - Ref `i27-skin-language#9`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `lng/README.txt`, `lng`, `docs/translation/TRANSLATING.txt`
  - Proposed fix: Delete lng/ (the translator credits in lng/README.txt are already carried in CHANGELOG.md; if the owner wants them kept verbatim, move the author list into docs/translation/TRANSLATING.txt first). Smaller alternative: replace lng/README.txt with a one-line pointer to docs/translation. Removing credited files is the owner's call under the repository attribution policy.

- [ ] **English template docs/translation/SMS.lng drifted from the built-in table on two lines (and every pack carries the older wording of one of them)**
  - Ref `i27-skin-language#10`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `docs/translation/SMS.lng`, `src/SMS_Locale.c`
  - Proposed fix: Edit the two lines of docs/translation/SMS.lng to match src/SMS_Locale.c (keep CRLF, keep exactly 311 lines). Optionally add lngcheck's template-equals-table assertion to an existing locale host test so the template cannot drift again.


## #25 shuffle and playlists

- [ ] **Randomize playlist hangs SMS forever when an .m3u or Play all -> Audio folder has two entries with the same title**
  - Ref `i25-shuffle-playlist#1`; issue #25; severity high; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check: confirmed; status: open
  - Files: `src/SMS_ContainerM3U.c`, `tools/test_m3u_probe.py`
  - Proposed fix: src/SMS_ContainerM3U.c line 431, replace if ( !lpNode || SMS_ListFind ( lpNewList, _STR( lpNode ) ) ) continue; with if ( !lpNode ) break; The duplicate check is redundant: the node is removed from lpList right after being copied, so it cannot be picked twice. lpNode is never NULL here (lIdx < lSize == lpList->m_Size); `break` just makes that path terminate instead of spinning.

- [ ] **Shuffle seed is always a multiple of 8, biasing the first random draw (one track/order favoured up to 1.6x)**
  - Ref `i25-shuffle-playlist#2`; issue #25; severity low; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check: confirmed; status: open
  - Files: `src/SMS_GUICmdProc.c`, `src/SMS_ContainerM3U.c`
  - Proposed fix: Drop the three constant zero bits when seeding, at all three sites: src/SMS_GUICmdProc.c:151 g_RandSeed = ( int )g_Timer; -> g_RandSeed = ( int )( g_Timer >> 3 ); src/SMS_GUICmdProc.c:225 same change src/SMS_ContainerM3U.c:424 same change Nothing else. Do not change SMS_rand itself (also used by SMS_PlayerBallSim.c).

- [ ] **Continuous audio playback opens a same-named .srt/.sub/.txt as a subtitle context that the audio player never closes**
  - Ref `i25-shuffle-playlist#3`; issue #25; severity low; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check: confirmed; status: open
  - Files: `src/SMS_GUICmdProc.c`
  - Proposed fix: src/SMS_GUICmdProc.c:182, change the flag to 0 so the loop opens tracks exactly as the browser does for audio: lpParam = SMS_OpenMediaFile ( _STR( lpNode ), 0 );

- [ ] **Single-track shuffle continuation repeats tracks and never ends; code comment misdescribes it and the docs do not mention it**
  - Ref `i25-shuffle-playlist#4`; issue #25; severity low; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check: confirmed; status: open
  - Files: `src/SMS_GUICmdProc.c`, `docs-site/content/audio.html`, `docs-site/content/settings.html`
  - Proposed fix: No behaviour change. (1) Correct the comment at SMS_GUICmdProc.c:140-144 to: random pick among the other audio files for each next track; tracks can recur before all have played and playback continues until the user stops; use Play all -> Audio for an each-track-once shuffle.


## #22 converter

- [ ] **MOV/MP4 parser skips 2 bytes too few in the video sample entry, so the video esds is never parsed (no VOL for MPEG-4, MPEG-1/2-in-MP4 misidentified)**
  - Ref `i22-converter#1`; issue #10; severity high; reproduced on host: yes; independent check: diagnosis check pending; fix check pending; status: open
  - Files: `src/SMS_ContainerMOV.c`, `src/SMS_MPEG4.c`, `tools/test_mov_parser.py`
  - Proposed fix: In _mov_read_stsd, video branch, immediately after `lpCodecCtx -> m_BitsPerSample = File_GetShortBE ( apFileCtx );` add `File_Skip ( apFileCtx, 2 ); /* colour table id */`. Nothing else.

- [ ] **Converter offers MP4/M4A codec combinations the player cannot decode or ffmpeg cannot write (AC-3, Vorbis, MPEG-1/2 video)**
  - Ref `i22-converter#2`; issue #22; severity medium; reproduced on host: yes; independent check: diagnosis check pending; fix check pending; status: open
  - Files: `tools/sms_converter/config.py`, `tools/sms_converter/test_converter.py`
  - Proposed fix: In config.py narrow the table to what is demonstrably readable: `.mp4: video {"mpeg4"}, audio {"aac", "mp3"}` and `.m4a: video set(), audio {"aac", "mp3"}`. If keeping MPEG-1/2-in-MP4 is desired, land the MOV esds fix first and get a hardware confirmation; AC-3 and Vorbis should be dropped regardless. No other code needs to change - the widget and validator already read this table.

- [ ] **FLAC preset emits multichannel and 24-bit files; player rejects >2-channel FLAC and outputs 16-bit anyway; validator says PASS**
  - Ref `i22-converter#3`; issue none; severity medium; reproduced on host: yes; independent check: diagnosis check pending; status: open
  - Files: `tools/sms_converter/ffmpeg_utils.py`, `tools/sms_converter/config.py`, `tools/sms_converter/validator.py`, `src/SMS_ContainerFLAC.c`
  - Proposed fix: In build_ffmpeg_cmd audio section, when acodec == 'flac' add `aformat=sample_fmts=s16:channel_layouts=mono|stereo` to the audio filter chain; build one comma-joined `-af` string so it combines with the existing optional `loudnorm` (currently `-filter:a loudnorm`) instead of emitting two filter options. No new settings or UI.

- [ ] **BT.709 sources are encoded with a BT.709 matrix while the PS2 decodes as BT.601 (measured hue/saturation error; not the #20/#23 defect)**
  - Ref `i22-converter#4`; issue #20; severity low; reproduced on host: yes; independent check: diagnosis check pending; fix check pending; status: open
  - Files: `tools/sms_converter/ffmpeg_utils.py`
  - Proposed fix: Optional and separable; do not bundle with a #20/#23 fix. If wanted: when the source is HD/BT.709 (probe color_space == bt709, or unspecified with height > 576) append `:in_color_matrix=bt709:out_color_matrix=bt601` to the existing scale filter built by build_scale_filter. Do not add out_range (range conversion is already correct). Otherwise leave as-is and document.

- [ ] **Dead and duplicated converter code that can be deleted**
  - Ref `i22-converter#5`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `tools/sms_converter/validator.py`, `tools/sms_converter/ffmpeg_utils.py`, `tools/sms_converter/ui/main_window.py`, `tools/sms_converter/ui/preset_selector.py`, `tools/sms_converter/ui/queue_table.py`, `tools/sms_converter/ui/advanced_settings.py`, `tools/sms_converter/test_end_to_end.py`
  - Proposed fix: Delete items 1, 3, 5, 6, 7 outright; drop the unused parameter in 2 (update its callers); for 8 remove the spin box, its form row, the `passes` key and the setEnabled line at 618; fix the docstring in 9. Item 4: keep the `fps=` filter and drop `-r` only if the two-pass and MPEG-1/2 frame-rate tests still pass - otherwise leave it. No behaviour change intended.


## #26 artwork and poster views

- [ ] **R2 back to list view (and Triangle-back to a level entered from grid/poster view) leaves an invalid list window; UP then moves the selection off-screen**
  - Ref `i26-artwork-poster#1`; issue #26; severity medium; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check: confirmed; status: open
  - Files: `src/SMS_GUIFileMenu.c`, `tools/test_browser_reload.py`
  - Proposed fix: Two inline edits in GUIFileMenu_HandleEvent, src/SMS_GUIFileMenu.c (kept inline on purpose: a new helper function would have to be added to the extraction lists of tools/test_browser_reload.py and tools/test_browser_artwork_lifecycle.py, which was tried and fails to compile otherwise). (1) R2 handler, line 1451. Before: else { lpMenu -> m_pFirst = lpMenu -> m_pCurrent; lpMenu -> m_YOffset = 0;

- [ ] **Circle (choose subtitles) in grid/poster view re-decodes the artwork page just before playback and keeps the cache allocated while the video plays**
  - Ref `i26-artwork-poster#2`; issue #26; severity low; reproduced on host: no; independent check: defect confirmed, proposed fix disputed; status: open
  - Files: `src/SMS_GUIFileMenu.c`, `src/SMS_GUIMiniBrowser.c`
  - Proposed fix: Smallest change that restores the intended release without altering the existing order, in _perform_file_action (lines 1043-1048). Before: if ( apMenu -> m_ArtworkMode ) GUIFileMenu_Cleanup ( ( GUIObject* )apMenu ); if ( !SMS_OpenMediaFile ( _STR( apMenu -> m_pCurrent ), aFlags | SMS_FA_FLAGS_MSG ) && apMenu -> m_ArtworkMode ) _redraw ( apMenu, 0 ); After: void** lpParam;

- [ ] **User docs do not describe the R2 three-view cycle (selected-poster view) or that matching JPEG sidecars are hidden from the browser**
  - Ref `i26-artwork-poster#3`; issue #26; severity low; reproduced on host: no; independent check: defect confirmed, proposed fix disputed; status: open
  - Files: `wiki/Video.md`, `docs-site/content/video.html`
  - Proposed fix: In wiki/Video.md (and the matching paragraph in docs-site/content/video.html) replace the two-state R2 sentence with the real cycle: list -> 3x2 grid -> list with poster of the selected file -> list; add the poster-view keys (Up/Down one row, L1/L2 six rows, Left/Right change focus) to the table;


## File I/O

- [ ] **Copy to HDD aborts after the first 384 KiB chunk: SMS_CopyFile compares against an lnWrite that fioSync never writes**
  - Ref `hunt-file-io#1`; issue none; severity medium; reproduced on host: yes; independent check: diagnosis check pending; status: open
  - Files: `src/SMS_CopyTree.c`, `src/SMS_GUIFileCtxMenu.c`
  - Proposed fix: src/SMS_CopyTree.c, SMS_CopyFile, line 342. Take the synchronous result from fioWrite itself. Before: if ( fioWrite ( lFD, lpBuf, lnRead ) < 0 ) { After: if ( ( lnWrite = fioWrite ( lFD, lpBuf, lnRead ) ) < 0 ) { Leave lines 363-365 as they are. In WAIT mode fioSync is a no-op and lnWrite keeps the synchronous byte count, so the short-write (disk full) check still works.

- [ ] **Copy to HDD from CD/DVD/host appends up to 384 KiB of junk: the queued write overwrites the read-ahead's result that STIO_FillStm collects**
  - Ref `hunt-file-io#2`; issue none; severity medium; reproduced on host: yes; independent check: diagnosis check pending; status: open
  - Files: `src/SMS_FileContext.c`, `src/SMS_CopyTree.c`
  - Proposed fix: src/SMS_FileContext.c, STIO_FillStm, insert one line after the existing range check that ends at line 1755 and before `lOldBuf = apCtx -> m_CurBuf;` (line 1756): if ( ( unsigned int )lnRead > apCtx -> m_Size - apCtx -> m_CurPos ) lnRead = apCtx -> m_Size - apCtx -> m_CurPos; m_CurPos < m_Size is already guaranteed by the enclosing if at line 1739.

- [ ] **Seek or Stream(pos,0) past end of file leaves the logical cursor and the device descriptor out of step**
  - Ref `hunt-file-io#3`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_FileContext.c`
  - Proposed fix: src/SMS_FileContext.c, two lines. 1. STIO_Seek, immediately after the declarations at lines 1380-1381: if ( aPos > apCtx -> m_Size ) return -1; This matches STIO_SeekPf and keeps the descriptor where it was. 2. STIO_Stream, anBlocks == 0 branch, line 1847. Before: if ( aStartPos > apCtx -> m_Size ) return 0; After: if ( aStartPos > apCtx -> m_Size ) { apCtx -> Seek ( apCtx, apCtx -> m_CurPos ); return 0;

- [ ] **SMB share list: D-cache is flushed after the IOP DMA instead of before it**
  - Ref `hunt-file-io#4`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_FileDir.c`, `iop/smbman-sms/src/smb_fio.c`
  - Proposed fix: src/SMS_FileDir.c: move the flush ahead of the devctl and make it unconditional. Before the `lnShares = fileXioDevctl ( ... )` call (inside the existing BDM block or just above it): SyncDCache ( lpShare, ( char* )lpShare + lMaxEnt * sizeof ( ShareEntry_t ) ); Delete the post-call line 368 (`if ( lnShares > 0 ) SyncDCache ( ...


## Player core and audio

- [ ] **SPU_RPC_TIMEOUT is 180 ms, not ~3 s: overlapping UI sounds permanently retire the audio server (silent, unpaced playback)**
  - Ref `hunt-player-sync#1`; issue none; severity high; reproduced on host: no; independent check: diagnosis check: confirmed; fix check: confirmed; status: open
  - Files: `src/SMS_SPU.c`, `src/SMS_Timer.s`, `src/SMS_Sounds.c`, `src/SMS_GUIDevMenu.c`, `src/SMS_GUIDesktop.c`, `src/SMS_GUIFileMenu.c`
  - Proposed fix: src/SMS_SPU.c:96: `#define SPU_RPC_TIMEOUT 3000 /* ms ( g_Timer counts milliseconds ) -- far beyond any healthy reply, and beyond the longest UI sound a queued one must wait for */`. One line plus the comment; matches the intent stated in c76cf00. No other change needed.

- [ ] **SMS_SPU.c:177-178 'assignment makes pointer from integer without a cast' warnings**
  - Ref `hunt-player-sync#2`; issue none; severity low; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check: refuted; status: open
  - Files: `src/SMS_SPU.c`
  - Proposed fix: Replace both lines with one: `apData = ( void* )( ( unsigned int )apData & 0x0FFFFFFF );` - identical result on the 32-bit EE, no warning.

- [ ] **Issue #10 audio loop is AUDSRV replaying its ring after the EE stops feeding it (inherited); the EE stall itself is not identified**
  - Ref `hunt-player-sync#3`; issue #10; severity medium; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_Data.c`, `src/SMS_SPU.c`, `src/SMS_Player.c`
  - Proposed fix: No code change recommended from this analysis. A reliable in-player stall detector is not available cheaply: thread-level watchdogs do not run when the EE threads are starved, issuing an RPC from interrupt context is unsafe, and the proper fix (silence on underrun) lives in the source-less AUDSRV binary.

- [ ] **_Destroy copies the media path into a 512-byte stack buffer with unbounded strcpy**
  - Ref `hunt-player-sync#4`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_Player.c`
  - Proposed fix: src/SMS_Player.c:1496: `char lPath[ SMS_CWD_CAPACITY ];` (1024, matching the buffers the path is built in). One token; the player thread stack already carries larger locals on this path.


## Containers and codecs

- [ ] **ASF header: Stream Bitrate Properties uses 'flags * 0x7F' as array index (heap out-of-bounds write on ordinary multi-stream WMV)**
  - Ref `hunt-containers-codecs#1`; issue none; severity high; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerASF.c`
  - Proposed fix: One character. Before: 'int lStmID = lFlags * 0x7F;' After: 'int lStmID = lFlags & 0x7F;'. Trialled in private tree: crafted/wmv_bitrate_1_2.wmv then reports 'RESULT ASF nstm=2 ...' and reads 50 packets with no sanitizer report; all 4 valid ASF corpus files unchanged. (The shadowed, never-filled outer lBitRate[] fallback is a separate dead-code matter; leave it.)

- [ ] **AVI _ReadPacket: stream number and chunk size are never both validated (lCond uses '+' and the wrong digits) -> m_pStm[] out-of-bounds and unbounded read into the packet buffer**
  - Ref `hunt-containers-codecs#2`; issue none; severity high; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerAVI.c`
  - Proposed fix: Three lines, restoring ffmpeg's semantics. Line 797 before: 'lCond = ( lCount < ( int32_t )apCont -> m_nStm ) + ( i + lSize <= ( uint32_t )lpAVICont -> m_MoviEnd );' after: 'lCond = lSize >= 0 && i + lSize <= ( uint32_t )lpAVICont -> m_MoviEnd;' Line 802 (ix branch) before: ' lCond' after: ' lCond && lCount < ( int32_t )apCont -> m_nStm' Line 815 before: ' ) && lCond' after: ' ) && lCond && lCount < ( int32_t )apCont -> m_nStm' Trialled: all ...

- [ ] **ASF _ReadPacket: negative fragment offset/size from the file pass the bounds test and drive a wild Read() into the heap**
  - Ref `hunt-containers-codecs#3`; issue none; severity medium; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerASF.c`
  - Proposed fix: Extend the existing guard at line 552. Before: ' if ( lpASF -> packet_frag_offset >= lpASFStm -> pkt.size ||' After: ' if ( lpASF -> packet_frag_size <= 0 || lpASF -> packet_frag_offset < 0 || !lpASFStm -> pkt.data ||\n lpASF -> packet_frag_offset >= lpASFStm -> pkt.size ||' (rest of the condition and the 'continue' unchanged). Trialled: crafted/asf_frag_crash.asf then reads 252 packets to EOF;

- [ ] **AAC (ADTS) _ReadPacket: frame_length < 8 underflows 'lSize - 8' and copies the rest of the file over the packet buffer / static data**
  - Ref `hunt-containers-codecs#4`; issue none; severity medium; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerAAC.c`, `src/SMS_Container.c`
  - Proposed fix: Make the reader apply the probe's own minimum. Line 141 before: ' if ( ( *( uint16_t* )&lBuf[ 0 ] & 0xF0FF ) == 0xF0FF ) {' After: ' if ( ( *( uint16_t* )&lBuf[ 0 ] & 0xF0FF ) == 0xF0FF &&\n ( ( lBuf[ 3 ] & 3 ) << 11 | lBuf[ 4 ] << 3 | lBuf[ 5 ] >> 5 ) >= 8\n ) {'. A header with length < 8 is then treated as non-sync and the existing else-branch resyncs byte-wise. Trialled: the three crafted files read to EOF (150/150/135 packets);

- [ ] **AVI _Load_idx1 keeps looping after EOF: 4 empty device reads per missing index entry (truncated-tail AVI stalls at 'loading indices', garbage entries appended)**
  - Ref `hunt-containers-codecs#5`; issue none; severity medium; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerAVI.c`, `include/SMS_FileContext.h`
  - Proposed fix: One line at the top of the loop body (after line 148): 'if ( FILE_EOF( lpFileCtx ) ) break;'. Trialled: avi_idx_cut.avi and avi_idx_huge.avi both open with 14 Fill calls and play all 255 packets; valid AVIs unchanged (15 fills, same bytes).

- [ ] **MPEG-PS probe repeats a 1024-header look-ahead for every video packet when no sequence header is found (87 MB read from a 630 KB file before rejecting)**
  - Ref `hunt-containers-codecs#6`; issue none; severity medium; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerMPEG_PS.c`
  - Proposed fix: Do not repeat a failed look-ahead inside the window it already covered. Line 682: add ', lVidRetry = 0' to the int declaration. Before line 776 ('if ( lCodecType != SMS_CodecTypeUnknown ) {') insert: 'if ( lCodecType == SMS_CodecTypeVideo && i < lVidRetry ) goto next;'. After line 807 insert: 'if ( !lfParams ) lVidRetry = i + 1023;'. Trialled: same two files now 'RESULT none bytes=1329158 fills=325 seeks=24' (65x less);

- [ ] **ASF header loop never checks EOF: a WMV/WMA truncated inside the File Properties object hangs the probe forever**
  - Ref `hunt-containers-codecs#7`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerASF.c`
  - Proposed fix: One line at the top of the loop (line 161): after ' while ( 1 ) {' insert ' if ( FILE_EOF( lpFileCtx ) ) goto fail;'. Trialled: the truncated file returns 'RESULT none ... bytes=930' immediately; valid ASF files unchanged (a valid header always reaches the Data object before EOF, and line 343 already fails on EOF after it).

- [ ] **MOV stsd: entry count from the file drives 'while ( lnEntries-- )' with no bound or EOF test (multi-billion-iteration hang)**
  - Ref `hunt-containers-codecs#8`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerMOV.c`
  - Proposed fix: Both loops (lines 331 and 726). Before: ' while ( lnEntries-- ) {' After: ' while ( lnEntries-- > 0 && !FILE_EOF( apFileCtx ) ) {'. Trialled: the crafted file opens ('RESULT MOV nstm=1') and plays its audio to EOF; the 4 valid MP4/M4A files unchanged.

- [ ] **AVI header: zero-stream file reaches _CheckOffsetBug with m_pStm[0]==NULL; OpenDML 'indx' accepts stream id == m_nStm (off-by-one)**
  - Ref `hunt-containers-codecs#9`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerAVI.c`
  - Proposed fix: (a) line 721 before: ' if ( lStmIdx != apCtx -> m_nStm - 1 )' after: ' if ( !apCtx -> m_nStm || lStmIdx != apCtx -> m_nStm - 1 )'. (b) line 261 before: ' if ( ( uint32_t )lStID > apCont -> m_nStm || lStID < 0 ) return 0;' after: ' if ( ( uint32_t )lStID >= apCont -> m_nStm ) return 0;' (the unsigned compare already covers negatives). Trialled: both repro files then parse/reject without sanitizer reports; valid AVIs unchanged.

- [ ] **AVI _Load_idx1: dead 'same index' bookkeeping reads m_pStm[-1] on every indexed AVI**
  - Ref `hunt-containers-codecs#10`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerAVI.c`
  - Proposed fix: Delete the dead code only: lines 135-137 (three locals), line 130 ('SMS_Stream* lpBaseStm;'), lines 168-181 (the 'lpBaseStm = ...' assignment and the whole 'if ( lIdx == lPrevIdx ) ... else { ... }' block), line 201 ('if ( lnMaxSameIdx < lnSameIdx ) lnMaxSameIdx = lnSameIdx;'), and change line 208 to ' return 1;'. Exact strings are in the scratch fixes.py ('avi_idx1_dead').

- [ ] **MP3 probe accepts a single header-like word when the next frame would start at/after EOF (small non-MP3 files detected as MP3)**
  - Ref `hunt-containers-codecs#11`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_ContainerMP3.c`
  - Proposed fix: Require at least one confirming frame. Line 58 before: ' if ( FILE_EOF( apFileCtx ) ) break;' After: ' if ( FILE_EOF( apFileCtx ) ) {\n if ( !i ) { lVal = 0; goto end; }\n break;\n } /* end if */'. Trialled: png512.bin -> none on both seek models; 3-frame and 5-frame MP3s still detected on both; detection matrix of the 49 valid files unchanged. Trade-off to decide: an MP3 consisting of exactly one frame (about 26 ms) is no longer recognised.

- [ ] **WMA decoder trusts the container's channel count (arrays are sized for 2 channels; the player clamps channels only for AC3/DTS/AAC)**
  - Ref `hunt-containers-codecs#12`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_WMA.c`, `src/SMS_Player.c`
  - Proposed fix: Not trialled beyond compile reasoning. Smallest consistent change: at the top of wma_decode_init (SMS_WMA.c:1510), mirroring the existing AAC/AC3 clamp, add 'if ( apCtx -> m_Channels > MAX_CHANNELS ) apCtx -> m_Channels = MAX_CHANNELS;' so both the decoder and the audio output configuration see at most 2 channels (such a stream decodes to noise but stays in bounds).


## GUI, boot and configuration

- [ ] **File-menu import (language / palette / SMB list / skin) destroys the file when it is already at its install location, and a failed import deletes the previously installed file**
  - Ref `hunt-gui-boot-config#1`; issue none; severity high; reproduced on host: yes; independent check: diagnosis check: confirmed; fix check: confirmed; status: open
  - Files: `src/SMS_GUIFileCtxMenu.c`
  - Proposed fix: Rewrite _copy_file_to_mc so the source is read fully into memory and closed BEFORE the destination is opened with O_TRUNC, and remove the destination only when it was opened by us and the write failed (also rejects a 0-byte source instead of installing an empty file). Same size function, no new helpers; exact text is in scratch apply_fixes.py / repro.py (FIXED_COPY). Verified: compiles in the patched private tree with the PS2 toolchain.

- [ ] **Boot-time video rescue (SELECT+R1 = NTSC, SELECT+R2 = PAL) is discarded on every non-memory-card filesystem boot**
  - Ref `hunt-gui-boot-config#2`; issue none; severity medium; reproduced on host: no; independent check: diagnosis check: confirmed; status: open
  - Files: `src/SMS_GUI.c`, `src/SMS_Config.c`, `include/SMS_Config.h`, `src/SMS_IOP.c`, `src/main.c`
  - Proposed fix: Keep the override in one global and re-apply it at the tail of SMS_LoadConfig: `int g_BootDisplayMode = GSVideoMode_Default;` in SMS_Config.c (+ extern in SMS_Config.h); SMS_GUI.c:772 becomes `g_Config.m_DisplayMode = g_BootDisplayMode = lVideoMode;`; after `g_Config.m_BrowserFlags &= ~SMS_BF_UDFL;` in SMS_LoadConfig add `if ( g_BootDisplayMode != GSVideoMode_Default ) g_Config.m_DisplayMode = g_BootDisplayMode;`. Four lines;

- [ ] **IPCONFIG.DAT written on a PC (CRLF lines or double spaces) parses to an empty netmask and gateway = netmask**
  - Ref `hunt-gui-boot-config#3`; issue none; severity medium; reproduced on host: yes; independent check: diagnosis check: confirmed; status: open
  - Files: `src/SMS_IOP.c`
  - Proposed fix: Replace the five strncpy/strlen lines (SMS_IOP.c:1406-1410) with a 3-iteration loop that skips a run of NULs before each field, stops when the buffer is exhausted (missing fields keep their current value) and strncpy()s 15 bytes into g_pDefIP/g_pDefMask/g_pDefGW. Exact text in scratch apply_fixes.py; host run of the fixed text parses all five sample files correctly; compiles in the patched private tree.

- [ ] **IP editor _set_ips never terminates or bounds the octet cells: stale digits after an unsaved edit, write past the 4-byte cell on a long octet**
  - Ref `hunt-gui-boot-config#4`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_GUIMenuSMS.c`
  - Proposed fix: In the loop: `for ( ; *apSrc && *apSrc != '.'; ++apSrc ) if ( j < 3 ) apDst[ i ][ j++ ] = *apSrc; apDst[ i ][ j ] = '\x00'; if ( *apSrc ) ++apSrc;` (3 lines). Fixed text passes the same host run and compiles in the patched tree.

- [ ] **Skin / subtitle-font names of 64+ characters overflow g_Config.m_SkinName / m_MBFName when selected in the menu**
  - Ref `hunt-gui-boot-config#5`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_EE.c`, `src/SMS_GUIMenuSMS.c`
  - Proposed fix: One condition in SMS_EEScanDir (its only users are these two lists): `if ( lLen > 4 && lLen < 68 )` so a base name that cannot fit the 64-byte fields never enters the list (such a skin could not be stored in SMS.cfg anyway). Compiles in the patched tree; tools/test_skin_loading.py still passes.

- [ ] **Dead code and unused statics in boot/config/GUI files (zero behaviour change)**
  - Ref `hunt-gui-boot-config#6`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_IOP.c`, `src/SMS_Config.c`, `src/SMS_GUIDesktop.c`, `src/SMS_GUIMenuSMS.c`, `src/SMS_GUI.c`, `src/SMS_PAD.c`, `include/SMS_PAD.h`
  - Proposed fix: Delete the three IOPInit locals, s_pSMSCfg and the Desktop_Render `i`; wrap the _autonet_handler prototype and body in #ifndef BDM; wrap s_PadBuf0 in #ifndef BDM; delete ReadPadStatus/ReadCombinedPadStatus/ReadCombinedPadStatus_raw/old_pad and their three prototypes (also drop the two old_pad resets in the pad init path).

- [ ] **First-ever USB boot (no SMS.cfg on the drive yet) loads the MX4SIO driver and locks MMCE out for the session**
  - Ref `hunt-gui-boot-config#7`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_IOP.c`
  - Proposed fix: Key the MX4SIO fallback on the device root instead of the config file: after SMS_IOPStartUSB(1) build lDev, `lRootFD = fioDopen(lDev); if (lRootFD < 0) { SMS_IOPStartMX4SIO(1); lRootFD = fioDopen(lDev); }` then keep the existing close / lFSGone handling; drop the lCfgFD probe. Net code shrinks. Not built or tested here.

- [ ] **'Use XH' browser setting is evaluated before the filesystem config is loaded, so it never takes effect on non-memory-card boots**
  - Ref `hunt-gui-boot-config#8`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/main.c`
  - Proposed fix: Owner decision needed: either move the `if ( ... SMS_BF_UXH ) SMS_OSInit ( argv[ 0 ] );` line below SMS_IOPInit() (one-line move, but SMS_OSInit copies a kernel patch to 0x00082000 and its safety after IOP module loading is unverified), or leave as is and document that the option only applies to memory-card boots. Do not move it without a console test.


## CI, docs and release

- [ ] **Converter 'Upload ... to Version Release' steps exit 0 after all 12 uploads fail (stable tag can ship without converter binaries, green check)**
  - Ref `hunt-ci-docs-release#1`; issue none; severity medium; reproduced on host: yes; independent check: diagnosis check pending; status: open
  - Files: `.github/workflows/sms-converter.yml`, `tools/test_converter_publication.py`
  - Proposed fix: In each of the three Version Release steps: replace `break` (lines 166, 180, 310) with `exit 0`, change the trailing `sleep 10` to `if [ "$i" -lt 12 ]; then sleep 10; fi`, and add after `done`: echo "Failed to publish <asset> to release $GITHUB_REF_NAME after 12 attempts" >&2 exit 1 Also add `shell: bash` to the Linux step (line 172) for consistency with the others.

- [ ] **known-issues page and CHANGELOG are stale: contradict the #24 reporter's 2026-11-02 result and omit five merged changes (#49-#53)**
  - Ref `hunt-ci-docs-release#2`; issue #24; severity medium; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `docs-site/content/known-issues.html`, `CHANGELOG.md`, `docs/known-issues.html`, `wiki/Known-Issues.md`
  - Proposed fix: Edit docs-site/content/known-issues.html only (docs/ and wiki/ are generated): - Line 2 lead: 'Status reviewed <date> against master c275476 (the current rolling build), including USB read-ahead (PR #49), the 720p width limit (PR #50), the scrollbar-seek worker stop (PR #51) and poster-list changes (PR #52, #53). The newest console result is the #24 reporter's 2026-11-02 test of 8a25e22;

- [ ] **Manual describes R2 as a two-state list/grid toggle; code cycles three views and hides video JPEG sidecars**
  - Ref `hunt-ci-docs-release#4`; issue #26; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `docs-site/content/video.html`, `src/SMS_GUIFileMenu.c`, `docs/video.html`, `wiki/Video.md`
  - Proposed fix: docs-site/content/video.html lines 109-111: 'While browsing files, press R2 to cycle the normal list, a three-column, two-row poster grid, and a list with the selected item's poster beside it; a third press returns to the normal list with the same file selected.

- [ ] **irx/smbman.irx is a stale pre-#28 driver; the documented regen tool would silently replace the current smbman.irx.xz with it (plus three unused IRX blobs)**
  - Ref `hunt-ci-docs-release#5`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `irx/smbman.irx`, `tools/compress_irx.py`, `irx/bdmfs_vfat.irx.xz`, `irx/mmcedrv.irx`, `irx/mmcedrv.irx.xz`, `Makefile`
  - Proposed fix: `git rm irx/smbman.irx` (source of truth is iop/smbman-sms + prepare_module.py). Optional, provably unused: `git rm irx/bdmfs_vfat.irx.xz irx/mmcedrv.irx irx/mmcedrv.irx.xz`. No Makefile or tool change needed.

- [ ] **lng/ holds 19 legacy language packs that the current build rejects (269 entries vs 311 required); unreferenced duplicate of docs/translation**
  - Ref `hunt-ci-docs-release#6`; issue #27; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `lng/README.txt`, `lng/SMS.lng_ES`, `docs/translation/SMS.lng_ES`, `src/SMS_Locale.c`
  - Proposed fix: `git rm lng/SMS.lng_*` and `git mv lng/README.txt docs/translation/CREDITS.txt`, deleting its first three lines (the obsolete install steps) and keeping the 'Authors are:' list and the thanks line unchanged. Do not add CREDITS.txt to the zip command unless wanted.

- [ ] **builds/SMS-mx4sio-full.elf.pre-smbman.bak: 1.7 MB backup ELF tracked by accident**
  - Ref `hunt-ci-docs-release#7`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `builds/SMS-mx4sio-full.elf.pre-smbman.bak`, `.gitignore`
  - Proposed fix: `git rm builds/SMS-mx4sio-full.elf.pre-smbman.bak`; add `*.bak` and `builds/` to .gitignore next to the existing `build/` line.

- [ ] **Linux converter zip is built with `cp -r tools/sms_converter/*` after the tests ran: ships test byproducts and tests, and the docs promise instructions it does not contain**
  - Ref `hunt-ci-docs-release#8`; issue none; severity low; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `.github/workflows/sms-converter.yml`, `docs-site/content/releases.html`, `tools/sms_converter/crash_log.py`, `tools/sms_converter/test_multitrack.py`
  - Proposed fix: sms-converter.yml lines 77-78: replace the cp with mkdir -p dist-artifact/SMS-Media-Converter-Linux git archive HEAD:tools/sms_converter | tar -x -C dist-artifact/SMS-Media-Converter-Linux rm -f dist-artifact/SMS-Media-Converter-Linux/test_*.py dist-artifact/SMS-Media-Converter-Linux/requirements-win7.txt dist-artifact/SMS-Media-Converter-Linux/.gitignore (works on the depth-1 checkout;

- [ ] **tools/check_devmenu.py (s_DevMenu index cross-check) is not run by any workflow**
  - Ref `hunt-ci-docs-release#9`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `tools/check_devmenu.py`, `.github/workflows/compilation.yml`, `src/SMS_GUIMenuSMS.c`
  - Proposed fix: compilation.yml, 'Host playback and file I/O regression checks' block: add `python3 tools/check_devmenu.py` after line 99 (`python3 tools/test_skin_loading.py`).

- [ ] **developers page says the compilation workflow lists 'all 17 current host regressions'; it lists 44**
  - Ref `hunt-ci-docs-release#10`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `docs-site/content/developers.html`, `docs/developers.html`, `wiki/Developers.md`
  - Proposed fix: Line 23: drop the number and widen the FFmpeg remark: '...; the MOV, JPEG and AAC fixture tests also need FFmpeg/FFprobe. The compilation workflow lists the current host regressions.' Then run `python3 docs-site/build.py`.

- [ ] **Release-publishing workflows: mutable toolchain tag, write token granted to non-publishing jobs, persisted checkout credentials**
  - Ref `hunt-ci-docs-release#11`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `.github/workflows/compilation.yml`, `.github/workflows/sms-converter.yml`, `.github/workflows/test-media.yml`, `.github/workflows/smbman.yml`
  - Proposed fix: compilation.yml: append `@sha256:da02eb0870afcfdb7198195de40d04cd121bd893668c7342fb84b3c4d273e9d1` to the three image references; move `permissions: contents: write` from workflow level to the `build` job and give the workflow `permissions: contents: read`. In compilation.yml, sms-converter.yml and test-media.yml add `with: persist-credentials: false` to each actions/checkout step (all publishing goes through `gh` with GH_TOKEN;

- [ ] **Every push to a same-repo PR branch runs build, sms-converter and smbman twice (push + pull_request)**
  - Ref `hunt-ci-docs-release#12`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `.github/workflows/compilation.yml`, `.github/workflows/sms-converter.yml`, `.github/workflows/smbman.yml`
  - Proposed fix: In the three workflows change the push filter to `branches: [master]` (keep `tags: ['v*']`, `pull_request` and `workflow_dispatch`). Branches without a PR can still be built on demand via workflow_dispatch.


## Dead code and binary size

- [ ] **Dead IRX modules still embedded in g_DataBuffer (SMSSMB always; UMS and USBD under BDM)**
  - Ref `hunt-bloat-deadcode#1`; issue none; severity medium; reproduced on host: yes; independent check: diagnosis check: confirmed; status: open
  - Files: `src/SMS_Data.c`, `include/SMS_Data.h`, `src/SMS_IOP.c`
  - Proposed fix: In src/SMS_Data.c delete the SMSSMB.IRX block and wrap the UMS.IRX and USBD.IRX blocks in `#ifndef BDM ... #endif`. In include/SMS_Data.h remove SMS_SMB_SIZE/SMS_SMB_OFFSET, chain SMS_SMSUTILS_OFFSET from `SMS_VU0_DATA_OFFSET + SMS_VU0_DATA_SIZE`, and under `#ifdef BDM` define `SMS_USB_MASS_SIZE 0` and `SMS_USBD_SIZE 0` so the offset chain collapses. Do not touch SMS_SIO2MAN (it sits inside the audio ring-buffer region).

- [ ] **Legacy IPU icon arrays, six icon tables and IPU_UnpackImage are dead after the RGBA icon blob replaced them**
  - Ref `hunt-bloat-deadcode#2`; issue none; severity medium; reproduced on host: yes; independent check: diagnosis check: confirmed; status: open
  - Files: `src/SMS_GUIcons.c`, `src/SMS_IPU_1.c`, `include/SMS_IPU.h`, `src/SMS_PlayerBallSim.c`
  - Proposed fix: Delete src/SMS_GUIcons.c lines 36-1368 (the arrays and six tables) and fix the comment at lines 25-28; delete the unused extern at src/SMS_PlayerBallSim.c:26; delete IPU_UnpackImage from src/SMS_IPU_1.c:220-324 and its prototype at include/SMS_IPU.h:140.

- [ ] **Small dead-code batch: orphaned asm round-rect, unused DS3/DS4 wrappers, dead pad helpers and BDM-unused statics**
  - Ref `hunt-bloat-deadcode#3`; issue none; severity low; reproduced on host: yes; independent check: defect confirmed, proposed fix disputed; status: open
  - Files: `src/SMS_GS_0.S`, `include/SMS_GS.h`, `src/SMS_GS_1.c`, `src/libds34usb.c`, `src/libds34bt.c`, `include/libds34usb.h`, `include/libds34bt.h`, `src/SMS_PAD.c`, `src/SMS_GUI.c`, `src/SMS_GUIMenuSMS.c`, `src/SMS_Config.c`, `src/lzma2.c`, `src/SMS_IOP.c`, `src/SMS_GUIDesktop.c`
  - Proposed fix: Delete (a)-(c) and (f) with their prototypes; fix the stale comment at src/SMS_GS_1.c:608-609. Wrap s_PadBuf0/g_PadBuf1 in `#ifndef BDM`. Wrap _autonet_handler in `#ifndef BDM` including its forward declaration at src/SMS_GUIMenuSMS.c:74 (the trial wrapped only the definition and got a new "declared static but never defined" warning). Remove the unused locals in (g).

- [ ] **SMS_ExitCrumb call sites and their string literals are compiled into release although the function body is empty**
  - Ref `hunt-bloat-deadcode#4`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_IOP.c`, `include/SMS_IOP.h`, `src/SMS_GUIMenuSMS.c`, `src/SMS_Player.c`
  - Proposed fix: In include/SMS_IOP.h replace the prototype at line 73 with: #ifdef SMS_DIAG void SMS_ExitCrumb ( int, const char* ); #else #define SMS_ExitCrumb( n, s ) ( ( void )0 ) #endif and wrap the definition in src/SMS_IOP.c:271-299 in `#ifdef SMS_DIAG ... #endif`.

- [ ] **Comments that misdescribe the code after the icon, round-rect and diagnostics rewrites**
  - Ref `hunt-bloat-deadcode#5`; issue none; severity low; reproduced on host: yes; independent check: not yet checked; status: open
  - Files: `src/SMS_GUIcons.c`, `src/SMS_GS_1.c`, `src/SMS_IOP.c`, `src/SMS_Player.c`, `tools/bake_icons_rgba.py`
  - Proposed fix: Correct or delete each comment in the same commits as the three cleanups above so the text matches the code; after the SMS_ExitCrumb macro fix the zero-cost statement becomes true and can stay.

- [ ] **In-place network switch resets the IOP but restarts only the network stack; comment about dev9 reload is false (code reading only)**
  - Ref `hunt-bloat-deadcode#6`; issue none; severity medium; reproduced on host: no; independent check: not yet checked; status: open
  - Files: `src/SMS_IOP.c`, `src/SMS_GUIMenuSMS.c`, `src/SMS_Player.c`, `src/SMS_ContainerMPEG_PS.c`
  - Proposed fix: Do not change code on this evidence alone. First confirm what SMS_IOPRESET_SWITCH actually tears down (read SMS_IOPReset for that mode); if it is a full IOP reboot, either reload dev9/audsrv/poweroff in SMS_IOPNetSwitch the way SMS_IOPInit does, or keep those images outside the region reused for audio. At minimum correct the comment at line 1805.
