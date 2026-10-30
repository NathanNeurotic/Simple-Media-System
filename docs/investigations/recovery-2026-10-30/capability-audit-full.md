{
  "summary": "Derive SMS playback ceilings from PS2 source + verify converter defects empirically",
  "agentCount": 10,
  "logs": [],
  "result": {
    "synthesis": "# SMS (PS2) OUTPUT CAPABILITY SPECIFICATION v1\n\n**Status of evidence.** Every claim below is tagged:\n\n- **[SRC]** — read directly out of the SMS C sources in this worktree.\n- **[SRC‑V]** — I re‑read the cited source myself in this session and confirmed it (not just inherited from an audit).\n- **[EMP]** — measured with ffmpeg 8.1.1‑full_build‑www.gyan.dev / ffprobe on this machine.\n- **[EMP‑V]** — measured by me in this session.\n- **[UNCONF]** — not established by source or measurement. Do not build a hard rule on it.\n\n**HARD** = SMS cannot play it (reject, crash, hang, or decode garbage). **SOFT** = plays but degrades (frame drops, wrong speed, silent substitution).\n\nNothing here is validated on real PS2 hardware. Every \"HARD\" is derived from source control flow or from ffmpeg's observable output, not from a console.\n\n---\n\n## 0. GLOBAL CEILINGS (apply to every container and codec)\n\n| Limit | Value | Kind | Evidence |\n|---|---|---|---|\n| Video width | ≤ 1024 | **HARD** | `src/SMS_Player.c:1676-1681` — `if (m_Width > 1024 \\|\\| m_Height > 1024) { m_pErrorMsg = &STR_RESOLUTION_TOO_BIG; goto destroy; }`. Strict `>`, so 1024 passes, 1025 fails. Runs after `SMS_CodecOpen`, before decoder Init → clean rejection, codec‑independent. **[SRC‑V]** |\n| Video height | ≤ 1024 | **HARD** | same line |\n| Padded frame area (32‑bit output) | `ceil(W/64)*64 × ((H+31)&~31)` ≤ ~741,376 px | **HARD in practice** | `src/SMS_IPU.c:735,751-757`: `lImgSize = ((lTBW<<6) * ((aHeight+31)&~31) * (4>>lf16)) >> 8; lVRAM = 0x4000 - lImgSize;`. Display FB top for 640×480 PSMCT24 = 4800 blocks, leaving 11,584 of 16,384. The auto 16‑bit fallback at `:754` fires **only** when the display is already 16‑bit. 720×576 (442k) and 1024×576 (590k) fit; 1024×768 (786k) and 1024×1024 do not. **[SRC]** |\n| Streams per file | ≤ 8 (**≤ 7 for MPEG‑PS**) | **HARD** | `include/SMS_Container.h:56,79`. MPEG‑PS is the exception: `src/SMS_ContainerMPEG_PS.c:743 apCont->m_pStm[apCont->m_nStm++] = lpStm;` is **unguarded** — the `< SMS_MAX_STREAMS` test at `:762` only controls a `continue`. I re‑read both. At exactly 8, `m_pStm[8]` aliases the low word of `int64_t m_Duration` (offset 32), which is `0x7FFFFFFFFFFFFFFF` when no DTS was found → the NULL‑terminator scans at `:486`/`:559` deref `0xFFFFFFFF`. **[SRC‑V]** |\n| Single video packet payload | ≤ **2,097,088** bytes | **HARD** | `SMS_Player.c:54` (2 MiB ring) + `SMS_Container.c:53` (`aSize + 64`) + `SMS_RingBuffer.c:74-76` (`(aSize+63)&~63 <= m_Capacity`, else NULL). 2,097,152 − 64. **[SRC]** |\n| Single audio packet payload | ≤ **524,224** bytes | **HARD** | `SMS_Player.c:55` + same math. 524,288 − 64. **[SRC]** |\n| File size | ≤ **2,147,483,647** bytes (2 GiB − 1) | **HARD** | `src/SMS_FileContext.c:1307-1325` — `static int STIO_Seek(FileContext*, unsigned int aPos)` passes `aPos` straight into `fioLseek(fd, aPos, SEEK_SET)` / `fileXioLseek(...)`, both of which take a **signed int** offset. A repo‑wide grep finds **no** `fileXioLseek64` / `fioLseek64` call anywhere in `src/`. `FileContext.m_Size/m_Pos/m_CurPos` are `unsigned int` (`include/SMS_FileContext.h:66-68`), so representation wraps at 4 GiB, but seeking breaks at 2 GiB — and AVI/MOV seek during *ordinary playback*, not just user scrubbing. **[SRC‑V]** |\n| Decoded output channels | 2 (stereo, 16‑bit interleaved) | **HARD** | Every decoder caps at 2: AC3 `src/SMS_AC3.c:2576-2585` (2‑entry `s_FloatToInt`), DTS `src/SMS_DTS.c:11556-11565`, AAC `src/SMS_AAC.c:8197` (`output_channels = 2`), Vorbis `src/SMS_OGG.c:5244-5246`, FLAC `src/SMS_FLAC.c:438-441`, MPA structurally 1‑2 (`src/SMS_MP123.c:5306`). Player pre‑clamps AC3/DTS/AAC at `src/SMS_Player.c:100-114`. **[SRC]** |\n| Reference pictures | 3 slots → max 2 simultaneous refs | **HARD** | `include/SMS_MPEG.h:96 #define MAX_PICTURE_COUNT 3`; `SMS_MPEG.c:181-183` does **not** NULL‑check `FindUnusedPic()`. Consecutive B‑frames are fine (non‑refs recycle each frame); **B‑pyramid is an unchecked NULL deref.** **[SRC]** |\n| A/V presentation window | video PTS within `[audio − 200 ms, audio + 80 ms]` | **SOFT** | `src/SMS_IPU.c:272-281` |\n| Aspect ratio | ignored by both video decoders | **HARD (encode square pixels)** | MPEG‑4 `m_SampleAspectRatio` is written and never read (`SMS_MPEG4.c:800-807`); libmpeg discards `aspect_ratio_information` (`src/libmpeg.c:266`). The **only** aspect signal is `m_fWS`, set exclusively by the MPEG‑PS demuxer when `aspect_ratio_information == 3` (`SMS_ContainerMPEG_PS.c:304`). AVI and MP4 have **no** way to signal anamorphic. **[SRC]** |\n\n---\n\n## 1. CEILING TABLE — PER CONTAINER\n\n### 1.1 `.avi` — `src/SMS_ContainerAVI.c`, routed at `src/SMS_FileDir.c:130`\n\n**Legal video codecs — exactly two.** Identity comes only from `BITMAPINFOHEADER.biCompression`, read at `SMS_ContainerAVI.c:595` and resolved at `:611` against `s_CodecVideoTags` (`src/SMS_Codec.c:32-55`, 21 real entries). **[SRC‑V]**\n\n| FourCC set | → codec | Verdict |\n|---|---|---|\n| `DIVX DX50 XVID MP4S M4S2 0x00000004 DIV1 BLZ0 mp4v UMP4 FMP4` | MPEG‑4 Part 2 | legal |\n| `DIV3 MP43 MPG3 DIV5 DIV6 DIV4 AP41 COL1 COL0` | MS‑MPEG4v3 | legal |\n| `DXSB` | VobSub subtitle | legal, see ordering rule |\n| **anything else, including `mpg1 mpg2 MPEG H264 avc1`** | `SMS_CodecID_NULL` | **HARD — silent** |\n\nAn unmatched FourCC is **not** an error: `SMS_CodecOpen` returns 0 (`SMS_Codec.c:117-121`), the player jumps to `addSub` (`SMS_Player.c:1670-1703`) and falls back to audio‑only, or shows the generic `STR_UNSUPPORTED_FILE`. Symptom = \"sound plays, black screen\". **[SRC]**\n\n**Legal audio — `wFormatTag` is read as a 16‑bit value** (`src/SMS_Container.c:290 File_GetUShort`), so only table entries whose upper two bytes are zero are reachable.\n\n| wFormatTag | codec | Verdict | Note |\n|---|---|---|---|\n| `0x0001` | PCM S16 **LE** | legal | `PCM16BE` has no tag entry |\n| `0x0050` | MP2 | legal | |\n| `0x0055` | MP3 | legal | |\n| **`0x00FF`** | **AAC** | **legal — see §2** | `SMS_Codec.c:67` **[SRC‑V]**; ffmpeg emits exactly this **[EMP‑V]** |\n| `0x0160` / `0x0161` | WMA v1 / v2 | legal | untested by the converter |\n| `0x2000` | AC‑3 | legal | |\n| `0x2001` | DTS | legal | ffmpeg's RIFF tag for DTS is 0x2001 |\n| `0x674F` | Vorbis | *table entry exists but* **unreachable from ffmpeg** | **HARD**, see below |\n| `0x6134706D` (`mp4a`), `0x43614C66` (`fLaC`) | AAC / FLAC | **unreachable** | 32‑bit tags vs a 16‑bit read |\n| `0x0070 0x0075 0x0141 0x474F 0x476F 0x676F` | *alias collisions* | **HARD — mis‑decode** | `SMS_CodecGetID` `toupper()`s every byte (`SMS_Codec.c:78-81`), so `'P'/'p'`, `'U'/'u'`, `'A'/'a'`, `'O'/'o'`, `'G'/'g'` collide. Never emit these. **[SRC‑V]** |\n\n> **Vorbis in AVI is broken.** ffmpeg's AVI muxer writes `wFormatTag 0x566F` (`'oV'`) — measured, `codec_tag 0x566f`. SMS's only Vorbis row is `0x674F`. Byte 0 matches under `toupper` (`0x6F`↔`0x4F`) but byte 1 does not (`toupper(0x56)=0x56` vs `toupper(0x67)=0x47`) → `SMS_CodecID_NULL` → completely silent file, validator‑clean. **HARD. [SRC‑V + EMP]**\n\n**Dimension / rate / bitrate cells**\n\n| Cell | Value | Kind | Evidence |\n|---|---|---|---|\n| Max W×H | 1024×1024 | HARD | global |\n| Recommended max | 720×576 (~8.0 MiB decoder heap) or 640×480 (~6.0 MiB) | SOFT | `SMS_VideoBuffer.c:33-59`, `SMS_MPEGInit.c:28-29`, `SMS_MPEG4.c:496-501` |\n| Granularity | even (yuv420p requirement); **multiple of 16 recommended** | SOFT | `m_MBW = (W+15)/16` |\n| MSMPEG4v3 min height | **≥ 144** | **HARD** | `SMS_MSMPEG4.c:2139-2143`: `m_SliceHeight = m_MBH / (lCode - 0x16)`, divisor up to 9. MBH ≤ 8 → 0 → `m_MBY % 0` and zero macroblocks decoded. |\n| Frame rate | any; `strh dwRate ≠ 0` and `dwScale ≠ 0` | **HARD** | `SMS_ContainerAVI.c:537-540` → `m_TimeBase.m_Den` is a divisor in every `SMS_Rescale`. The **video** path has a 25/1 fallback (`:479-493`); the **audio** path has none. |\n| `nAvgBytesPerSec` | **≠ 0** | **HARD** | `SMS_Container.c:296` → `m_BitRate`; divided by unconditionally at `SMS_Container.c:191-199`, *before* the `if (lDen && lNum)` guard |\n| Max video bitrate | no coded ceiling; single frame < 2,097,088 B | HARD | ring buffer |\n| Practical video bitrate | 1,500–2,500 kbps | SOFT | 2 MiB ring absorbs read jitter; USB/MX4SIO/SMB reads are **synchronous, single‑buffered, 4 KiB per IOP call** (`src/SMS_FileContext.c:22,1353-1360,1476-1477`) |\n| Audio sample rate | passed straight through to the IOP | see §1.9 | `SMS_Player.c:188-191` → `SPU_InitContext`. **Nothing resamples on the EE.** |\n| Audio channels | ≤ 2 | HARD | global |\n\n**AVI structural rules — all HARD, all from `src/SMS_ContainerAVI.c`**\n\n1. `avih.dwStreams` ≤ 8, and the count of `strh` chunks must **exactly** equal it (`:437-442`, `:720-722`) — otherwise the file is rejected outright.\n2. `strh.fccType` ∈ {`vids`,`auds`,`txts`} — anything else hits `default: goto error` (`:455-566`). `txts` streams are forced to `SMS_CodecID_NULL` (`:629-631`) and are inert.\n3. Video `strf` must be **≥ 40 bytes**. `lLen = lSize - 40` is `uint32_t` (`:603-605`); a 16‑byte strf wraps to `0xFFFFFFE8` and seeks backwards.\n4. **Video extradata is discarded.** No `m_pUserData` assignment exists on the AVI video path (`:586-611`). ⇒ **VOL/VOS must be in‑band.** See §1.3 for why this matters, and note AVI gets it right for free.\n5. Audio `strf` ≥ 14 bytes; ≥ 18 for AAC so the `cbSize` block carries the AudioSpecificConfig; `cbSize` ≤ 32767 (`m_UserDataLen` is a signed `short`, `include/SMS_Codec.h:81`).\n6. Inside `movi`, **only** `NNdc NNdb NNwb NNsb NN__` are recognised, plus `ixNN` as skippable (`:782-816`). `JUNK`, `LIST rec ` and anything else is **not skipped** — the demuxer byte‑slides over it.\n7. **Crash trap:** the stream‑index bound check at `:796-797` is dead (it reads `lData[2],lData[3]` = the `dc`/`wb` letters, giving 571, so the test always contributes 0). `:805` then recomputes `lCount ∈ [0,99]` and indexes an 8‑element array unchecked at `:818`. ⇒ **the two‑digit stream index must be zero‑padded and < stream count and ≤ 7.**\n8. OpenDML `indx` stream id must be **strictly less** than the stream count — the guard at `:251-263` is `>` not `>=` → NULL deref at the boundary. A parsed `indx` also **replaces** `idx1` entirely (`:727-729`).\n9. `idx1` offsets must be **movi‑relative** (ffmpeg convention, first entry = 4) (`:421-422`, `:1154`), and `idx1` must contain entries for **every** stream or seeking is disabled for the whole file (`:1123-1131`).\n10. **Seek trap:** `_CheckOffsetBug` (`:377-387`) reads two bytes at `m_pIdx[0].m_Pos` (= file offset 4 and 5, i.e. the low two bytes of `dwRIFFSize`) and sets `m_fOffsetBug` if either equals `'0'` (0x30). Probability ≈ 0.78 % of all file sizes. When it fires, **every seek in that file is wrong.** Cheap deterministic fix: pad the file until neither byte 4 nor byte 5 is 0x30 and rewrite `dwRIFFSize`.\n11. ≤ 8 RIFF segments for OpenDML (`:62-64`, `:341`).\n12. **DXSB subtitle streams must come *after* the `vids` stream** in strl order, or they are silently dropped (`SMS_Codec.c:101 case DXSB: return 1;` combined with `SMS_Player.c:1668-1703`). Max 8 subs (`SMS_Player.c:1531`).\n13. **Do not mux DXSB alongside MPEG‑4/MSMPEG4v3 video** — `src/SMS_DXSB.c:209,274` writes a subtitle scanline over `SMS_SPR_FREE` = `0x70001B30`, which is exactly `g_MPEGCtx.m_pMBIntraTbl` (`src/SMS_MPEGInit.c:64`).\n\n**FourCC choice is not cosmetic — corrected finding.** The bug‑compat autodetect is **on by default** (`src/SMS_MPEGInit.c:25 g_MPEGCtx.m_Bugs = SMS_BUG_AUTODETECT;`) **[SRC‑V]**. At `SMS_MPEG4.c:3251-3255`, if no `XviD<build>` or `DivX...` user‑data string was parsed **and** the tag is `XVID`/`XVIX`, `m_XviDBuild = -1`, which satisfies `<=1`, `<=12` and `<=32` at `:3281-3283` and turns on `SMS_BUG_QPEL_CHROMA | SMS_BUG_EDGE | SMS_BUG_DC_CLIP`. `SMS_BUG_EDGE` replaces the macroblock‑unit MV clamp with a pixel‑unit one (`:2693-2698` vs `SMS_MPEGInit.c:33-34`) → out‑of‑bounds reference reads.\n\nI measured which encoders emit the string **[EMP‑V]**:\n\n| Encoder | user_data | `m_XviDBuild` | Bug flags | `-vtag XVID` verdict |\n|---|---|---|---|---|\n| `libxvid` | `XviD0074` | 74 | none | **SAFE** |\n| ffmpeg native `mpeg4` | `Lavc62.28.101` | **−1** | EDGE + QPEL_CHROMA + DC_CLIP | **UNSAFE** |\n\n⇒ **`-vtag XVID` is safe only when the encoder is actually libxvid.** If libxvid is unavailable and the converter falls back to native `mpeg4`, it must switch the tag to `FMP4` (in SMS's table, trips neither the XVID nor the DIVX heuristic). ffmpeg accepts `-vtag FMP4`, `-vtag DX50`, `-vtag MP4S` for AVI **[EMP‑V]**. Also never emit plain `DIVX` — `SMS_MPEG4.c:3257-3262` sets `m_DivXVer = 400` → `SMS_BUG_EDGE` again.\n\n---\n\n### 1.2 `.mpg` / `.mpeg` — `src/SMS_ContainerMPEG_PS.c`\n\nNo magic‑number check; acceptance is content‑driven (`:639-656`). Extension only reorders the probe list (`SMS_FileDir.c:132,169`).\n\n| Cell | Value | Kind | Evidence |\n|---|---|---|---|\n| Video codec | **MPEG‑1 or MPEG‑2 only**, on stream id `0x1E0`, **exactly one** | HARD | `:697-702` — first `0x1E0-0x1EF` latches; `SMS_CodecID_MPEG2` for both, and `SMS_Codec.c:98-99` routes MPEG1 and MPEG2 to the same `SMS_Codec_MPEG12_Open` |\n| Audio codec | MP1/2/3 on `0x1C0`; AC‑3 on `0x80`; DTS on `0x88`; LPCM16BE on `0xA0`. **Exactly one.** | HARD | `:703-725`; `lfAudio` is *assigned* not OR‑ed at `:752`, so the **last** audio stream decides acceptance |\n| **Both a video and an audio stream are required** | video‑only or audio‑only `.mpg` is rejected | **HARD** | `:774-782` |\n| Program Stream Map | **do not emit one** | HARD‑ish | `:664-695` recognises only stream_type 0x01/0x02/0x03/0x04/0x81 — no DTS, no LPCM. A PSM entry for id `0xBD` also disables substream extraction (`:256-274`), making AC‑3/DTS/LPCM undetectable. ffmpeg's `-f mpeg`/`-f vob`/`-f dvd` emit **no PSM** — verified, `PSM(BC)=0` **[EMP‑V]** |\n| Sequence header | must be discoverable in the first video PES | **HARD** | `:293-304`; search abandons after 1024 PES headers (`:318`) or 100,000 bytes per scan (`:25`) |\n| GOP header before every I‑frame | strongly recommended | **SOFT (seek)** | `:609-614` — the picture_coding_type test does a native little‑endian 32‑bit load and then extracts big‑endian bit positions, so it is wrong. The `0x1B8` (GOP) and `0x1B3` (seq) tests in the same condition are byte‑order independent and **do** work. ffmpeg emits `seqhdr=5, gop=5` for a 5‑GOP clip **[EMP‑V]** |\n| Frame rate | `frame_rate_code` **1..8 only**; **avoid code 1** | HARD / SOFT | `src/libmpeg.c:43-48` — codes 0 and 9‑15 are 0.0 → divide‑by‑zero into COP0 Compare. **Table entry 1 is coded `23.0F*1000.0F/1001.0F` = 22.977, not 23.976** → 23.976 fps content plays ~4.3 % slow. Prefer 25 (code 3), 29.97 (code 4), 30 (code 5) |\n| Chroma | **4:2:0 only** | HARD | `libmpeg.c:439` parses `m_ChromaFmt` and **never reads it**; the IPU is 4:2:0 hardware and `_MPEGMacroBlock8` is a fixed 6‑block layout (`include/libmpeg_internal.h:68-74`) |\n| Interlaced MPEG‑2 | fully supported; **height must be a multiple of 32** | HARD | `libmpeg.c:148-161` rounds `2*ceil(H/32)` MB rows and overwrites `m_SI.m_Height` |\n| Progressive height | multiple of 16 | HARD | same |\n| Max video bitrate | PES length is 16‑bit → ≤ 65,535 per packet; use conventional 2048‑byte packs | HARD | `:173`; ample ring headroom |\n| MPEG audio | **no free format** | HARD | `:344` zeroes `m_FreeFmtFrameSize`; `src/SMS_MP123.c:5337-5339` then fails |\n| AC‑3 | bsid ≤ 11 (**no E‑AC‑3**), frmsizecod < 38 (**≤ 640 kbps**), 32000/44100/48000 | HARD | `src/SMS_AC3.c:390,399` |\n| LPCM | **16‑bit only**; rates {48000, 96000, 44100, 32000} | HARD | `:411-424` reads the quantization field and ignores it; `src/SMS_PCM.c:35-36` is an unconditional 16‑bit byteswap. **Never emit >2‑ch LPCM** — PCM is the one path with no channel clamp anywhere |\n| Audio PES payload | **≥ 5 bytes**, always | **HARD — hangs the browser** | `:348-350` does `aLen -= 4` then `while (aLen && !retVal) { ...; --aLen; }`. Payload 1/2/3 → `aLen` goes negative and never reaches 0. **Infinite loop during file probing.** Keep audio PES at the conventional ~2 KB; also avoid ≥ 64 KB audio PES because `:340` declares `char lBuf[aLen]` — a VLA sized by untrusted input |\n| Audio parameters | must be findable in the **first** audio PES payload | HARD | `:346-441` reads exactly one payload |\n| Stream discovery window | every stream must appear inside the first **1 MiB** and within 1024 PES headers | HARD | `:26`, `:650`, `:762-764` |\n| PES_scrambling_control | must be 00 | HARD | `:214-216` |\n| Elementary streams | ≤ **7** | HARD | see global table |\n| Duration / seek | PTS on essentially every packet, and PTS for every stream in the **last 131,071 bytes**; do not wrap or restart the 33‑bit PTS | SOFT | `:807-826`; seek is a linear byte‑offset estimate, so CBR helps |\n\nffmpeg `-f mpeg`, `-f vob` and `-f dvd` all place AC‑3 on private_stream_1 **substream 0x80** — verified by re‑implementing SMS's own `_read_header` walk over the produced files **[EMP‑V]**.\n\n---\n\n### 1.3 `.mp4` — `src/SMS_ContainerMOV.c`\n\n| Cell | Value | Kind | Evidence |\n|---|---|---|---|\n| File must start with `ftyp` and a non‑zero major_brand | required | **HARD** | `:1011-1027`, `:1259-1261`. A bare‑`moov` QuickTime file is rejected |\n| Extension | **must be `.mp4`** | **HARD** | `src/SMS_FileDir.c:150-153`. There is **no `.mov` string anywhere in the tree** (I listed all `s_p*` extension literals — confirmed). `.mov`/`.m4v` fall into the default probe order where the **M4A** creator sits at index 3, before MOV at index 11 → every video trak is freed and you get audio‑only playback |\n| Video codec | **`mp4v` (MPEG‑4 Part 2) ONLY** | **HARD** | `:286 m_ID = SMS_CodecGetID(m_Type, lFmt)` against the same 21‑entry FourCC table. `avc1`/`hvc1`/`mp2v`/`s263` → NULL |\n| **MPEG‑1 / MPEG‑2 video in MP4** | **FORBID** | **HARD** | ffmpeg tags them `mp4v` with esds objtype `0x6A`/`0x61`. But esds is **never dispatched for a video sample entry** (below), so `m_ID` stays `SMS_CodecID_MPEG4` and an MPEG‑2 elementary stream is fed to the MPEG‑4 Part 2 software decoder |\n| **Video `esds` / extradata never reaches the decoder** | confirmed | **HARD** | **I recounted this from source myself.** `:277-292` consumes 4+4+8+16+2+2+46+2 = **84** bytes; an ISO `VisualSampleEntry` header is **86** (16 SampleEntry + 70). The trailing 2‑byte `pre_defined` (0xFFFF) is left, so `_mov_read_dflt` reads a box size of `0xFFFF0000`, `-= 8`, and `:769 if (lAtom.m_Size < 0 \\|\\| ...) break;` exits before ever matching `esds`. Compare the **audio** branch at `:294-299`, which consumes exactly 36 and lands correctly. **This resolves the contradiction with commit 99b1b5f: the commit is wrong for video sample entries.** **[SRC‑V]** |\n| ⇒ **VOL/VOS must be in‑band** | required | **HARD** | And ffmpeg does **not** do it by default. Measured **[EMP‑V]**, 2 s / 50 frames / g=12, counting `00 00 01 B0` (VOS) and `00 00 01 20` (VOL) in the demuxed ES: `mpeg4 → mp4`: **VOS 0, VOL 0** (47 bytes of extradata in esds). `libxvid → mp4`: **VOS 0, VOL 0**. `-flags:v -global_header`: **no effect** (mov forces it). `mpeg4 → avi`: **VOS 5, VOL 5** — one per keyframe. `libxvid → avi`: **VOS 5, VOL 5**. **Fix:** `-bsf:v dump_extra=freq=keyframe` → **VOS 5, VOL 5** in MP4, tag still `mp4v` |\n| Audio codec | **`mp4a` with esds** | HARD | `:386-391`; objtype **must** be `0x40` (AAC), `0x69` or `0x6B` (MP3). Anything outside `{32,64,96,97,102,103,105,106,107,221,226}` wipes `m_ID` to NULL (`_mp4_get_id`, `:359-368`) |\n| MP3 in MP4 | **works** | legal | Measured: tag `mp4a`, esds objtype **`0x6B` = 107**, avgBitrate `0x0001F863` (non‑zero), ES_Descr flags byte `0x00`, no DecSpecificInfo — exactly what `_mov_read_esds` (`:382-404`) expects. **[EMP‑V + SRC‑V]** |\n| **AC‑3 in MP4/M4A** | **FORBID** | **HARD** | ffmpeg writes an `ac-3` sample entry + `dac3` box and **no esds** (box scan: `ac-3` True, `dac3` True, `esds` False, `mp4a` False) **[EMP]**. `'ac-3'` = 0x332D6361, not in `s_CodecAudioTags`; and `s_MOVParseTbl` (`:700-724`) has no `ac-3`/`dac3` handler, so there is nothing to recover the id from |\n| **Vorbis in MP4/M4A** | **FORBID** | **HARD (recommended)** | objtype `0xDD` = 221 does map to `SMS_CodecID_OGGV`, so the id resolves — but whether SMS's Vorbis decoder can init from the MP4‑packed DecoderSpecificInfo is **[UNCONF]**. Not worth shipping |\n| ES_Descriptor flags byte | must be **0** | HARD | `:382-386` skips a fixed 3 bytes; any streamDependence/URL/OCR flag desynchronises the descriptor walk |\n| `ctts` | version 0, **non‑negative** offsets; safest `-bf 0` | **HARD** | `:571-582` — the first negative duration zeroes `m_nCtts` while leaving `m_pCtts` allocated, and `:1081-1090` gates only on the pointer → every PTS uses entry 0 of a partly/never‑initialised table |\n| Video `stts` | non‑empty, first delta > 0, `mdhd` timescale > 0; single entry (CFR) | HARD | `:454-461` → `SMS_MPEG4.c:513-524` divides into COP0 Compare |\n| Audio `stts` | single entry **and** non‑zero esds avgBitrate | HARD | `:981-983` + `SMS_Container.c:193-194` |\n| `stsd` | **exactly one** sample entry per trak | HARD | `:281-286` — later entries are skipped, and the lock‑out is on `m_Tag`, set even for unknown 4CCs |\n| `hdlr` | `vide` or `soun` only | HARD | `:227-234`; unknown handlers leave `m_Type = Unknown`, and `SMS_Codec.c:74` then uses the **audio** table |\n| traks | ≤ 8; the 9th is silently skipped | HARD | `:182-185` |\n| `moov` before `mdat` | strongly recommended | **SOFT** | `:608-619` — `File_Skip` over `mdat` is a **512‑byte read loop**, not a seek (`src/SMS_FileContext.c:1718-1744`). A 700 MB mdat = 1,433,600 iterations. Use `-movflags +faststart` |\n| Fragmented MP4 (`moof`) | **FORBID** | HARD | no `moof` entry in `s_MOVParseTbl` |\n| Edit lists (`elst`) | silently discarded | SOFT | `:708` routes it to `_mov_read_dflt` |\n| Per‑sample size | ≤ 2,097,088 (video) / 524,224 (audio chunk) | HARD | global |\n| Resolution | ≤ 1024×1024, from the `stsd` u16 width/height | HARD | `:289-290` + `SMS_Player.c:1676` |\n\n**Net for `.mp4`: MPEG‑4 Part 2 video with `-bsf:v dump_extra=freq=keyframe`, AAC or MP3 audio, `-bf 0`, `+faststart`, exactly 2 traks. Everything else is forbidden.** Given the extradata trap and the fact that AVI gets in‑band VOL for free, **`.avi` is the safer MPEG‑4 carrier and should stay the recommended preset.**\n\n### 1.4 `.m4a`\n\nAudio‑only. `SMS_FileDir.c:150` routes it to `SMS_CONTAINER_M4A`, which **frees every video trak** (`SMS_ContainerMOV.c:252-258`). Same audio rules as `.mp4`: `mp4a` + esds objtype `0x40` (AAC) or `0x6B` (MP3). AC‑3 and Vorbis: **FORBID**. ≤ 2 channels, `-movflags +faststart`.\n\n> Latent bug, not a format rule: `SMS_M4AProbe` hands a caller‑owned `SMS_AudioInfo*` to `MOVContext.m_pMDAT` (`:1319`) and `_mov_read_header` unconditionally `free()`s it (`:1005`). On the `.m3u` path that pointer is a **stack local** (`src/SMS_ContainerM3U.c:128`). ⇒ **Do not generate `.m3u` playlists referencing converted `.m4a`/`.mp4` files** until this is fixed.\n\n### 1.5 `.mp3` — `src/SMS_ContainerMP3.c`\n\n| Cell | Value | Kind |\n|---|---|---|\n| Codec | MPEG‑1/2/2.5 Layer I, II, III | HARD |\n| Channels | 1 or 2 (structural) | HARD — `src/SMS_MP123.c:5306` |\n| Sample rates | 44100/48000/32000 (MPEG‑1), 22050/24000/16000 (MPEG‑2), 11025/12000/8000 (MPEG‑2.5). Reserved index 3 rejected | HARD — `src/SMS_MP123.c:49,5294,4795` |\n| Bitrates | full standard tables (L1 ≤ 448, L2 ≤ 384, L3 ≤ 320 for MPEG‑1) | HARD — `src/SMS_MP123.c:50-61` |\n| **Free format** | **FORBID** | HARD — a frame > 1792 B is silently **truncated** (`:6684`, `include/SMS_MP123.h:26`) |\n\n### 1.6 `.flac` — `src/SMS_ContainerFLAC.c`\n\n| Cell | Value | Kind | Evidence |\n|---|---|---|---|\n| Channels | **≤ 2** | **HARD — file will not open** | `:114 lData.m_Info.m_nChannels < 3`. This gate is load‑bearing: the decoder has exactly two channel buffers and a 2‑entry synth table (`src/SMS_FLAC.c:40,438-441`) |\n| Bit depth | 16 ideal; 24 works; **32 = undefined behaviour** | HARD | `src/SMS_FLAC.c:518 lShift = 24 - m_BPS` then `(x << aShift) >> 8` at `:424`. **Not reachable from ffmpeg:** an s32 input encodes at **bps 24**, measured **[EMP‑V]** |\n| STREAMINFO max frame size ≠ 0 | required | HARD in principle | `src/SMS_FLAC.c:488-500` infinite‑loops on 0. **Not reachable from ffmpeg:** file output → maxFrameSize 1266; even a non‑seekable pipe → 9237 **[EMP‑V]**. Downgrade this from \"mandatory converter check\" to \"assert cheaply\" |\n| Sample rate | 20‑bit STREAMINFO value passed through unvalidated; per‑frame codes from {8000,16000,22050,24000,32000,44100,48000,96000} | SOFT | `src/SMS_FLAC.c:309-312`. Use 44100/48000 |\n| ffmpeg encoder | never hard‑fails on any offered combination (0/84 cells) | — | **[EMP]** |\n\n### 1.7 `.ogg` — `src/SMS_ContainerOGG.c`\n\n| Cell | Value | Kind |\n|---|---|---|\n| Channels | **≤ 2** — a 5.1 `.ogg` is an **unplayable file, not a downmix** | **HARD** — `:70 retVal = lInfo.channels < 3 ? 1 : -1;` and `:96` requires `> 0` |\n| Sample rate | libvorbis accepts 8000–192000; SMS passes through | SOFT |\n| Bitrate | **libvorbis has a per‑channel *window*, floor and ceiling** — see §1.9 | ffmpeg‑side HARD |\n\n### 1.8 `.ac3` — `src/SMS_ContainerAC3.c`\n\nAudio‑only, single stream. `SMS_AC3Probe` requires **8 consecutive valid AC‑3 frames starting at offset 0** (`:19-45`) — no ID3 tag, no leading junk. bsid ≤ 11 (no E‑AC‑3), frmsizecod < 38 (≤ 640 kbps), 32000/44100/48000 only. Decoded to ≤ 2 channels unless SPDIF is on (§3).\n\n**Note: the converter has no `.ac3` preset.** The `SMS_CONTAINER_CODECS[\".ac3\"]` entry is currently dead.\n\n### 1.9 ffmpeg‑side encoder ceilings (these bind *before* SMS does)\n\nMeasured across a 2160‑cell reachable matrix and a 504‑cell bare‑encoder sweep **[EMP]**:\n\n| Encoder | Legal sample rates | Max channels | Bitrate rule |\n|---|---|---|---|\n| `libmp3lame` | 8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000 | 2 | **Silently clamps to 160 kbps below 32 kHz** while ffmpeg prints the requested value and exits 0 |\n| `mp2` | 16000, 22050, 24000, 32000, 44100, 48000 | 2 | **Hard‑fails above 160 kbps below 32 kHz**; ≤ 384 at/above 32 kHz |\n| `ac3` | **32000, 44100, 48000 ONLY** | 6 | 32–640 kbps, no practical ceiling in range |\n| `aac` | 8000–96000 | 8 | **Silently clamps to exactly `6 × rate × channels` bit/s** |\n| `libvorbis` | 8000–192000 | 6 | **Per‑channel window with a floor as well as a ceiling** (mono 48 kHz: 32–240k; stereo 48 kHz: 48–496k; mono 22.05 kHz: 16–88k). Mono fails where stereo succeeds |\n| `flac` | all tested 8000–192000 | 8 | `-b:a` ignored |\n\n**The mechanism:** ffmpeg treats an explicit `-ar`/`-ac` as a **hard constraint** and aborts if the encoder cannot meet it, but silently auto‑negotiates when the flag is absent. Proven side by side: `-i s8000.wav -c:a ac3` (no `-ar`) succeeded and resampled to 32000 Hz; `-i s8000.wav -c:a ac3 -ar 8000` failed with *\"Specified sample rate 8000 is not supported by the ac3 encoder\"*. **[EMP]**\n\n---\n\n## 2. CORRECTIONS TO THE CONVERTER'S CURRENT BELIEFS\n\n| Constant / belief | Verdict | Detail |\n|---|---|---|\n| `MAX_WIDTH = 1024` | ✅ **RIGHT** | `src/SMS_Player.c:1676-1681` **[SRC‑V]**. The `config.py:20-21` comment lacks a citation — add one. Note the *practical* ceiling is much lower: `WARNING_RESOLUTION_MSG` claims anything ≤ 1024 \"satisfies SMS format rules\", which is true of the format but ignores the VRAM texture budget (§0) — 1024×768 and 1024×1024 will collide with the display framebuffer |\n| `MAX_HEIGHT = 1024` | ✅ **RIGHT** | same |\n| `MAX_VIDEO_PACKET_SIZE = 2_097_088` | ✅ **RIGHT** | Exact. 2 MiB ring minus the 64‑byte `SMS_AVPacket` header, and 2,097,152 is already a multiple of 64 so no extra rounding loss. **Caveat:** a packet *at* the ceiling monopolises the whole ring and stalls the pipeline before it succeeds — treat anything above ~256 KB as a warning, not just >2,097,088 |\n| `MAX_AUDIO_PACKET_SIZE = 524_224` | ✅ **RIGHT** | Same derivation. Target ≤ 32 KB in practice |\n| `MAX_SMS_STREAMS = 8` | ⚠️ **RIGHT GLOBALLY, WRONG FOR `.mpg`** | `SMS_MAX_STREAMS` is 8, but the MPEG‑PS demuxer's array write is unguarded and its NULL‑terminator scans need `m_pStm[7] == NULL`. **For `.mpg`/`.mpeg` the cap is 7.** Also the failure modes differ per container and none of them is graceful: AVI **rejects the file** at >8, ASF **rejects**, MOV **silently drops** trak 9+, MPEG‑PS **writes out of bounds**. **[SRC‑V]** |\n| `SIZE_4_0_GIB` ≈ 3.99 GiB ceiling | ❌ **WRONG — TOO PERMISSIVE by 2×** | The real ceiling is **2,147,483,647 bytes**. `STIO_Seek(FileContext*, unsigned int aPos)` passes `aPos` into `fioLseek`/`fileXioLseek`, whose offset parameter is a **signed int**, and there is no `*Lseek64` call anywhere in `src/`. The in‑tree `udpfs` 32‑bit slot literally does `return (int)udpfs_lseek64(f, (s64)offset, whence)` — it sign‑extends. The 4 GiB figure in two of the audits is the *representation* limit of `unsigned int m_Size`, not the *seek* limit, and seeking happens during ordinary playback (AVI skips chunks by seek, MOV seeks backwards between samples). Change the FAIL threshold to 2 GiB and the warning threshold to ~1.9 GiB. **[SRC‑V]** — the underlying BDM/USB/SMB driver behaviour for a negative offset is **[UNCONF]** (those drivers are prebuilt `.irx` with no source here), but there is no scenario in which relying on >2 GiB is defensible |\n| `SMS_CONTAINER_CODECS[\".avi\"][\"audio\"]` omits `aac` | ❌ **WRONG** | `src/SMS_Codec.c:67 { SMS_CodecID_AAC, 0x000000FF }` is a 16‑bit‑reachable tag **[SRC‑V]**, and ffmpeg's AVI muxer emits exactly it: measured `codec_tag 0x00ff`, `strf` = 24 bytes, `nAvgBytesPerSec` = 16000, `cbSize` = 5, extradata `11 90 56 e5 00` (a valid AudioSpecificConfig) — which `SMSContainer_GetWAVHeader` reads into `m_pUserData` and `SMS_AAC.c:8262` feeds to `NeAACDecInit2`. **[EMP‑V]**. The `config.py:258-259` comment (\"no AAC in the audio one\") is factually wrong. Whether it decodes correctly on a real console is **[UNCONF]** — enable it behind a \"verified on hardware\" flag, but fix the comment now |\n| `SMS_CONTAINER_CODECS[\".avi\"][\"audio\"]` omits `flac` | ✅ **RIGHT** | `0x43614C66` is a 32‑bit tag and the read is 16‑bit. Unreachable. The file plays completely silent with no diagnostic |\n| `SMS_CONTAINER_CODECS[\".avi\"][\"audio\"]` **includes `vorbis`** | ❌ **WRONG — remove it** | ffmpeg writes `wFormatTag 0x566F`; SMS only knows `0x674F`; the `toupper` compare fails on byte 1. Silent file, validator‑clean PASS. **[SRC‑V + EMP]** |\n| `SMS_CONTAINER_CODECS[\".mp4\"/\".m4a\"][\"audio\"]` **includes `ac3`** | ❌ **WRONG — remove it** | ffmpeg writes an `ac-3`+`dac3` sample entry with **no esds**; SMS's MOV reader identifies codecs only through `_mov_read_esds` and has no `ac-3`/`dac3` atom handler. **[EMP + SRC]** |\n| `SMS_CONTAINER_CODECS[\".mp4\"][\"video\"]` **includes `mpeg1video`, `mpeg2video`** | ❌ **WRONG — remove them** | Tagged `mp4v` by ffmpeg, so SMS resolves them to `SMS_CodecID_MPEG4` and feeds an MPEG‑1/2 elementary stream to the MPEG‑4 Part 2 decoder. The esds objtype that would correct this is never read for video sample entries. **[SRC‑V + EMP]** |\n| `SMS_CONTAINER_CODECS[\".mp4\"/\".m4a\"][\"audio\"]` includes `vorbis` | ⚠️ **REMOVE (precaution)** | objtype 0xDD does resolve to `SMS_CodecID_OGGV`, but the Vorbis decoder's ability to init from an MP4 DecoderSpecificInfo is **[UNCONF]** |\n| `SMS_CONTAINER_CODECS[\".mp4\"/\".m4a\"][\"audio\"]` includes `mp3` | ✅ **RIGHT** | Verified end to end: tag `mp4a`, esds objtype `0x6B` = 107 → `{SMS_CodecID_MP3, 107}` in `s_MP4ObjType`, avgBitrate non‑zero, ES_Descr flags 0. **[EMP‑V + SRC‑V]** |\n| `MPEG_ALLOWED_FPS_MAP` (8 entries) | ✅ **RIGHT but incomplete** | Exactly the 8 MPEG `frame_rate_code` values. Missing: **code 1 (23.976) is mis‑tabulated in SMS** as 22.977 fps (`src/libmpeg.c:44`), so 23.976 content plays ~4.3 % slow. Demote it in the UI |\n| `-vtag XVID` on the recommended AVI preset | ⚠️ **CONDITIONALLY RIGHT** | Safe with `libxvid` (emits `XviD0074` → build 74 → no bug flags). **Unsafe with ffmpeg's native `mpeg4`** (emits `Lavc…` → `m_XviDBuild = -1` → `SMS_BUG_EDGE`+`QPEL_CHROMA`+`DC_CLIP`). `resolve_video_encoder` falls back to native `mpeg4` when libxvid is missing but the tag is applied unconditionally at `ffmpeg_utils.py:320-322`. **Fix: tie the tag to the resolved encoder — `XVID` for libxvid, `FMP4` otherwise.** **[SRC‑V + EMP‑V]** |\n| MP4 presets rely on ffmpeg's default (global‑header) MPEG‑4 | ❌ **WRONG** | Produces zero in‑band VOL. Add `-bsf:v dump_extra=freq=keyframe` to every MPEG‑4‑in‑MP4 command, or drop the MP4 video presets in favour of AVI. **[EMP‑V]** |\n| Validator FAIL thresholds generally | ❌ **ALL UNREACHABLE from the UI** | Across ~40 deliberately abusive configurations the validator returned PASS or WARN every time and never deleted a file: resolution FAIL needs >1024 but the spin box caps *at* 1024; the MPEG‑1/2 fps whitelist is byte‑identical to the combo's own list; stream‑count FAIL is impossible because `limit_streams` is hardcoded True; the packet ceilings have 25× and 900× margin at the worst reachable settings. **[EMP]** |\n\n---\n\n## 3. AUDIO PASSTHROUGH VERDICT\n\n### Can SMS play 5.1 AC‑3 as‑is?\n\n**No — with exactly one exception, and it is not under the converter's control.**\n\n**Default path (SPDIF off).** The player forces the request to stereo *before the decoder runs*: `src/SMS_Player.c:104-106` — `if (!(m_PlayerFlags & SMS_PF_SPDIF) || (m_PlayerFlags & SMS_PF_PDW22)) { if (m_Channels > 2) m_Channels = 2; }`. The AC‑3 decoder then parses **all five full‑bandwidth channels** and inverse‑transforms them, and `_ac3_downmix` (`src/SMS_AC3.c:2446-2449`, kernel `_ac3_mix32toS` at `:1689`) folds them to two using the **Dolby Surround matrix** (Lt/Rt), because the caller passes `AC3_DOLBY` (`:2589-2590`). The **LFE channel is decoded into scratch at `s_Samples + 1280` and never read again** (`:2403`) — `AC3_LFE` (16) can never be set because the only flags passed are `AC3_MONO` (1) and `AC3_DOLBY` (10), and `1 & 16 == 0`, `10 & 16 == 0` (`:581`). No level compensation.\n\nSo: **a 5.1 AC‑3 file plays. It plays as matrixed stereo with the .1 silently discarded.**\n\n**SPDIF path.** `src/SMS_AC3.c:347-349` swaps the decode function to `_ac3_spdif`, which does **not decode at all** — it byte‑swaps the frame into a fixed 6144‑byte IEC‑61937 burst (`:2620-2631`, preamble `0xF872`) and hands it to the IOP. `SMS_Player.c:108-109` sets `m_Channels = 5` as a marker (the function ignores its channel argument). This is the **only** way original 5.1 AC‑3 leaves the console intact, and it requires the *user* to have `SMS_PF_SPDIF` enabled and `SMS_PF_PDW22` disabled in SMS's own settings.\n\n**DTS** behaves identically: same 2‑entry converter table (`src/SMS_DTS.c:11556-11565`), same downmix, its own `_dts_spdif` path (`:11639`).\n\n### What \"keep original audio (stream copy)\" can and cannot promise\n\n**Can promise:**\n- Bit‑exact preservation of the elementary stream, which is what SPDIF users need.\n- No generational loss for the stereo fold‑down (SMS decodes the full 5.1 and matrixes it, rather than playing a pre‑downmixed 2.0).\n- Faster conversion and no audio re‑encode failures.\n\n**Cannot promise:**\n- **Discrete multichannel output.** Never claim \"keeps your 5.1\". The honest label is *\"keeps the original 5.1 stream — SMS plays it as Dolby‑Surround stereo unless your console's SPDIF passthrough is on.\"*\n- **The LFE.** It is discarded with no compensation. If bass matters, pre‑fold LFE into L/R during a transcode instead.\n- **Anything for E‑AC‑3.** `src/SMS_AC3.c:390` rejects `bsid ≥ 12` at sync — the file behaves as if it has no audio.\n\n### Which container/codec pairs stream‑copy is legal to attempt\n\n| Target | Copyable audio | Notes |\n|---|---|---|\n| `.avi` | AC‑3 (`0x2000`), DTS (`0x2001`), MP2 (`0x0050`), MP3 (`0x0055`), PCM S16**LE** (`0x0001`), AAC (`0x00FF`, unverified on HW) | Must write a non‑zero `nAvgBytesPerSec` and non‑zero `strh dwRate`. **Never** Vorbis or FLAC |\n| `.mpg`/`.mpeg` | AC‑3 → substream `0x80`; DTS → `0x88`; MP1/2/3 → `0x1C0`; LPCM S16**BE** → `0xA0` (7‑byte DVD header) | AC‑3 must be bsid ≤ 11 and ≤ 640 kbps. Exactly one audio stream, and a video stream is **mandatory** |\n| `.mp4`/`.m4a` | AAC only (`mp4a` + objtype `0x40`); MP3 (`mp4a` + objtype `0x6B`) also works | **AC‑3 copy is illegal** — ffmpeg emits `ac-3`/`dac3` with no esds |\n| `.ac3` | AC‑3 raw | Must start with 8 consecutive valid frames at offset 0 — **strip ID3 tags** |\n| `.mp3`, `.flac`, `.ogg` | matching codec only | FLAC/Vorbis ≤ 2 channels or the file will not open at all |\n\n**Hard prohibitions for any passthrough option:**\n- **Mono DTS.** `_dts_float_to_int_1` is a stub — `jr $ra; nop` (`src/SMS_DTS.c:11511-11513`) — yet `:11585-11593` still allocates the buffer and declares the length. The result is **uninitialised ring memory presented as PCM**. Transcode it.\n- **DTS with more than 5 primary channels.** `prim_channels = bitstream_get(3) + 1` ∈ [1,8] with no clamp (`:10756`), against `DTS_PRIM_CHANNELS_MAX = 5` arrays. 5.1 core is safe; 6.1/7.1 corrupts the decoder state struct.\n- **Any AC‑3/DTS track whose container header declares 0 channels.** `SMS_Player.c:105` clamps only `> 2`, so 0 survives, and `SMS_AC3.c:2725-2727` then installs the real stream count (up to 6) which indexes a 2‑entry function‑pointer table. Always write a non‑zero `nChannels`.\n- **>2‑channel LPCM.** The one codec with no clamp anywhere in the EE (`SMS_ContainerMPEG_PS.c:423` forwards 1..8 straight to `SPU_InitContext`).\n- **>2‑channel Vorbis or FLAC.** Container probe rejects the file outright.\n\n---\n\n## 4. CROSS‑CONVERSION RECIPES\n\n### 4.1 Video source → audio target — **already works, no change needed**\n\nThe current pattern is correct in ffmpeg 8.1.1: `-vn` **does** override an explicit `-map 0:v:0?`. Proof is ffmpeg's own mapping block for an input with both streams — only one line, `Stream #0:1 -> #0:0 (aac -> mp3)`; the video candidate is filtered out before the muxer sees it, with no warning and nothing dangling. **[EMP]**\n\n```\nffmpeg -y -i IN -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -vn \\\n       -c:a libmp3lame -b:a 192k -ar 48000 -ac 2 -f mp3 OUT.mp3\n```\n\nVerified clean for all four audio targets (`.mp3`, `.m4a`/aac, `.flac`, `.ogg`), and the existing pre‑flight guard for a **silent** video source (`worker.py:162`, *\"This file has no audio track…\"*) correctly refuses before ffmpeg launches. **[EMP]**\n\n### 4.2 Audio source → video target — **currently broken, two working fixes**\n\nToday, when `has_real_video_stream()` is False, `ffmpeg_utils.py:302` skips the whole video block and `:382-383` appends `-vn`, so an MP3 + \"Xvid‑Compatible AVI\" produces a **91,356‑byte AVI containing only an MP3 track, zero video streams**, validator PASS, promoted to `*_SMS.avi`, reported as success. The queue also over‑estimates size by 11.3× because `main_window.py:415` gates the video bitrate on the *preset* rather than the *source*. **[EMP]**\n\nBoth replacements below were run and validated end to end.\n\n**Recipe A — synthesised black video (works for any audio source):**\n\n```\nffmpeg -y -f lavfi -i \"color=c=black:s=640x480:r=30\" -i IN.mp3 \\\n       -map 0:v:0 -map 1:a:0 -sn -dn -map_metadata -1 \\\n       -c:v libxvid -vtag XVID -b:v 1500k -bf 0 -g 50 -vf format=yuv420p \\\n       -c:a libmp3lame -b:a 128k -ar 48000 -ac 2 \\\n       -shortest -f avi OUT.avi\n```\nResult: exit 0, 134,090 B, frame=150, `mpeg4/XVID 640x480 151 frames` + `mp3 210 frames`, clean re‑decode, validator PASS. **`-shortest` is mandatory** — the lavfi source is infinite. **[EMP]**\n\n**Recipe B — use the embedded cover art as the video track:**\n\n```\nffmpeg -y -i IN.mp3 \\\n  -filter_complex \"[0:v]scale=640:480:force_original_aspect_ratio=decrease,\\\npad=640:480:(ow-iw)/2:(oh-ih)/2:black,loop=loop=-1:size=1:start=0,\\\nsetpts=N/30/TB,format=yuv420p[v]\" \\\n  -map \"[v]\" -map 0:a:0 -sn -dn -map_metadata -1 \\\n  -c:v libxvid -vtag XVID -r 30 -b:v 1500k -bf 0 -g 50 \\\n  -c:a libmp3lame -b:a 128k -ar 48000 -ac 2 -shortest -f avi OUT.avi\n```\nResult: exit 0, frame=150, validator PASS, 111× realtime. **Three non‑obvious requirements:** (1) it must be `-filter_complex` with an explicit `-map \"[v]\"` — a plain `-vf` on an `attached_pic` stream emits *\"No filtered frames for output stream\"* and writes zero video packets; (2) `setpts=N/30/TB` after `loop` is required — without it the attached_pic's 90k tbr makes the `fps` filter run at **0.117× realtime** (42.7 s of CPU for 5 s of audio), ~370× slower for identical output; (3) `-shortest`. **[EMP]**\n\n**Recipe C — the MP4 equivalent** (no `-vtag`, which the muxer rejects):\n\n```\nffmpeg -y -f lavfi -i \"color=c=black:s=640x480:r=30\" -i IN.mp3 \\\n       -map 0:v:0 -map 1:a:0 -sn -dn -map_metadata -1 \\\n       -c:v mpeg4 -b:v 1500k -bf 0 -g 50 -vf format=yuv420p \\\n       -bsf:v dump_extra=freq=keyframe \\\n       -c:a aac -b:a 128k -ar 48000 -ac 2 \\\n       -shortest -f mp4 -movflags +faststart OUT.mp4\n```\nThe base form (without `dump_extra`) was measured working: exit 0, `mp4v 640x480 150 frames` + `mp4a`, validator PASS. **[EMP]** The `dump_extra` bsf is my addition and was independently verified to put VOS+VOL in‑band in MP4 while keeping the `mp4v` tag. **[EMP‑V]**\n\n**Do not** \"fix\" this by relaxing `has_real_video_stream()` (`ffmpeg_utils.py:131-159`). The control runs reproduce exactly the failures its docstring describes: feeding cover art as video gives MP4 exit −22 with *\"Could not find tag for codec mpeg4 in stream #0\"* and zero bytes, or an AVI advertising a 5.04 s 320×240 stream that decodes to **frame=0**. The guard is load‑bearing; the fix belongs above it.\n\n### 4.3 Recommended canonical commands\n\n**AVI / MPEG‑4 (the recommended target):**\n```\n-c:v libxvid -vtag XVID          # ONLY if libxvid resolved; else -c:v mpeg4 -vtag FMP4\n-b:v 1500k -bf 0 -g 50 -flags -gmc\n-vf scale=…:…:force_original_aspect_ratio=decrease,pad=…:black,fps=…,format=yuv420p\n-c:a libmp3lame -b:a 128k -ar 48000 -ac 2\n-f avi\n# post-mux: if byte 4 or byte 5 of the file == 0x30, pad and rewrite dwRIFFSize\n```\nAVI gets per‑keyframe VOS+VOL for free (measured: 5 VOS / 5 VOL for a 5‑GOP clip).\n\n**MPEG‑PS:**\n```\n-c:v mpeg2video -b:v 1500k -g 12 -pix_fmt yuv420p -r 25   (or 29.97 / 30 — avoid 23.976)\n-c:a mp2 -b:a 192k -ar 48000 -ac 2                        (or -c:a ac3, ≤640k, 32/44.1/48k)\n-f vob      # or -f mpeg for MPEG-1; both emit no PSM, video on 0xE0, AC-3 on substream 0x80\n```\nVerified structure: pack/system headers present, **PSM absent**, seq header + GOP header at every GOP.\n\n**MP4 (only if the user insists):** add `-bsf:v dump_extra=freq=keyframe`, `-bf 0`, `-movflags +faststart`, and audio = `aac` or `libmp3lame` only.\n\n**Never emit:** `-qpel 1` (not an ffmpeg option at all — it is a value of `-flags`; the correct spelling is `-flags +qpel`, and today it kills every job at ffmpeg's argument parser with *\"Unrecognized option 'qpel'\"*, exit −1, no output). `-bf > 0` with `msmpeg4v3`. B‑pyramid, in any form. Interlaced MPEG‑4. GMC with more than 3 warp points. DivX packed bitstream.\n\n---\n\n## 5. VALIDATION GAPS — RANKED\n\nLegend: **CLAMP** = constrain the control so the bad value is unselectable · **WARN** = allow, produce, surface a message · **BLOCK** = refuse before launching ffmpeg.\n\n### Tier 1 — jobs that die with a raw ffmpeg error and no output\n\n| # | Gap | Action | Why |\n|---|---|---|---|\n| 1 | **`-qpel 1` is not an ffmpeg option.** Fails at the argument parser on **every** codec and container, exit −1, *\"Unrecognized option 'qpel'\"*, in single‑ and two‑pass. `ffmpeg_utils.py:370-371` | **Fix the emission** to `-flags +qpel` (SMS's QPel support is real and complete: `SMS_DSP_QPel.S`, tables at `src/SMS_DSP.c:414-491`), and **WARN** that QPel is expensive on a 294 MHz EE | 100 % of ticks fail |\n| 2 | **`msmpeg4v3` + B‑frames ≥ 1.** exit −22, *\"B-frames not supported by codec\"*, zero bytes | **CLAMP** the B‑frames spin to 0..0 when the codec is msmpeg4v3 | SMS's MSMPEG4v3 decoder also mishandles B‑pictures (`SMS_MSMPEG4.c:2126-2130` returns 1, and the caller only aborts on `< 0`), so this is doubly right |\n| 3 | **246 / 2160 reachable audio combinations hard‑fail** (11.4 %): ac3 @ 22050 (96 cells), mp2 @ 22050 above 160 kbps (18), libvorbis outside its per‑channel bitrate window (132) | **CLAMP** — repopulate bitrate and sample‑rate combos from the selected audio codec (and, for libvorbis and aac, from the channel count too). `advanced_settings.py:267-300` currently has zero cross‑control coupling; `_restrict_codec_choices` is the only cross‑filter and it keys on container extension alone | Eliminates all 36 dropdown‑reachable hard failures |\n| 4 | **`mpeg4` two‑pass can produce zero bytes** on high‑entropy material | keep the existing libxvid preference; **BLOCK** two‑pass with native `mpeg4` if libxvid is absent, or fall back to single‑pass with a WARN | already documented in `resolve_video_encoder`'s docstring |\n\n### Tier 2 — files that are produced, pass validation, and will not play on SMS\n\n| # | Gap | Action | Why |\n|---|---|---|---|\n| 5 | **Vorbis in AVI** — ffmpeg writes `0x566F`, SMS knows only `0x674F` → silent file | **BLOCK**: remove `vorbis` from `SMS_CONTAINER_CODECS[\".avi\"][\"audio\"]` | `[SRC‑V + EMP]` |\n| 6 | **AC‑3 in MP4/M4A** — `ac-3`+`dac3`, no esds → no codec id | **BLOCK**: remove `ac3` from `.mp4`/`.m4a` | `[SRC + EMP]` |\n| 7 | **MPEG‑1/MPEG‑2 video in MP4** — tagged `mp4v`, decoded by the MPEG‑4 Part 2 decoder | **BLOCK**: remove `mpeg1video`/`mpeg2video` from `.mp4` video | `[SRC‑V + EMP]` |\n| 8 | **MPEG‑4 in MP4 ships with no in‑band VOL** | **Fix the command** (`-bsf:v dump_extra=freq=keyframe`) and **assert** post‑mux that the first keyframe packet begins with `00 00 01 B0` | `[SRC‑V + EMP‑V]` |\n| 9 | **`-vtag XVID` applied to native `mpeg4`** turns on three bug‑compat modes including out‑of‑bounds MV clamping | **Fix**: tie the tag to the *resolved* encoder — `XVID` for libxvid, `FMP4` otherwise. **Assert** at build time | `[SRC‑V + EMP‑V]` |\n| 10 | **No check that a video‑target container actually contains a video stream** | **WARN, not FAIL** (a FAIL deletes the output at `worker.py:235`, and the audio is salvageable) — and add the mirror of `worker.py:162` for the audio‑source → video‑target direction, offering Recipe A/B | `validator.py:117-278` has no such check |\n| 11 | **File‑size ceiling is 2× too permissive** (3.99 GiB vs a real 2 GiB) | **BLOCK** at 2,147,483,647; **WARN** from ~1.9 GiB; make `worker.py:309`'s in‑flight abort use the new value | `[SRC‑V]` |\n| 12 | **`.mpg` stream cap is 7, not 8** | **CLAMP** per container | `[SRC‑V]` |\n| 13 | **`.ac3` output must have no leading ID3/junk** and needs 8 valid frames at offset 0 | **WARN** if `.ac3` is ever wired up (no preset today) | `SMS_ContainerAC3.c:19-45` |\n| 14 | **AVI `idx1` offset‑bug false positive** — 0.78 % of file sizes break every seek | **auto‑fix** post‑mux: if byte 4 or 5 == 0x30, pad and rewrite `dwRIFFSize` | `SMS_ContainerAVI.c:377-387` |\n| 15 | **AVI without `idx1`** plays but cannot be scrubbed or resumed; and `idx1` must cover **every** stream | **WARN**; ffmpeg writes idx1 by default for non‑OpenDML AVI | `SMS_ContainerAVI.c:1123-1131` |\n\n### Tier 3 — silent substitutions (ffmpeg exits 0 and, in one case, lies)\n\n| # | Gap | Action |\n|---|---|---|\n| 16 | **`libmp3lame` above 160 kbps below 32 kHz writes 160 and prints the requested value.** ffprobe reads back 160000 for a job the UI said was 320 | **CLAMP** the bitrate list from the rate; if it must be allowed, **WARN** post‑hoc by comparing the requested vs. probed bitrate |\n| 17 | **`aac` clamps at exactly `6 × rate × channels`** — 9 dropdown‑reachable cells silently reduced, and mono halves the ceiling with no feedback | **CLAMP** from rate *and* channels |\n| 18 | **\"Keep source layout\" + 5.1 source + mp3/mp2 silently downmixes to stereo**, contradicting the label | **WARN**, and relabel |\n| 19 | **\"Keep source rate\" + 22.05 kHz source + mp2 @ 192k (the MPEG presets' own default) hard‑fails**; the same option with ac3 silently resamples 8000/22050/24000 → 32000 | **Pre‑flight check** the source rate against the selected encoder; **BLOCK** with a specific message rather than letting ffmpeg fail |\n| 20 | **FLAC preset + Normalize → 192 kHz and 13.4× the source size**, from a preset described as \"keeps every bit of the original\" | **BLOCK** loudnorm when `sample_rate is None`, or force `-ar` after the filter |\n| 21 | **`-gmc 1` is a no‑op on 4 of 5 codecs** (only libxvid has the AVOption; ffmpeg prints *\"Codec AVOption gmc … has not been used\"* and exits 0) | **Hide or disable** the checkbox unless the resolved encoder is libxvid — and note SMS has **no `case 4:` for perspective GMC** (`SMS_MPEG4.c:1196-1291`) and an unclamped 6‑bit warp‑point read that overruns a 4‑entry stack array (`:893`, `:1134`). Default OFF |\n| 22 | **MPEG‑1/2 at 23.976 fps plays ~4.3 % slow** because `s_FrameRate[1]` is `23.0*1000/1001` | **WARN** on frame‑rate code 1 for MPEG targets; prefer 25 / 29.97 / 30 |\n| 23 | **\"Fit inside dimensions\" pads**, producing output byte‑identical to \"Letterbox\" despite a tooltip promising no black bars | **Fix the filter** (`ffmpeg_utils.py:246-249`) or fix the tooltip |\n| 24 | **The orange over‑resolution warning quotes the spin‑box value, which the upscale clamp silently overrides** (`ffmpeg_utils.py:238-242` vs `advanced_settings.py:551`) | **Recompute** the warning from the effective output size |\n| 25 | **Interlaced MPEG‑4 is a NULL dereference**, not just wrong‑looking (`g_MPEGCtx.m_pPFieldMVTbl` is never allocated anywhere in the tree) | **BLOCK**: force progressive; deinterlace interlaced sources with `yadif`. There is no UI control for this today, but any future one must be gated |\n| 26 | **DXSB subtitles alongside MPEG‑4/MSMPEG4v3 corrupt the AC/DC prediction table** (`src/SMS_DXSB.c:274` writes over `0x70001B30` = `m_pMBIntraTbl`) | **BLOCK** if subtitle muxing is ever added |\n| 27 | **No test coverage** for any Tier‑1 failure: grep across `test_converter.py` / `test_end_to_end.py` for `qpel`/`gmc`/`bframes`/`msmpeg4` yields exactly one hit (`test_converter.py:237`, an unrelated `sms_supports` assertion) | Add regression tests that assert the *generated command*, not just the output file |\n\n---\n\n## 6. OPEN QUESTIONS — DO NOT PAPER OVER\n\n1. **AAC‑in‑AVI on real hardware.** Source and ffmpeg output line up perfectly (tag `0x00FF`, 24‑byte strf, 5‑byte AudioSpecificConfig, non‑zero `nAvgBytesPerSec`), but no console has played one. **[UNCONF]** — enable behind a flag.\n2. **SPU2 / AUDSRV ceiling.** AUDSRV is an 8989‑byte prebuilt blob (`include/SMS_Data.h:26`, loaded at `src/SMS_IOP.c:179`) with no source in this tree. Nothing resamples on the EE (`SMS_Player.c:188-191`), so the max sample rate the hardware accepts, and what it does with the `anChannels = 5` SPDIF marker, are **[UNCONF]**. Use 48000/44100 and stereo.\n3. **Sustained read bandwidth.** No MB/s figure exists in the source. The only hard numbers are `MX4SIO_RD_CHUNK = 4096` and `UDPFS_RD_CHUNK = 65536` (`src/SMS_FileContext.c:22,32`) plus a synchronous single‑buffered fill path on mass/smb/mmce. The 4 KiB figure's \"empirically confirmed reliable ceiling\" claim lives in `MX4SIO_SOLUTION.md:183` — documentation, not code. All bitrate guidance in §1 is therefore **SOFT and unverified**.\n4. **EE heap size.** No linker script or `_heap_size` symbol exists in this tree. Per‑resolution decoder allocations are exact (§0), but the absolute \"runs out of memory at X\" point is **[UNCONF]**.\n5. **What the IPU BDEC does with non‑4:2:0 MPEG‑2.** There is no chroma_format check (`libmpeg.c:439` writes `m_ChromaFmt` and nothing reads it). Garbage vs. hang in `_MPEG_WaitBDEC` is **[UNCONF]**. Emit 4:2:0 and the question never arises.\n6. **MIPS divide‑by‑zero semantics** for `SMS_Container.c:193`, `SMS_EE.c:105` and `SMS_MSMPEG4.c:2322` — trap vs. garbage quotient is **[UNCONF]**. The arithmetic reaching zero is confirmed in all three; all three are converter‑actionable regardless.\n7. **DTS / LPCM / WMA in AVI and MPEG‑PS** — the tag mappings check out on paper but none is exercised by the converter's UI and none was measured. **[UNCONF]**.\n8. **BDM/USB/SMB driver behaviour above 2 GiB.** Those `.irx` files are prebuilt. Only the in‑tree `udpfs` was traceable, and it sign‑extends. **[UNCONF]** for the rest — which is itself the argument for the 2 GiB cap.",
    "sourceFindings": [
      {
        "area": "AVI container path (src/SMS_ContainerAVI.c + src/SMS_Codec.c + the SMS_Container.c / SMS_Player.c code they hand off to)",
        "findings": [
          {
            "claim": "s_CodecVideoTags accepts EXACTLY 22 entries (21 real + NULL terminator), all mapping to only 3 codec IDs: MPEG4 (11 tags), MSMPEG4V3 (9 tags), DXSB (1 tag). Full set -> SMS_CodecID_MPEG4: 'DIVX','DX50','XVID','MP4S','M4S2',0x00000004,'DIV1','BLZ0','mp4v','UMP4','FMP4. -> SMS_CodecID_MSMPEG4V3: 'DIV3','MP43','MPG3','DIV5','DIV6','DIV4','AP41','COL1','COL0'. -> SMS_CodecID_DXSB: 'DXSB'.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:32-55 `static SMS_CodecTag s_CodecVideoTags[] = { { SMS_CodecID_MPEG4, SMS_MKTAG( 'D','I','V','X' ) }, ... { SMS_CodecID_MPEG4, SMS_MKTAG( '\\4','\\0','\\0','\\0' ) }, ... { SMS_CodecID_MSMPEG4V3, SMS_MKTAG( 'C','O','L','0' ) }, { SMS_CodecID_DXSB, SMS_MKTAG( 'D','X','S','B' ) }, { SMS_CodecID_NULL, SMS_MKTAG( '\\0','\\0','\\0','\\0' ) } };`  Tag is matched against BITMAPINFOHEADER.biCompression read at src/SMS_ContainerAVI.c:595 `lSubTag = File_GetUInt ( lpFileCtx );` and resolved at :611 `lpCodecCtx -> m_ID = SMS_CodecGetID ( SMS_CodecTypeVideo, lSubTag );`. SMS_MKTAG is little-endian: include/SMS_FourCC.h:27 `#define SMS_MKTAG( a, b, c, d ) ( ( a ) | ( ( b ) << 8 ) | ( ( c ) << 16 ) | ( ( d ) << 24 ) )`",
            "converter_impact": "Emit video biCompression from this list only. Safest universal choice: 'XVID' or 'DIVX' (-> MPEG4 decoder). Note 0x00000004 (a bare numeric 4, not a printable FourCC) is also accepted as MPEG-4. Anything else -> no decoder (see next findings)."
          },
          {
            "claim": "MPEG-1 and MPEG-2 video have NO entry in s_CodecVideoTags. An AVI carrying 'mpg1'/'mpg2'/'MPEG'/'mpeg'/0x10000001/0x10000002 resolves to SMS_CodecID_NULL even though a working MPEG12 decoder exists in the binary.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:32-55 — the whole table contains only SMS_CodecID_MPEG4, SMS_CodecID_MSMPEG4V3, SMS_CodecID_DXSB, SMS_CodecID_NULL; the strings MPEG1/MPEG2 do not appear. The decoder itself is wired at src/SMS_Codec.c:98-99 `case SMS_CodecID_MPEG1 : case SMS_CodecID_MPEG2 : SMS_Codec_MPEG12_Open ( apCtx ); break;` but nothing in the AVI path can ever produce those IDs — the only AVI assignment is src/SMS_ContainerAVI.c:611 `lpCodecCtx -> m_ID = SMS_CodecGetID ( SMS_CodecTypeVideo, lSubTag );`",
            "converter_impact": "FORBID MPEG-1/MPEG-2 video inside AVI. The IPU-accelerated MPEG12 decoder is reachable only through the MPEG-PS container (src/SMS_ContainerMPEG_PS.c) and ASF. If you want MPEG-2, mux to MPEG-PS, not AVI."
          },
          {
            "claim": "An unmatched video FourCC does not produce an error — SMS_CodecOpen leaves m_pCodec NULL, returns 0, and the player silently reclassifies the video stream as a subtitle-candidate. If that was the only video stream the file falls back to audio-only playback, or is rejected outright with STR_UNSUPPORTED_FILE when there is also no usable audio.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:117-121 `default: break; } /* end switch */ return apCtx -> m_pCodec != NULL;`  src/SMS_Player.c:1670-1703 `if ( SMS_CodecOpen ( lpStm -> m_pCodec ) ) { ... } else goto addSub;` ... `addSub: if ( lSubIdx < MAX_SUB ) lSubStm[ lSubIdx++ ] = i;`  src/SMS_Player.c:1760-1774 `} else if ( s_Player.m_AudioIdx >= 0 ) { lpAR = _sms_audio_only_renderer; ... } else { destroy: s_Player.m_pCont -> Destroy ( s_Player.m_pCont, 1 ); ... goto error; }`",
            "converter_impact": "Do not rely on any error message to tell the user the codec was wrong — the symptom is 'plays sound, black screen' or a generic unsupported-file box. The converter must guarantee the FourCC before shipping the file."
          },
          {
            "claim": "s_CodecAudioTags contains EXACTLY 12 entries (11 real + NULL terminator): MP2=0x0050, MP3=0x0055, AC3=0x2000, DTS=0x2001, OGGV(Vorbis)=0x674F, WMA_V1=0x0160, WMA_V2=0x0161, PCM16LE=0x0001, AAC=0x6134706D, AAC=0x000000FF, FLAC=0x43614C66.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:57-70 `static SMS_CodecTag s_CodecAudioTags[] = { { SMS_CodecID_MP2, 0x00000050 }, { SMS_CodecID_MP3, 0x00000055 }, { SMS_CodecID_AC3, 0x00002000 }, { SMS_CodecID_DTS, 0x00002001 }, { SMS_CodecID_OGGV, 0x0000674F }, { SMS_CodecID_WMA_V1, 0x00000160 }, { SMS_CodecID_WMA_V2, 0x00000161 }, { SMS_CodecID_PCM16LE, 0x00000001 }, { SMS_CodecID_AAC, 0x6134706D }, { SMS_CodecID_AAC, 0x000000FF }, { SMS_CodecID_FLAC, 0x43614C66 }, { SMS_CodecID_NULL, 0x00000000 } };`",
            "converter_impact": "This is the complete audio universe for every WAVEFORMATEX-based container in SMS (AVI and ASF). Note PCM is 16-bit LITTLE-endian only — SMS_CodecID_PCM16BE exists in the enum but has no tag entry."
          },
          {
            "claim": "SETTLED: AAC *is* present and *is* reachable in AVI, via wFormatTag 0x00FF. The downstream tool that believes AAC is absent is WRONG for 0x00FF.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:67 `{ SMS_CodecID_AAC, 0x000000FF },`  Reached from AVI at src/SMS_ContainerAVI.c:617 `SMSContainer_GetWAVHeader ( lpFileCtx, lpCodecCtx, lSize );` -> src/SMS_Container.c:290,299 `int lID = File_GetUShort ( apFileCtx ); ... apCodecCtx -> m_ID = SMS_CodecGetID ( SMS_CodecTypeAudio, lID );`. Both comparison operands for byte 0 are the identical literal 0xFF, and bytes 1-3 are 0x00 on both sides, so the toupper() comparison at src/SMS_Codec.c:78-81 matches exactly. Decoder is real: src/SMS_Codec.c:114 `case SMS_CodecID_AAC : apCtx -> m_Tag = SMS_MKTAG( 'a','a','c',' ' ); SMS_Codec_AAC_Open ( apCtx ); break;` and src/SMS_AAC.c:8281-8287 sets m_pCodec/Init/Decode/Destroy.",
            "arithmetic": "lID = File_GetUShort() in [0,0xFFFF]. Table entry 0x000000FF has byte0=0xFF, byte1=byte2=byte3=0x00. lID=0x00FF gives byte0=0xFF, byte1..3=0x00. All four byte comparisons are identity comparisons -> match.",
            "converter_impact": "AAC in AVI: set wFormatTag = 0x00FF. Allow it."
          },
          {
            "claim": "AAC tag 0x6134706D ('mp4a') is UNREACHABLE from AVI. The wFormatTag is read as a 16-bit value, so any table entry with a non-zero byte at bit positions 16-31 can never match.",
            "kind": "HARD",
            "evidence": "src/SMS_Container.c:290 `int lID = File_GetUShort ( apFileCtx );`  include/SMS_FileContext.h:96-99 `static INLINE unsigned int File_GetUShort ( FileContext* apCtx ) { unsigned int retVal = File_GetByte ( apCtx ); return retVal | File_GetByte ( apCtx ) << 8; }`  Comparison at src/SMS_Codec.c:80 `toupper ( ( aTag >> 16 ) & 0xFF ) == toupper ( ( lpTag -> m_Tag >> 16 ) & 0xFF )`",
            "arithmetic": "lID <= 0xFFFF => (lID >> 16) & 0xFF == 0 and (lID >> 24) & 0xFF == 0. Entry 0x6134706D has (tag>>16)&0xFF = 0x34 ('4') and (tag>>24)&0xFF = 0x61 ('a'). toupper(0) = 0 != toupper(0x34) = 0x34. No match, ever. (This entry exists for the MOV/MP4 demuxer, which passes a real 32-bit FourCC: src/SMS_ContainerMOV.c:286.)",
            "converter_impact": "Never write 'mp4a' as an AVI wFormatTag — it will silently fall through to SMS_CodecID_NULL. Use 0x00FF."
          },
          {
            "claim": "SETTLED: FLAC is UNREACHABLE from AVI. The only FLAC tag is the 32-bit 'fLaC' (0x43614C66), and the 16-bit wFormatTag read makes it unmatchable. No 16-bit FLAC tag (e.g. the de-facto 0xF1AC) exists in the table.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:68 `{ SMS_CodecID_FLAC, 0x43614C66 },` — that is the ONLY FLAC row. src/SMS_Container.c:290 `int lID = File_GetUShort ( apFileCtx );`",
            "arithmetic": "0x43614C66 bytes = {0x66 'f', 0x4C 'L', 0x61 'a', 0x43 'C'}. Bytes 2 and 3 are non-zero; a 16-bit lID always has bytes 2 and 3 == 0. toupper(0x61)=0x41 != toupper(0)=0. Never matches. So the downstream tool is CORRECT about FLAC-in-AVI and WRONG about AAC-in-AVI.",
            "converter_impact": "FORBID FLAC inside AVI. FLAC is only reachable via the native .flac container (src/SMS_ContainerFLAC.c:133 `lpCodecCtx -> m_ID = SMS_CodecID_FLAC;`) or MOV. An AVI FLAC track resolves to SMS_CodecID_NULL, SMS_CodecOpen returns 0, and src/SMS_Player.c:1707 `if ( SMS_CodecOpen ( lpStm -> m_pCodec ) )` skips the stream with no diagnostic — the file plays completely silent."
          },
          {
            "claim": "SMS_CodecGetID case-folds every byte with toupper(), so the audio table matches ALIAS wFormatTags that are not the intended codec. Confirmed aliases: 0x0070 -> MP2, 0x0075 -> MP3, 0x0141 -> WMA_V2, and 0x474F / 0x476F / 0x676F -> Vorbis. These are silent mis-detections, not rejections.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:78-81 `if ( toupper ( ( aTag >> 0 ) & 0xFF ) == toupper ( ( lpTag -> m_Tag >> 0 ) & 0xFF ) && toupper ( ( aTag >> 8 ) & 0xFF ) == toupper ( ( lpTag -> m_Tag >> 8 ) & 0xFF ) && ... ) return lpTag -> m_ID;`  toupper is the libc one (no project override — no ctype.h/toupper definition exists anywhere under include/, and SMS_Codec.c includes only SMS_*.h, <malloc.h>, <string.h>), so in the C locale it maps 0x61-0x7A -> 0x41-0x5A and leaves every other byte unchanged.",
            "arithmetic": "Two byte values b1,b2 collide iff {b1,b2} = {X, X+0x20} with X in ['A','Z']. Table byte 0x50 ('P') therefore also matches 0x70 ('p') => wFormatTag 0x0070 hits the MP2 row. 0x55 ('U') also matches 0x75 ('u') => 0x0075 hits MP3. 0x61 ('a') in 0x0161 also matches 0x41 ('A') => 0x0141 hits WMA_V2. In 0x674F, byte0 0x4F ('O') also matches 0x6F, and byte1 0x67 ('g') also matches 0x47 ('G') => 0x474F, 0x476F, 0x676F all hit Vorbis. Rows 0x2000, 0x2001, 0x0160, 0x0001, 0x00FF contain no ASCII letters and have no aliases.",
            "converter_impact": "Never emit wFormatTag 0x0070, 0x0075, 0x0141, 0x474F, 0x476F or 0x676F — they will be handed to the wrong decoder and produce noise or a decoder hang, not a clean rejection. Restrict output to the exact canonical values: 0x0001, 0x0050, 0x0055, 0x0160, 0x0161, 0x00FF, 0x2000, 0x2001, 0x674F."
          },
          {
            "claim": "SMS_CodecGetID IS the sole codec-selection authority for AVI audio; nothing overrides it. The AVI demuxer does not call SMS_CodecGetID for audio directly — it delegates to SMSContainer_GetWAVHeader, which is the only writer of m_ID on that path. (Contrast ASF, which DOES override.)",
            "kind": "HARD",
            "evidence": "Call chain: SMS_GetContainerAVI (src/SMS_ContainerAVI.c:1179) -> _ReadHeader -> case 'strf' -> src/SMS_ContainerAVI.c:615-623 `case SMS_CodecTypeAudio: { SMSContainer_GetWAVHeader ( lpFileCtx, lpCodecCtx, lSize ); if ( lSize % 2 ) File_Skip ( lpFileCtx, 1 ); lpStm = apCtx -> m_pStm[ lStmIdx ]; if ( lpCodecCtx -> m_BlockAlign == 1 && MYSTRM( lpStm ) -> m_SampleSize == 0 ) MYSTRM( lpStm ) -> m_SampleSize = 1; } break;` — the only field it touches afterwards is m_SampleSize. -> src/SMS_Container.c:299 `apCodecCtx -> m_ID = SMS_CodecGetID ( SMS_CodecTypeAudio, lID );`. A repo-wide grep for assignments to m_ID shows the AVI file only ever writes `lpCodecCtx -> m_ID = SMS_CodecID_NULL;` at src/SMS_ContainerAVI.c:630 (the 'txts'/unknown-stream-type default branch). By contrast src/SMS_ContainerASF.c:268 `lpStm -> m_pCodec -> m_ID = SMS_CodecID_NULL;` and :289 `if ( lpStm -> m_pCodec -> m_ID == SMS_CodecID_MP3 )` show ASF post-processing that AVI does not have. Consumer: src/SMS_Player.c:1707 `if ( SMS_CodecOpen ( lpStm -> m_pCodec ) )` -> src/SMS_Codec.c:94 `switch ( apCtx -> m_ID )`.",
            "converter_impact": "You can model AVI audio acceptance purely as: wFormatTag -> s_CodecAudioTags (with the toupper aliasing) -> SMS_CodecOpen switch. No hidden second chance, no sniffing of the bitstream, no extension-based override."
          },
          {
            "claim": "Every codec ID reachable from the AVI tables has a real decoder that sets m_pCodec — there are no dead tag entries for video. MPEG4 -> SMS_Codec_MPEG4_Open, MSMPEG4V3 -> SMS_Codec_MSMPEG4_Open, MP2/MP3 -> SMS_Codec_MP123_Open, AC3, DTS, OGGV, WMA_V1/V2, PCM16LE, AAC, FLAC all allocate an SMS_Codec with Init/Decode/Destroy. DXSB is the one special case: it has NO decoder but SMS_CodecOpen returns 1 for it anyway via an early return.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:96-115 (the full dispatch) and src/SMS_Codec.c:101 `case SMS_CodecID_DXSB: return 1;` — an early return that bypasses `return apCtx -> m_pCodec != NULL;` at :121. Decoder constructors confirmed present: src/SMS_MPEG4.c:477-486, src/SMS_MSMPEG4.c:1397-1406, src/SMS_MPEG12.c:78-97, src/SMS_AAC.c:8277-8293, src/SMS_FLAC.c:585-594 — each begins `apCtx -> m_pCodec = calloc ( 1, sizeof ( SMS_Codec ) );` and assigns Init/Decode/Destroy.",
            "converter_impact": "No tag in the table is a trap in the 'no decoder' sense. The traps are elsewhere: unreachable tags (mp4a/fLaC), aliased tags, and the DXSB ordering bug below."
          },
          {
            "claim": "DXSB (VobSub-in-AVI) subtitle streams are SILENTLY DROPPED if they appear before the real video stream in strl order, because SMS_CodecOpen returns 1 for DXSB and the player then does nothing with it while m_VideoIdx is still unset.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:1668-1703 `if ( s_Player.m_VideoIdx < 0 ) { if ( SMS_CodecOpen ( lpStm -> m_pCodec ) ) { if ( lpStm -> m_pCodec -> m_ID != SMS_CodecID_DXSB ) { ... s_Player.m_VideoIdx = i; ... } } else goto addSub; } else if ( lpStm -> m_pCodec -> m_ID == SMS_CodecID_DXSB ) addSub: if ( lSubIdx < MAX_SUB ) lSubStm[ lSubIdx++ ] = i;`  combined with src/SMS_Codec.c:101 `case SMS_CodecID_DXSB: return 1;`. m_VideoIdx starts negative: src/SMS_Player.c:1657 `s_Player.m_VideoIdx = 0x80000000;`",
            "arithmetic": "For a DXSB stream at index i encountered while m_VideoIdx < 0: SMS_CodecOpen returns 1 (early return), so the `else goto addSub` branch is not taken; the inner `if (m_ID != DXSB)` is false, so the body is skipped entirely and i is never appended to lSubStm. The stream is lost. Only DXSB streams at an index AFTER the one that set m_VideoIdx reach addSub.",
            "converter_impact": "When muxing DXSB subtitles into AVI, the 'vids' stream carrying the real video MUST be stream 0 (or at least come before every DXSB stream) in the strl chunk order. Also cap subtitle streams at 8: src/SMS_Player.c:1531 `#define MAX_SUB 8`."
          },
          {
            "claim": "Maximum 8 streams. dwStreams > 8 in the avih makes _NewStream return NULL and the entire AVI is rejected (SMS_GetContainerAVI returns 0).",
            "kind": "HARD",
            "evidence": "include/SMS_Container.h:56 `#define SMS_MAX_STREAMS 8` and :79 `SMS_Stream* m_pStm[ SMS_MAX_STREAMS ];`  src/SMS_ContainerAVI.c:100 `if ( apCont -> m_nStm < SMS_MAX_STREAMS ) {` (else retVal stays NULL)  src/SMS_ContainerAVI.c:438-442 `lCount = File_GetUInt ( lpFileCtx ); /* dwStreams */ for ( i = 0; i < lCount; ++i ) if ( !_NewStream ( apCtx ) ) goto error;`  src/SMS_ContainerAVI.c:721-722 `if ( lStmIdx != apCtx -> m_nStm - 1 ) error: retVal = 0;`",
            "arithmetic": "_NewStream succeeds for m_nStm = 0..7 (8 calls) and returns NULL on the 9th. dwStreams >= 9 => goto error => retVal = 0 => src/SMS_ContainerAVI.c:1179 `if ( ( retVal = _ReadHeader ( apCont ) ) )` fails => container rejected.",
            "converter_impact": "Hard cap: at most 8 streams total (video + audio + subtitle + text) in the avih dwStreams field and in the file."
          },
          {
            "claim": "The number of 'strh' chunks must EXACTLY equal avih.dwStreams, or the file is rejected.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:450 `++lStmIdx;` (once per strh) and :720-722 `endOfHeader: if ( lStmIdx != apCtx -> m_nStm - 1 ) error: retVal = 0;` where m_nStm was set by the avih dwStreams loop.",
            "converter_impact": "Never write a padded or short dwStreams. One strl/strh per declared stream, no more, no fewer."
          },
          {
            "claim": "strh fccType must be exactly 'vids', 'auds' or 'txts'. Any other type (e.g. 'mids' MIDI) hits `default: goto error` and rejects the whole file.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:455-566 `switch ( lTag ) { case SMS_MKTAG( 'v','i','d','s' ): ... case SMS_MKTAG( 'a','u','d','s' ): ... case SMS_MKTAG( 't','x','t','s' ): lCodecType = SMS_CodecTypeUnknown; ... default: goto error;`",
            "converter_impact": "Restrict to vids/auds/txts. 'txts' streams are parsed but forced to SMS_CodecID_NULL at src/SMS_ContainerAVI.c:629-631, so they are inert placeholders, not usable subtitles (DXSB subtitles are a 'vids' stream, not 'txts')."
          },
          {
            "claim": "Video strf (BITMAPINFOHEADER) must be AT LEAST 40 bytes. A shorter strf causes an unsigned 32-bit underflow in the skip computation and seeks the file pointer to a garbage position, corrupting the rest of header parsing.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:395 `uint32_t lTag, lSubTag, lSize, lnFrames, lLen, lStmIdx, lFlags = 0;`  src/SMS_ContainerAVI.c:603-605 `lLen = lSize - 40; lpFileCtx -> Seek ( lpFileCtx, lpFileCtx -> m_CurPos + lLen );`",
            "arithmetic": "lLen and lSize are uint32_t. lSize = 16 (a BITMAPCOREHEADER-ish strf) gives lLen = 16 - 40 = 0xFFFFFFE8 = 4294967272, and CurPos + lLen wraps modulo 2^32 to CurPos - 24. Only lSize >= 40 is safe.",
            "converter_impact": "Always emit a full 40-byte BITMAPINFOHEADER for vids strf. Reject/repair anything smaller."
          },
          {
            "claim": "Video codec extradata (anything in strf beyond the 40-byte BITMAPINFOHEADER, e.g. an out-of-band MPEG-4 VOL/VOS header) is SKIPPED and DISCARDED. m_pUserData is never populated on the AVI video path.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:586-611 reads exactly the 40 header bytes then `lLen = lSize - 40; lpFileCtx -> Seek ( lpFileCtx, lpFileCtx -> m_CurPos + lLen ); if ( lLen & 1 ) File_GetByte ( lpFileCtx ); lpCodecCtx -> m_Type = SMS_CodecTypeVideo; lpCodecCtx -> m_Tag = lSubTag; lpCodecCtx -> m_ID = SMS_CodecGetID ( SMS_CodecTypeVideo, lSubTag );` — no assignment to m_pUserData or m_UserDataLen anywhere in the file. (Compare the audio path, which DOES fill m_pUserData at src/SMS_Container.c:305-307.)",
            "converter_impact": "The MPEG-4 elementary stream must be self-describing IN BAND: every keyframe chunk must carry VOS/VO/VOL headers before the VOP. Do not use ffmpeg's 'global header' mode for AVI output; the VOL must be repeated in the stream."
          },
          {
            "claim": "Audio strf must be >= 14 bytes; extradata (cbSize block) is only read when strf size > 16, is clamped to (strfSize - 18), and its length is stored in a SIGNED SHORT, so a cbSize > 32767 flips negative and makes the AAC decoder take the ADTS/ADIF path instead of the AudioSpecificConfig path.",
            "kind": "HARD",
            "evidence": "src/SMS_Container.c:288-310 `int lID = File_GetUShort ( apFileCtx ); ... apCodecCtx -> m_BitsPerSample = aSize == 14 ? 8 : File_GetUShort ( apFileCtx ); ... if ( aSize > 16 ) { int lLen = File_GetUShort ( apFileCtx ); if ( lLen > 0 ) { if ( lLen > aSize - 18 ) lLen = aSize - 18; apCodecCtx -> m_pUserData = malloc ( ( lLen + 15 ) & ~15 ); apFileCtx -> Read ( apFileCtx, apCodecCtx -> m_pUserData, lLen ); apCodecCtx -> m_UserDataLen = lLen; }`  include/SMS_Codec.h:81 `short m_UserDataLen;`  src/SMS_AAC.c:8262-8264 `if ( apCtx -> m_pUserData && apCtx -> m_UserDataLen > 0 ) retVal = _NeAACDecInit2 ( lpAAC, apCtx -> m_pUserData, apCtx -> m_UserDataLen, &lSR, &lnChan ); else retVal = _NeAACDecInit ( lpAAC, apCtx -> m_pUserData, abs ( apCtx -> m_UserDataLen ), &lSR, &lnChan );`",
            "arithmetic": "m_UserDataLen is a 16-bit signed short. lLen in [1,32767] is stored faithfully; lLen in [32768,65535] becomes negative (lLen - 65536), so the `m_UserDataLen > 0` test fails. Real AudioSpecificConfig is 2-5 bytes, so this is only a malformed-input concern. Reading below aSize=14 is not guarded at all — the function unconditionally consumes 14 or 16 bytes.",
            "converter_impact": "Emit strf >= 18 bytes for AAC (WAVEFORMATEX + cbSize) with the AudioSpecificConfig in the cbSize block; keep cbSize <= 32767 (in practice <= 5). For MP2/MP3/AC3/PCM a plain 16- or 18-byte WAVEFORMATEX is fine. Never emit an audio strf smaller than 14 bytes."
          },
          {
            "claim": "MAXIMUM CHUNK SIZE: a video chunk may not exceed 2,097,088 bytes and an audio chunk may not exceed 524,224 bytes. An over-sized chunk makes the ring allocator return NULL and _ReadPacket bails out mid-chunk, leaving the byte-scanner desynchronised.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:822-824 `SMS_AVPacket* lpPkt = apCont -> AllocPacket ( lpStm -> m_pPktBuf, lSize ); if ( !lpPkt || lpBuf -> Read ( lpBuf, lpPkt -> m_pData, lSize ) != lSize ) return 0;`  src/SMS_Container.c:53 `SMS_AVPacket* lpPkt = aSize ? SMS_RingBufferAlloc ( apRB, aSize + 64 ) : &s_EmptyPacket;`  src/SMS_RingBuffer.c:74-76 `aSize = ( aSize + 63 ) & 0xFFFFFFC0; if ( aSize <= apRB -> m_Capacity ) {` (retVal stays NULL otherwise)  src/SMS_Player.c:54-55 `#define SMS_VP_BUFFER_SIZE ( 1024 * 1024 * 2 )` / `#define SMS_AP_BUFFER_SIZE ( 1024 * 512 )`  src/SMS_RingBuffer.c:28 `retVal -> m_Size = retVal -> m_Capacity = aSize;`",
            "arithmetic": "Requirement: roundup64(chunkSize + 64) <= Capacity. Capacity is a multiple of 64, so this is chunkSize + 64 <= Capacity, i.e. chunkSize <= Capacity - 64.  VIDEO: 2*1024*1024 - 64 = 2097152 - 64 = 2,097,088 bytes.  AUDIO: 512*1024 - 64 = 524288 - 64 = 524,224 bytes.  (The 64-byte overhead is the SMS_AVPacket header: src/SMS_Container.c:62 `lpPkt -> m_pData = ( ( unsigned char* )lpPkt ) + 64;`.)",
            "converter_impact": "CLAMP: cap the encoder's max per-frame size well under 2,097,088 bytes for video and 524,224 for audio. In practice go much lower — a single chunk near the ceiling monopolises the whole ring buffer and stalls the pipeline (SOFT), so target <= 256 KB video / <= 32 KB audio per chunk. Do not use huge interleave blocks for audio."
          },
          {
            "claim": "CHUNK ID GRAMMAR IS STRICT: only `NNdc`, `NNdb`, `NNwb`, `NNsb`, `NN__` (NN = two ASCII digits) are recognised as data chunks, plus `ixNN` as a skippable index. Anything else (JUNK, `NNtx`, `NNpc`, `LIST rec `) is NOT skipped — the demuxer slides a byte-at-a-time window over it, so arbitrary payload bytes can be mis-parsed as a chunk header.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:782-816 `lSize = File_GetByte ( lpBuf ); ... lChunkID = *( uint16_t* )&lData[ 2 ]; if ( lData[ 0 ] >= '0' && lData[ 0 ] <= '9' && lData[ 1 ] >= '0' && lData[ 1 ] <= '9' && ( lChunkID == 0x6364 || /* dc */ lChunkID == 0x6277 || /* wb */ lChunkID == 0x6273 || /* sb */ lChunkID == 0x6264 || /* db */ lChunkID == 0x5F5F /* __ */ ) && lCond ) {`  and :799-803 `*( uint16_t* )&lData[ 0 ] == 0x7869 && /* ix */ lCond ) File_Skip ( lpBuf, lSize );`",
            "arithmetic": "lChunkID is a little-endian u16 over lData[2],lData[3]: 'd','c' -> 0x63<<8|0x64 = 0x6364; 'd','b' -> 0x6264; 'w','b' -> 0x6277; 's','b' -> 0x6273; '_','_' -> 0x5F5F. 'ix' -> lData[0]='i'(0x69), lData[1]='x'(0x78) -> 0x7869.",
            "converter_impact": "Inside `movi`, emit ONLY the five recognised chunk kinds. Avoid `LIST rec ` wrappers and JUNK padding inside movi; if padding is unavoidable, fill it with 0x00 (never with bytes that could form `NN` ASCII digits followed by dc/db/wb/sb/__)."
          },
          {
            "claim": "CRASH TRAP: the stream-index bounds check in _ReadPacket is dead. lCond is a SUM of two booleans (OR semantics) and its stream-index term is computed from the WRONG two bytes, so `apCont->m_pStm[lCount]` is dereferenced with lCount in [0,99] against an 8-element array with no effective guard.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:796-797 `lCount = ( ( int )lData[ 2 ] - '0' ) * 10 + ( ( int )lData[ 3 ] - '0' ); lCond = ( lCount < ( int32_t )apCont -> m_nStm ) + ( i + lSize <= ( uint32_t )lpAVICont -> m_MoviEnd );`  then :805 `lCount = ( ( int )lData[ 0 ] - '0' ) * 10 + ( ( int )lData[ 1 ] - '0' );`  then :818-820 `SMS_Stream* lpStm = apCont -> m_pStm[ lCount ]; if ( lpStm -> m_pPktBuf ) {`  Array declared at include/SMS_Container.h:79 `SMS_Stream* m_pStm[ SMS_MAX_STREAMS ];` and the container is zero-filled at src/SMS_Container.c:118 `SMS_Container* retVal = ( SMS_Container* )calloc ( 1, sizeof ( SMS_Container ) );`",
            "arithmetic": "For a normal chunk '00dc': lData[2]='d'(0x64), lData[3]='c'(0x63), so line 796 computes lCount = (0x64-0x30)*10 + (0x63-0x30) = 52*10 + 51 = 571. `571 < m_nStm` is 0, so lCond reduces to the size test alone — the index test contributes nothing. Line 805 then recomputes lCount = 10*(lData[0]-'0') + (lData[1]-'0') in [0,99] and indexes m_pStm with it, unchecked. lCount in [m_nStm,7] -> NULL pointer deref of lpStm->m_pPktBuf. lCount in [8,99] -> reads 4*(lCount-8) bytes past the array into m_Duration/m_StartTime/m_nStm and treats the result as an SMS_Stream*.",
            "converter_impact": "FORBID: the two-digit stream index in every movi chunk id must be < the actual stream count AND <= 7. Zero-pad single digits ('00dc', not '0dc'). Never emit '10dc'-'99dc' style ids. This is a crash, not a graceful failure."
          },
          {
            "claim": "CRASH TRAP (OpenDML): the ODML 'indx' stream-ID bound is off by one (`>` instead of `>=`), so an indx whose chunk id names stream number == the stream count dereferences a NULL SMS_Stream*.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:251-263 `int i, lCID = File_GetUInt ( lpFileCtx ); int lStID = 10 * ( ( lCID & 0xFF ) - '0' ) + ( ( ( lCID >> 8 ) & 0xFF ) - '0' ); ... if ( ( uint32_t )lStID > apCont -> m_nStm || lStID < 0 ) return 0; lpStm = apCont -> m_pStm[ lStID ];`  then :282 `lpAVIStm = MYSTRM( lpStm );` which expands (src/SMS_ContainerAVI.c:24 `#define MYSTRM( s ) ( ( _AVIStream* )s -> m_pCtx )`) to `lpStm -> m_pCtx` on a NULL pointer.",
            "arithmetic": "For a 2-stream file (m_nStm = 2), lStID = 2 passes the guard because 2 > 2 is false. m_pStm[2] is NULL (calloc-zeroed), and MYSTRM(NULL) reads offset 0x44 from address 0.",
            "converter_impact": "When emitting OpenDML indx chunks, the two-digit stream number in the indx's dwChunkId must be strictly less than the stream count. Correct bound is `>=`; the source has `>`."
          },
          {
            "claim": "OpenDML 'indx' completely REPLACES idx1: when any indx chunk is parsed successfully, _LoadIndex (and therefore idx1) is never read.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:324 `MYCONT( apCont ) -> m_fODML = 1;` (end of _LoadODMLIdx)  src/SMS_ContainerAVI.c:727-729 `if ( MYCONT( apCtx ) -> m_fODML ) { apCtx -> m_Flags |= SMS_CONT_FLAGS_SEEKABLE; } else retVal = _LoadIndex ( apCtx );`",
            "converter_impact": "Do not emit a half-populated indx alongside a good idx1 expecting idx1 to win — it will not be read. Either emit a complete, correct OpenDML index hierarchy, or emit no indx at all and rely on idx1."
          },
          {
            "claim": "Seeking requires that EVERY stream has at least one index entry; one un-indexed stream disables seeking for the whole file.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:1114 `if ( aPos >= lpStm -> m_nIdx ) return 0;`  src/SMS_ContainerAVI.c:1123-1131 `for ( i = 0; i < ( int )apCont -> m_nStm; ++i ) { ... lpStm = MYSTRM( apCont -> m_pStm[ i ] ); if ( lpStm -> m_nIdx <= 0 ) return 0;`",
            "converter_impact": "idx1 must contain entries for every stream, including audio and any DXSB subtitle stream. A video-only idx1 makes the file unseekable."
          },
          {
            "claim": "idx1 dwChunkOffset must be relative to the file offset of the 'movi' FOURCC (ffmpeg convention: 4 for the first chunk). SMS adds m_MoviList, which is the offset of the 'movi' FOURCC itself.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:421-422 `MYCONT( apCtx ) -> m_MoviList = lpFileCtx -> Seek ( lpFileCtx, lpFileCtx -> m_CurPos - 4 ); MYCONT( apCtx ) -> m_MoviEnd = MYCONT( apCtx ) -> m_MoviList + lSize;` (CurPos is just past the 'movi' subtag, so CurPos-4 is the 'movi' FOURCC offset; Seek returns the new position, e.g. src/SMS_FileContext.c:534 `return apCtx -> m_CurPos = aPos;`)  src/SMS_ContainerAVI.c:1154 `if ( !MYCONT( apCont ) -> m_fOffsetBug ) lPos += MYCONT( apCont ) -> m_MoviList;`",
            "converter_impact": "Use the standard ffmpeg/AVI convention. Absolute-offset idx1 is also tolerated, but only via the fragile heuristic described next."
          },
          {
            "claim": "SEEK TRAP: _CheckOffsetBug decides 'idx1 offsets are absolute' by reading two bytes at file offset m_pIdx[0].m_Pos and testing for ASCII '0'. With a standard movi-relative idx1 (first entry = 4) it reads bytes 4-5 of the file, i.e. the low two bytes of dwRIFFSize, and produces a FALSE POSITIVE whenever either equals 0x30 — permanently breaking every seek in that file.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:377-387 `if ( lpStm -> m_nIdx ) { char lBuf[ 2 ]; uint32_t lPos = lpFileCtx -> m_CurPos; lpFileCtx -> Seek ( lpFileCtx, lpStm -> m_pIdx[ 0 ].m_Pos ); lpFileCtx -> Read ( lpFileCtx, lBuf, 2 ); lpFileCtx -> Seek ( lpFileCtx, lPos ); if ( lBuf[ 0 ] == '0' || lBuf[ 1 ] == '0' ) lpCont -> m_fOffsetBug = 1; }`  Consequence at src/SMS_ContainerAVI.c:1154 `if ( !MYCONT( apCont ) -> m_fOffsetBug ) lPos += MYCONT( apCont ) -> m_MoviList;`",
            "arithmetic": "With the standard convention m_pIdx[0].m_Pos = 4. Bytes at file offsets 4 and 5 are byte0 and byte1 of dwRIFFSize = fileSize - 8. False positive iff ((fileSize-8) & 0xFF) == 0x30 OR (((fileSize-8) >> 8) & 0xFF) == 0x30. That is 2/256 - 1/65536 = 511/65536 ~= 0.78% of all file sizes. When it fires, the seek target loses the +m_MoviList term and lands inside the RIFF header, so every seek corrupts playback.",
            "converter_impact": "WARN + auto-fix: after muxing, read bytes 4 and 5 of the output file; if either is 0x30, append 1-2 bytes of trailing padding (or adjust a JUNK chunk) until neither is 0x30, then rewrite dwRIFFSize. Cheap, deterministic, eliminates the trap."
          },
          {
            "claim": "Maximum 8 RIFF segments are tracked for seeking. An OpenDML file with more than 8 RIFF/AVIX chunks still PLAYS past segment 8 (the packet reader chains them dynamically) but cannot SEEK past it.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:62-64 `uint32_t m_RiffEndList [ 8 ]; uint32_t m_MoviEndList [ 8 ]; uint32_t m_MoviListList[ 8 ];`  src/SMS_ContainerAVI.c:341 `for ( i = 1; i < 8; ++i ) {`  src/SMS_ContainerAVI.c:1089-1103 `for ( i = 0; i < lpCont -> m_nChunks; ++i ) { if ( aPos < lpCont -> m_MoviEndList[ i ] ) { ... break; } } return i != lpCont -> m_nChunks;`  (playback chaining is independent, at src/SMS_ContainerAVI.c:760-780).",
            "arithmetic": "m_nChunks <= 8. If a seek target lies beyond m_MoviEndList[7], the loop exhausts and _FixOffsets returns `i != m_nChunks` = 0, so _Seek at :1160 returns 0. With the usual 1 GiB RIFF segmentation that is roughly the first 8 GiB — but see the 4 GiB uint32 ceiling below, which binds first.",
            "converter_impact": "Keep OpenDML output to at most 8 RIFF segments (which the 4 GiB limit already forces)."
          },
          {
            "claim": "Every file offset in the AVI path is 32-bit unsigned. The file must be under 4 GiB (4,294,967,296 bytes) or RIFF/movi bounds and index positions wrap.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:56-64 `uint32_t m_BitRate; uint32_t m_RiffEnd; uint32_t m_MoviEnd; uint32_t m_MoviList;`  src/SMS_ContainerAVI.c:29-35 `typedef struct _AVIdxEntry { uint32_t m_Pos __attribute__( ( packed ) ); ... }`  src/SMS_ContainerAVI.c:77-78 `*apRiffEnd = File_GetUInt ( apFileCtx ); *apRiffEnd += apFileCtx -> m_CurPos;`  src/SMS_ContainerAVI.c:306 `lpIndex -> m_Pos = ( uint32_t )lPos;` (explicit truncation of the 64-bit ODML offset)  include/SMS_FileContext.h:66-68 `unsigned int m_Size; unsigned int m_Pos; unsigned int m_CurPos;`",
            "arithmetic": "2^32 = 4,294,967,296. Any offset at or beyond that wraps modulo 2^32. The ODML path computes `int64_t lPos = File_GetUInt ( lpFileCtx ) + lBase - 8;` (:295) in 64-bit and then truncates to uint32 at :306, so an entry past 4 GiB indexes into the beginning of the file.",
            "converter_impact": "CLAMP: hard-cap output AVI size below 4 GiB. Split into separate files above that, not into >4 GiB OpenDML."
          },
          {
            "claim": "DIVISION-BY-ZERO: audio strf nAvgBytesPerSec must be non-zero. SMSContainer_CalcPktFields divides by m_BitRate (= nAvgBytesPerSec * 8) unconditionally for every audio packet, before any guard.",
            "kind": "HARD",
            "evidence": "src/SMS_Container.c:296 `apCodecCtx -> m_BitRate = File_GetUInt ( apFileCtx ) * 8;`  src/SMS_Container.c:191-199 `} else if ( lpCodecCtx -> m_Type == SMS_CodecTypeAudio ) { lNum = lpCodecCtx -> m_FrameSize <= 1 ? ( apPkt -> m_Size * 8 * lpCodecCtx -> m_SampleRate ) / lpCodecCtx -> m_BitRate : lpCodecCtx -> m_FrameSize; lDen = lpCodecCtx -> m_SampleRate; } else return; if ( lDen && lNum )` — the `if ( lDen && lNum )` guard runs AFTER the division. Called from the AVI reader at src/SMS_ContainerAVI.c:855 `SMSContainer_CalcPktFields ( lpStm, lpPkt );`",
            "arithmetic": "m_FrameSize is 0 for AVI audio (calloc'd at src/SMS_ContainerAVI.c:107 and never assigned anywhere in SMS_ContainerAVI.c — grep for `m_FrameSize =` shows only ASF/MOV/decoder-internal writers), so `m_FrameSize <= 1` is always true on the first packets and the divide always executes. Divisor = nAvgBytesPerSec * 8. nAvgBytesPerSec = 0 => divide by zero on the R5900 (undefined quotient at best, trap if the compiler emitted the teq check).",
            "converter_impact": "REQUIRE nAvgBytesPerSec != 0 in every audio strf. Also set it accurately: it drives all audio packet durations and hence A/V sync."
          },
          {
            "claim": "DIVISION-BY-ZERO: audio strh dwRate must be non-zero. It becomes m_TimeBase.m_Den, which is the divisor in every SMS_Rescale call on the packet timestamps. Unlike the video path there is NO zero fallback for audio.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:537-540 `MYSTRM( lpStm ) -> m_Scale = File_GetUInt ( lpFileCtx ); /* dwScale */ MYSTRM( lpStm ) -> m_Rate = File_GetUInt ( lpFileCtx ); /* dwRate */ SMSContainer_SetPTSInfo ( lpStm, MYSTRM( lpStm ) -> m_Scale, MYSTRM( lpStm ) -> m_Rate );`  src/SMS_Container.c:176-178 `apStm -> m_TimeBase.m_Num = aPTSNum; apStm -> m_TimeBase.m_Den = aPTSDen;`  src/SMS_Container.c:229 `apPkt -> m_PTS = SMS_Rescale ( apPkt -> m_PTS, SMS_TIME_BASE * ( int64_t )apStm -> m_TimeBase.m_Num, apStm -> m_TimeBase.m_Den );`  src/SMS_EE.c:103-106 `if ( aB <= INT_MAX && aC <= INT_MAX ) { if ( anA <= INT_MAX ) return ( anA * aB + lR ) / aC;`",
            "arithmetic": "aC = m_TimeBase.m_Den = dwRate. dwRate = 0 => integer division by zero. The video path guards this (src/SMS_ContainerAVI.c:479-493 `if ( !lScale || !lRate ) { ... lRate = 25; lScale = 1; }`); the audio path at :537-540 has no such fallback, only a partial guard on the duration at :551 `if ( MYSTRM( lpStm ) -> m_Rate ) lpStm -> m_Duration = ...`.",
            "converter_impact": "REQUIRE audio strh dwRate != 0 and dwScale != 0. For CBR audio use dwScale=1, dwRate=nAvgBytesPerSec with dwSampleSize=nBlockAlign; for VBR-style (MP3 chunk-per-frame) use dwSampleSize=0 with dwScale/dwRate expressing chunks-per-second."
          },
          {
            "claim": "PLAYER-LEVEL HARD CEILING (applies to AVI because m_Width/m_Height come straight from the BITMAPINFOHEADER): video width > 1024 OR height > 1024 aborts playback with STR_RESOLUTION_TOO_BIG.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:588-589 `lpCodecCtx -> m_Width = File_GetUInt ( lpFileCtx ); lpCodecCtx -> m_Height = File_GetUInt ( lpFileCtx );`  src/SMS_Player.c:1676-1681 `if ( lpVideoCtx -> m_Width > 1024 || lpVideoCtx -> m_Height > 1024 ) { s_Player.m_pErrorMsg = &STR_RESOLUTION_TOO_BIG; goto destroy; }`",
            "arithmetic": "Strict `>`, so 1024x1024 is accepted and 1025 in either dimension is rejected. Note the check runs AFTER SMS_CodecOpen but BEFORE the decoder's Init, so it is a clean rejection, not a crash.",
            "converter_impact": "CLAMP output resolution to <= 1024 in both dimensions. (Real-world SOFT ceiling is far lower — PS2 EE MPEG-4 software decode; 640x480 / 720x400 is the practical target — but 1024 is the source-confirmed HARD wall.)"
          },
          {
            "claim": "An AVI with no idx1 and no indx still loads and plays, but is not marked seekable.",
            "kind": "SOFT",
            "evidence": "src/SMS_ContainerAVI.c:212-242 `_LoadIndex`: `if ( lTag == SMS_MKTAG( 'i','d','x','1' ) && ( retVal = _Load_idx1 ( apCont, lSize ) ) ) { apCont -> m_Flags |= SMS_CONT_FLAGS_SEEKABLE; break; }` ... `return retVal >= 0 ? 1 : -1;` — retVal stays 0 when no idx1 is found, and 0 >= 0 so the function still returns 1 (success). src/SMS_ContainerAVI.c:139 `if ( lnIdx <= 0 ) return 0;`",
            "converter_impact": "Always write idx1 (it costs 16 bytes per chunk) — without it the user gets a file that plays but cannot be scrubbed, fast-forwarded or resumed."
          },
          {
            "claim": "Index memory is unbounded and allocation failure is unchecked: _Load_idx1 loops idx1Size/16 times, growing a heap array of 12-byte entries with no cap, and a failed realloc is dereferenced immediately.",
            "kind": "SOFT",
            "evidence": "src/SMS_ContainerAVI.c:128 `int i, lnIdx = aSize / 16;`  src/SMS_ContainerAVI.c:29-35 `typedef struct _AVIdxEntry { uint32_t m_Pos ...; uint32_t m_CumLen ...; uint32_t m_fKey ...; } _AVIdxEntry;`  src/SMS_ContainerAVI.c:185-192 `lpIndices = SMS_Realloc ( lpStm -> m_pIdx, ( unsigned int * )( &lpStm -> m_IdxAllocSize ), ( lpStm -> m_nIdx + 1 ) * sizeof ( _AVIdxEntry ) ); lpStm -> m_pIdx = lpIndices; lpIndex = &lpIndices[ lpStm -> m_nIdx++ ];` — no NULL check.  src/SMS_EE.c:45-52 `if ( aMinSize < *apSize ) return apData; *apSize = 17 * aMinSize / 16 + 32; return realloc ( apData, *apSize );`",
            "arithmetic": "Entries = idx1Size / 16. RAM per entry = sizeof(_AVIdxEntry) = 12 bytes (3 packed uint32). Peak RAM ~= (17/16) * 12 * idx1Size/16 = 0.797 * idx1Size. Worked example: 2-hour 25 fps AVI, 2 streams, one chunk per frame per stream = 2 * 180,000 = 360,000 entries => idx1 = 5,760,000 bytes on disk => ~4.6 MB resident. That is fine on a 32 MB PS2 alongside the 2 MB video ring + 512 KB audio ring, but the ceiling is bounded only by malloc success, and a NULL return is written through immediately.",
            "converter_impact": "WARN (do not forbid) above roughly 500,000 total index entries. Prefer one chunk per video frame and reasonably large audio chunks over per-sample audio chunking, which multiplies idx1 entries."
          },
          {
            "claim": "AAC, AC3 and DTS are force-downmixed to 2 channels (AC3/DTS keep 5 channels only when SPDIF passthrough is enabled). No other audio codec gets a channel clamp.",
            "kind": "SOFT",
            "evidence": "src/SMS_Player.c:100-114 `static void _check_ac3 ( SMS_Stream* apStm ) { if ( apStm -> m_pCodec -> m_ID == SMS_CodecID_AC3 || apStm -> m_pCodec -> m_ID == SMS_CodecID_DTS ) { if ( !( g_Config.m_PlayerFlags & SMS_PF_SPDIF ) || ( g_Config.m_PlayerFlags & SMS_PF_PDW22 ) ) { if ( apStm -> m_pCodec -> m_Channels > 2 ) apStm -> m_pCodec -> m_Channels = 2; ... } else { if ( apStm -> m_pCodec -> m_ID == SMS_CodecID_AAC && apStm -> m_pCodec -> m_Channels > 2 ) apStm -> m_pCodec -> m_Channels = 2; ...`",
            "converter_impact": "Encode AVI audio as stereo. Multichannel MP2/MP3/PCM/WMA/Vorbis has no clamp at all and its behaviour past 2 channels is UNCONFIRMED from this reading — do not ship it."
          }
        ],
        "gaps": [
          "The practical (SOFT) performance ceiling for MPEG-4/MSMPEG4 software decode on the EE -- max resolution, bitrate, B-frame usage, GMC/QPel support -- is NOT determinable from SMS_ContainerAVI.c or SMS_Codec.c. It would require auditing src/SMS_MPEG4.c and src/SMS_MSMPEG4.c. The only source-confirmed video dimension limit is the 1024x1024 rejection in SMS_Player.c:1676.",
          "Supported audio sample rates are UNCONFIRMED here. The AVI path passes m_SampleRate straight through (src/SMS_Container.c:295) to SPU_InitContext (src/SMS_Player.c:188-191). Whether the SPU2 path resamples arbitrary rates or only accepts 48000/44100/etc. requires reading the SPU context code, which was out of scope for this AVI audit.",
          "Whether the MIPS toolchain emits a trapping `teq $divisor,$zero` for the two division-by-zero sites (src/SMS_Container.c:193 and src/SMS_EE.c:105) is UNCONFIRMED -- I could not determine from the source whether the failure mode is a hard exception or a garbage quotient. Either way both are converter-actionable (never emit nAvgBytesPerSec == 0 or audio dwRate == 0).",
          "Multichannel (>2ch) behaviour for MP2/MP3/PCM16LE/WMA/Vorbis in AVI is UNCONFIRMED. Only AAC/AC3/DTS have an explicit clamp (src/SMS_Player.c:100-114); the others are passed through to SPU_InitContext unmodified and I did not trace what SPU_InitContext does with >2.",
          "src/SMS_ContainerAVI.c:135,176 contains a pre-existing out-of-bounds read that fires on EVERY AVI with an idx1: `int lPrevIdx = -1;` then on the first index entry whose stream differs from lPrevIdx, `if ( lnMaxSameIdx < lnSameIdx && !( apCtx -> m_pStm[ lPrevIdx ] -> m_Flags & SMS_STRM_FLAGS_AUDIO ) )` dereferences m_pStm[-1]. Since -1 < 1 is true, the second operand IS evaluated. The computed value is then discarded (line 208 `return 1;//lnMaxSameIdx > 256 ? -1 : 1;` -- the use is commented out). Empirically benign (AVIs play), so I cannot characterise the real-world impact, and it is not converter-actionable. Flagging it only so it is not mistaken for a limit.",
          "I did not verify what the audio decoders do when a wFormatTag matches via the toupper alias path (e.g. a genuine 0x0075 Voxware stream handed to SMS_Codec_MP123_Open). The mis-detection itself is source-confirmed; the downstream failure mode (silence vs. noise vs. hang) is UNCONFIRMED."
        ]
      },
      {
        "area": "MP4 / MOV / M4A container path (src/SMS_ContainerMOV.c + src/SMS_Codec.c and the pieces they feed)",
        "findings": [
          {
            "claim": "The file MUST begin with a box whose type field (bytes 4..7) is literally 'ftyp' AND whose bytes 8..11 (major_brand) are non-zero. Nothing else is probed. A classic QuickTime .mov that starts with 'moov'/'mdat'/'wide'/'free' is rejected outright by BOTH SMS_GetContainerMOV and SMS_GetContainerM4A.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:1011-1027 `static uint32_t _mov_probe ( FileContext* apFileCtx ) { ... lSize = File_GetUInt; lType = File_GetUInt; lTag = File_GetUInt; lSize = lType == SMS_MKTAG( 'f', 't', 'y', 'p' ) ? lTag : 0; ... return lSize; }` and src/SMS_ContainerMOV.c:1259-1261 `uint32_t lType = _mov_probe ( lpFileCtx ); if ( lType ) {` — when _mov_probe returns 0 the whole container creator returns 0.",
            "arithmetic": "major_brand == 0x00000000 also yields lSize == 0, so a brand of four NUL bytes fails the probe even though 'ftyp' is present.",
            "converter_impact": "FORBID output without a leading ftyp box; require a real 4-char major brand (e.g. 'isom'/'mp42'). Never emit a bare-moov QuickTime file."
          },
          {
            "claim": "Container routing is by FILE EXTENSION, and '.mov' does not exist anywhere in the codebase. '.mp4' -> SMS_CONTAINER_MOV (video kept). '.m4a' -> SMS_CONTAINER_M4A (every video trak is freed). Any other extension (including .mov, .m4v) falls through to the default probe order, where the M4A creator sits at index 3 and the MOV creator at index 11 — so the audio-only reader claims the file first and the video track is destroyed.",
            "kind": "HARD",
            "evidence": "src/SMS_FileDir.c:150-153 `else if ( !stricmp ( lpExt, s_pM4A ) ) retVal = SMS_CONTAINER_M4A; else if ( !stricmp ( lpExt, s_pMP4 ) ) retVal = SMS_CONTAINER_MOV;` — grep for s_pMOV / \".mov\" over src/ and include/ returns nothing. src/SMS_Container.c:32-47 `static const ContainerCreator s_CCreator[] = { SMS_GetContainerAVI, SMS_GetContainerASF, SMS_GetContainerOGG, SMS_GetContainerM4A, ... SMS_GetContainerMOV, /* index 11 */ NULL };`. src/SMS_ContainerMOV.c:252-258 `else if ( lSubType == SMS_MKTAG( 'v','i','d','e' ) || ... ) { free ( lpStm -> m_pCodec ); free ( lpStm ); apCtx -> m_pBase -> m_pStm[ ... ] = NULL; }`.",
            "arithmetic": "SMS_ContID returns -1 for an unknown extension; the guard `lID < sizeof(s_CCreator)/sizeof(s_CCreator[0])` compares int -1 against an unsigned size_t, so it converts to 0xFFFFFFFF and fails — the ordered fallback list is used, M4A (3) before MOV (11).",
            "converter_impact": "HARD REQUIREMENT: name every video output '.mp4'. Naming it '.mov' or '.m4v' silently produces an audio-only playback with the music visualiser. Use '.m4a' only for audio-only output."
          },
          {
            "claim": "Video codec identity comes ONLY from the stsd sample-entry 4CC via s_CodecVideoTags (compared byte-wise case-insensitively). The complete accepted set is: DIVX, DX50, XVID, MP4S, M4S2, 0x04000000, DIV1, BLZ0, mp4v, UMP4, FMP4 -> SMS_CodecID_MPEG4; DIV3, MP43, MPG3, DIV5, DIV6, DIV4, AP41, COL1, COL0 -> SMS_CodecID_MSMPEG4V3; DXSB -> subtitle. Anything else (avc1, avc3, hvc1, hev1, mp2v, mjpa, jpeg, s263, ...) maps to SMS_CodecID_NULL.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:32-55 `static SMS_CodecTag s_CodecVideoTags[] = { { SMS_CodecID_MPEG4, SMS_MKTAG( 'm','p','4','v' ) }, ... { SMS_CodecID_DXSB, SMS_MKTAG( 'D','X','S','B' ) }, { SMS_CodecID_NULL, ... } };`; src/SMS_Codec.c:74-88 the toupper-per-byte compare loop returning SMS_CodecID_NULL on fall-through; src/SMS_ContainerMOV.c:286 `lpCodecCtx -> m_ID = SMS_CodecGetID ( lpStm -> m_pCodec -> m_Type, lFmt );`; src/SMS_Codec.c:117 `default: break;` in SMS_CodecOpen leaves m_pCodec NULL and SMS_CodecOpen returns 0.",
            "arithmetic": "File_GetUInt reads the 4CC as a native little-endian word; SMS_MKTAG(a,b,c,d) = a | b<<8 | c<<16 | d<<24, so file bytes 'm','p','4','v' == 0x7634706D == SMS_MKTAG('m','p','4','v'). The two encodings agree exactly.",
            "converter_impact": "FORBID H.264/H.265 in MP4 — the demuxer accepts the file but the track never opens. The only usable MP4 video sample entry is 'mp4v' (MPEG-4 Part 2, i.e. -c:v mpeg4)."
          },
          {
            "claim": "The nested-box parse of a VIDEO sample entry starts 2 bytes early, so 'esds' inside a VisualSampleEntry is NEVER dispatched. Consequence: for video tracks m_pUserData stays NULL, m_BitRate stays 0, and the MPEG-4 object-type table has no effect at all.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:277-292 consumes: lStartPos; `_read_int_be` (4, size); `File_GetUInt` (4, format); `File_Skip(8)`; then `File_Skip(16)`; `File_GetShortBE` width (2); `File_GetShortBE` height (2); `File_Skip(46)`; `File_GetShortBE` m_BitsPerSample (2). Then src/SMS_ContainerMOV.c:316-318 `lAtom.m_Size = lSize - ( apFileCtx -> m_CurPos - lStartPos ); if ( lAtom.m_Size > 8 ) _mov_read_dflt ( apCtx, apFileCtx, &lAtom );`. Compare the AUDIO branch at src/SMS_ContainerMOV.c:294-299, which consumes 16+2+6+2+2+4+4 = 36 = the exact AudioSampleEntry header size and therefore lands correctly on the child boxes.",
            "arithmetic": "ISO 14496-12 VisualSampleEntry header = SampleEntry 16 (size4+type4+reserved6+data_reference_index2) + 16 (pre_defined2+reserved2+pre_defined[3]12) + width2 + height2 + horizresolution4 + vertresolution4 + reserved4 + frame_count2 + compressorname32 + depth2 + pre_defined(-1)2 = 86 bytes. SMS consumes 4+4+8+16+2+2+46+2 = 84. Shortfall = 2 bytes (the trailing pre_defined/color_table_id, which the upstream ffmpeg mov.c this file derives from does read). Trace of the miss: _mov_read_dflt then does `lAtom.m_Size = _read_int_be` over bytes [84..87] = 0xFFFF (pre_defined) concatenated with the high 16 bits of the esds box size (0x0000) = 0xFFFF0000; after `lAtom.m_Size -= 8` (SMS_ContainerMOV.c:768) the guard at SMS_ContainerMOV.c:769 `if ( lAtom.m_Size < 0 || lAtom.m_Size > apAtom -> m_Size - lTotalSize ) break;` fires (4294901752 > lSize-92) and the loop exits without ever matching 'esds'. If a muxer writes pre_defined = 0 instead, the `lAtom.m_Size == 0` branch at :764 swallows the rest of the entry and the type word (read from offset 88) is still not 'esds'. Either way esds is skipped.",
            "converter_impact": "CRITICAL and CONTRADICTS the repo's own commit 99b1b5f (\"The MOV demuxer already routes esds into the codec context's m_pUserData\") — verify at runtime before relying on either. If it holds: do NOT let the muxer strip the VOL header out of mp4v samples into esds, because the decoder will never receive it. Practically that means keeping the VOL/VOS start codes in-band on keyframes, or shipping MPEG-4 Part 2 in AVI instead of MP4."
          },
          {
            "claim": "MPEG-1 and MPEG-2 VIDEO are not genuinely accepted inside MP4. s_MP4ObjType does contain 96/97 -> SMS_CodecID_MPEG2 and 106 -> SMS_CodecID_MPEG1, and SMS_CodecOpen does route both to the real IPU decoder — but those rows are only reachable through _mov_read_esds, which for a video sample entry is never dispatched, and there is no MPEG-1/2 entry in the video 4CC table.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:345-357 `const SMS_CodecTag s_MP4ObjType[] = { { SMS_CodecID_MPEG4, 32 }, { SMS_CodecID_AAC, 64 }, { SMS_CodecID_MPEG2, 96 }, { SMS_CodecID_MPEG2, 97 }, { SMS_CodecID_AAC, 102 }, { SMS_CodecID_AAC, 103 }, { SMS_CodecID_MP3, 105 }, { SMS_CodecID_MPEG1, 106 }, { SMS_CodecID_MP3, 107 }, { SMS_CodecID_OGGV, 221 }, { SMS_CodecID_AC3, 226 } };`; src/SMS_Codec.c:98-99 `case SMS_CodecID_MPEG1 : case SMS_CodecID_MPEG2 : SMS_Codec_MPEG12_Open ( apCtx ); break;`; src/SMS_Codec.c:32-55 contains no MPEG1/MPEG2 video 4CC. Reachability blocked per the 84-vs-86 finding above.",
            "arithmetic": "",
            "converter_impact": "FORBID MPEG-1/MPEG-2 video in an MP4 container. (MPEG-1/2 video is only reachable via the MPEG-PS container path, which is a different demuxer.) Worse: objtype 96/97/106 on an AUDIO track would set an audio stream's codec ID to MPEG1/MPEG2 and open the IPU video decoder on it — never emit those object types at all."
          },
          {
            "claim": "Audio sample-entry 4CCs that produce a non-NULL initial codec ID are exactly 'mp4a' (0x6134706D -> AAC) and 'fLaC' (0x43614C66 -> FLAC), plus the WAVE numeric tags 0x0001/0x0050/0x0055/0x0160/0x0161/0x00FF/0x2000/0x2001/0x674F which do not occur as ISO-BMFF sample-entry types. 'ac-3', 'ec-3', 'alac', 'twos', 'sowt', 'lpcm', 'samr', '.mp3' all map to SMS_CodecID_NULL.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:57-70 `static SMS_CodecTag s_CodecAudioTags[] = { { SMS_CodecID_MP2, 0x00000050 }, { SMS_CodecID_MP3, 0x00000055 }, { SMS_CodecID_AC3, 0x00002000 }, { SMS_CodecID_DTS, 0x00002001 }, { SMS_CodecID_OGGV, 0x0000674F }, { SMS_CodecID_WMA_V1, 0x00000160 }, { SMS_CodecID_WMA_V2, 0x00000161 }, { SMS_CodecID_PCM16LE, 0x00000001 }, { SMS_CodecID_AAC, 0x6134706D }, { SMS_CodecID_AAC, 0x000000FF }, { SMS_CodecID_FLAC, 0x43614C66 }, { SMS_CodecID_NULL, 0x00000000 } };`",
            "arithmetic": "0x6134706D little-endian byte order = 6D 70 34 61 = 'm','p','4','a'. 0x43614C66 = 66 4C 61 43 = 'f','L','a','C'.",
            "converter_impact": "Audio in MP4 must be 'mp4a'. FORBID AC-3, E-AC-3, ALAC, and PCM ('twos'/'sowt'/'lpcm') inside MP4/MOV — they are accepted by the demuxer and then never open a decoder."
          },
          {
            "claim": "When an esds IS parsed (audio path only), m_ID is OVERWRITTEN unconditionally by _mp4_get_id(objectTypeIndication). An object type outside {32,64,96,97,102,103,105,106,107,221,226} wipes the 4CC-derived AAC id back to SMS_CodecID_NULL and the track never opens.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:386-391 `if ( lTag == MP4DecConfigDescrTag ) { int lObjTypeID = File_GetByte ( apFileCtx ); ... lpCodecCtx -> m_ID = _mp4_get_id ( lObjTypeID ); }` and src/SMS_ContainerMOV.c:359-368 `_mp4_get_id` returns `SMS_CodecID_NULL` when no row matches.",
            "arithmetic": "Notable holes in the accepted set: 104 (MPEG-2 AAC SSR), 108/109/110, 225 (QCELP), 227+ — all become NULL. 0x40 (64, MPEG-4 Audio) is the correct value for AAC.",
            "converter_impact": "Require objectTypeIndication == 0x40 (64) for AAC, or 0x69/0x6B (105/107) for MP3-in-MP4. Anything else must be forbidden."
          },
          {
            "claim": "_mov_read_esds skips a FIXED 3 bytes after an ES_Descr tag (ES_ID 2 + flags 1). If streamDependenceFlag, URL_Flag or OCRstreamFlag is set in that flags byte, the extra fields are not skipped, the next descriptor read is misaligned, MP4DecConfigDescrTag is not found, and neither the object type nor the extradata is picked up (the codec silently keeps its 4CC-derived id and no DecoderSpecificInfo).",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:382-386 `File_Skip ( apFileCtx, 4 ); lLen = _mp4_read_descr ( apFileCtx, &lTag ); File_Skip ( apFileCtx, lTag == MP4ESDescrTag ? 3 : 2 ); lLen = _mp4_read_descr ( apFileCtx, &lTag ); if ( lTag == MP4DecConfigDescrTag ) {`",
            "arithmetic": "ES_Descr body when all flags clear = ES_ID(2) + streamDependenceFlag/URL_Flag/OCRstreamFlag/streamPriority packed byte(1) = 3 bytes. Each set flag adds 2, 1+N, or 2 bytes respectively, none of which SMS consumes.",
            "converter_impact": "Require the ES_Descriptor flags byte to be 0 (no dependency, no URL, no OCR). Standard ffmpeg mp4 muxing satisfies this; a converter that hand-builds esds must not set those bits."
          },
          {
            "claim": "DecoderSpecificInfo (extradata) is only stored when its declared descriptor length is > 0 AND fits inside the esds atom's own declared payload; otherwise it is silently dropped and m_pUserData stays NULL. The allocation is rounded up to a 16-byte multiple.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:399-404 `if ( lTag == MP4DecSpecificDescrTag && lLen > 0 && _tbl_fits ( apAtom, lLen, 1, 0 )  ) { lpCodecCtx -> m_pUserData = ( uint8_t* )malloc (  ( lLen + 15 ) & ~15  ); if ( lpCodecCtx -> m_pUserData ) { apFileCtx -> Read ( apFileCtx, lpCodecCtx -> m_pUserData, lLen ); lpCodecCtx -> m_UserDataLen = lLen; } }` with src/SMS_ContainerMOV.c:136-139 `_tbl_fits` = `lAvail = apAtom -> m_Size - aConsumed; return lAvail >= 0 && ( int64_t )anEntries * aWidth <= lAvail;`",
            "arithmetic": "aConsumed = 0 and aWidth = 1, so the test reduces to lLen <= (declared esds box size - 8).",
            "converter_impact": "The esds box size must honestly cover its DecoderSpecificInfo. A truncated/lying esds silently yields a decoder with no AudioSpecificConfig."
          },
          {
            "claim": "SMS_CodecContext.m_UserDataLen is a signed 16-bit short, so extradata larger than 32767 bytes stores a negative length and the AAC init takes the wrong branch.",
            "kind": "HARD",
            "evidence": "include/SMS_Codec.h:81 `short         m_UserDataLen;` assigned at src/SMS_ContainerMOV.c:403 `lpCodecCtx -> m_UserDataLen = lLen;`; consumed at src/SMS_AAC.c:8262-8264 `if ( apCtx -> m_pUserData && apCtx -> m_UserDataLen > 0 ) retVal = _NeAACDecInit2 ( lpAAC, apCtx -> m_pUserData, apCtx -> m_UserDataLen, ... ); else retVal = _NeAACDecInit ( lpAAC, apCtx -> m_pUserData, abs ( apCtx -> m_UserDataLen ), ... );`",
            "arithmetic": "short range is -32768..32767. lLen = 40000 stores as 40000 - 65536 = -25536, which fails the `> 0` gate and then feeds abs() = 25536 to the wrong init entry point.",
            "converter_impact": "Clamp/forbid DecoderSpecificInfo > 32767 bytes. A real AAC AudioSpecificConfig is 2-5 bytes and an MPEG-4 VOL is tens of bytes, so this only bites on malformed input — but a converter that pads extradata must respect it."
          },
          {
            "claim": "moov may legally come after mdat and the file still plays, but when it does the ENTIRE mdat is read off the device byte-for-byte and thrown away: _mov_read_mdat calls File_Skip, which is a sequential 512-byte read loop, not a seek. With moov first, mdat is never read during header parse at all.",
            "kind": "SOFT",
            "evidence": "src/SMS_ContainerMOV.c:608-619 `_mov_read_mdat`: `... apCtx -> m_fMDAT = 1; apCtx -> m_MDATOffset = apAtom -> m_Offset; if ( apCtx -> m_fMOOV ) return 1; File_Skip (  apFileCtx, ( uint32_t )( apAtom -> m_Size )  );` and src/SMS_FileContext.c:1718-1744 `void File_Skip ( FileContext* apFileCtx, unsigned int aCount ) { unsigned int lnBlocks = aCount / 512; ... for ( i = 0; i < lnBlocks; ++i ) if ( apFileCtx -> Read ( apFileCtx, g_SMSounds, 512 ) != 512 ) return; ...` — the file's own comment says \"This is a read LOOP, not a seek\". The real seek only happens afterwards, at src/SMS_ContainerMOV.c:948 `if ( lpFileCtx -> m_CurPos != lpMOV -> m_MDATOffset ) lpFileCtx -> Seek ( lpFileCtx, ( uint32_t )( lpMOV -> m_MDATOffset ) );` (STIO_Seek is a genuine fioLseek, src/SMS_FileContext.c:1307-1325).",
            "arithmetic": "A 700 MB mdat = 700*1024*1024/512 = 1,433,600 File_Skip iterations, each a STIO_Read memcpy, backed by 700*1024*1024/4096 = 179,200 device reads (STIO m_BufSize = 4096, src/SMS_FileContext.c:1578 and :1656). Correctness is unaffected; only open time is.",
            "converter_impact": "Always run faststart (ffmpeg -movflags +faststart) so moov precedes mdat. Not a failure if omitted, but the file appears to hang for minutes on open over USB/MX4SIO/network."
          },
          {
            "claim": "Any atom whose declared size exceeds the bytes remaining in its parent aborts the parse loop immediately. At top level the parent size is the whole file size, so a truncated download stops parsing at the offending atom; if neither moov nor mdat was reached first, the container is rejected.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:768-769 `lAtom.m_Size -= 8; if ( lAtom.m_Size < 0 || lAtom.m_Size > apAtom -> m_Size - lTotalSize ) break;`; src/SMS_ContainerMOV.c:941 `MOVAtom lAtom = { 0, 0, lpFileCtx -> m_Size };`; src/SMS_ContainerMOV.c:946 `if (  retVal < 0 || ( !lpMOV -> m_fMOOV && !lpMOV -> m_fMDAT )  ) return 0;`",
            "arithmetic": "",
            "converter_impact": "Box sizes must be exact and the file must not be truncated. A converter that writes a placeholder mdat size and fails to patch it will produce a file SMS refuses."
          },
          {
            "claim": "Maximum 8 streams (traks). The 9th and later trak is skipped whole — not an error, just silently absent.",
            "kind": "HARD",
            "evidence": "include/SMS_Container.h:56 `#define SMS_MAX_STREAMS 8`; src/SMS_ContainerMOV.c:182-185 `if (  apCtx -> m_pBase -> m_nStm >= SMS_MAX_STREAMS  ) { File_Skip ( apFileCtx, ( uint32_t )apAtom -> m_Size ); return 0; }`",
            "arithmetic": "",
            "converter_impact": "Emit at most 8 traks; put the intended video and audio traks first. Ideally emit exactly 2 (1 video + 1 audio) and no chapter/timecode/metadata traks."
          },
          {
            "claim": "A single VIDEO sample (frame) larger than 2,097,088 bytes cannot be delivered: the ring-buffer allocation fails and _ReadPacket returns -1, ending the stream.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:54 `#define SMS_VP_BUFFER_SIZE ( 1024 * 1024 * 2 )` used at src/SMS_Player.c:1688-1694 `s_pVPBuff = SMS_SyncMalloc ( SMS_VP_BUFFER_SIZE ); lpStm -> m_pPktBuf = lpVideoBuffer = SMS_RingBufferInit ( ..., SMS_VP_BUFFER_SIZE );`; src/SMS_Container.c:53 `SMS_AVPacket* lpPkt = aSize ? SMS_RingBufferAlloc ( apRB, aSize + 64 ) : &s_EmptyPacket;`; src/SMS_RingBuffer.c:74-76 `aSize = ( aSize + 63 ) & 0xFFFFFFC0; if ( aSize <= apRB -> m_Capacity ) {` (retVal stays NULL otherwise); src/SMS_ContainerMOV.c:1066-1068 `lpPkt = apCont -> AllocPacket ( lpStm -> m_pPktBuf, lpSample -> m_Size ); if ( !lpPkt ) return -1;`",
            "arithmetic": "Request = roundup64(S + 64) must be <= 2,097,152. Since 2,097,152 is itself a multiple of 64, this is exactly S + 64 <= 2,097,152, i.e. S <= 2,097,088 bytes = 2 MiB - 64.",
            "converter_impact": "Clamp peak per-frame size below 2,097,088 bytes. Cap the encoder with -maxrate/-bufsize so no single I-frame can exceed ~2 MB; at 25 fps that is a 419 Mbit/s instantaneous ceiling, so it only bites on lossless/near-lossless intra encodes."
          },
          {
            "claim": "A single AUDIO packet larger than 524,224 bytes fails the same way. For tracks with a FIXED stsz sample size that are not video, the index entry covers a whole CHUNK, so it is the chunk — not the frame — that must fit.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:55 `#define SMS_AP_BUFFER_SIZE ( 1024 *  512     )` used at src/SMS_Player.c:1722-1723; the per-chunk index path is src/SMS_ContainerMOV.c:874-911, entered only when `!( apMyStm -> m_pSampleSize || apStm -> m_pCodec -> m_Type == SMS_CodecTypeVideo )` (src/SMS_ContainerMOV.c:834) and calling `_add_idx_entry ( apMyStm, 1, ( uint32_t )lCurDTS, ( uint32_t )lCurOffset, lChunkSize )` at :911.",
            "arithmetic": "roundup64(S + 64) <= 524,288 => S <= 524,224 bytes = 512 KiB - 64.",
            "converter_impact": "AAC in MP4 always has a variable stsz table so it takes the safe per-sample path. Avoid fixed-sample-size (CBR/PCM-style) audio traks with large chunks; keep interleave chunks well under 512 KiB."
          },
          {
            "claim": "Video wider or taller than 1024 pixels is refused with STR_RESOLUTION_TOO_BIG and playback is aborted. The dimensions come from the stsd VisualSampleEntry as unsigned 16-bit values.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:1676-1681 `if ( lpVideoCtx -> m_Width  > 1024 || lpVideoCtx -> m_Height > 1024 ) { s_Player.m_pErrorMsg = &STR_RESOLUTION_TOO_BIG; goto destroy; }`; source of the values: src/SMS_ContainerMOV.c:289-290 `lpCodecCtx -> m_Width  = File_GetShortBE ( apFileCtx ); lpCodecCtx -> m_Height = File_GetShortBE ( apFileCtx );`",
            "arithmetic": "Boundary is inclusive: 1024x1024 passes, 1025 in either axis fails. Note 1280x720 FAILS on width.",
            "converter_impact": "HARD CAP: both dimensions <= 1024. 720x480 / 720x576 / 640x480 are safe; 1280x720 and 1920x1080 are rejected outright."
          },
          {
            "claim": "All sample positions and sizes are stored as 32-bit; _ReadPacket bails if a sample position exceeds the file size. Combined with the 32-bit FileContext size field this caps usable files at 4 GiB, and the underlying size comes from an int-returning lseek.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:22-27 `typedef struct MOVIndex { uint32_t m_Flags; uint32_t m_DTS; uint32_t m_Pos; uint32_t m_Size; } MOVIndex;`; src/SMS_ContainerMOV.c:857 `_add_idx_entry (  apMyStm, lKeyFrame, ( uint32_t )lCurDTS, ( uint32_t )lCurOffset, lSampleSize  );`; src/SMS_ContainerMOV.c:1062 `if ( lpSample -> m_Pos > lpFileCtx -> m_Size ) return -1;`; include/SMS_FileContext.h:66-68 `unsigned int   m_Size; ... unsigned int   m_CurPos;`; src/SMS_FileContext.c:1652-1654 `retVal -> m_Size = lXio ? fileXioLseek ( lFD, 0, SEEK_END ) : fioLseek ( lFD, 0, SEEK_END );`",
            "arithmetic": "m_Pos is uint32_t, so any chunk offset >= 2^32 = 4,294,967,296 wraps. fioLseek returns int, so a size above 2^31 = 2,147,483,648 cannot be represented positively.",
            "converter_impact": "Keep output files under 2 GiB to stay clear of the signed-int lseek return; 4 GiB is the absolute wrap point. Split long content into parts."
          },
          {
            "claim": "The sample index is grown one entry at a time by SMS_Realloc whose return value is NOT checked — on allocation failure the code writes through a NULL pointer. This is the failure mode for a sample count that outruns the heap, and it is a crash, not a graceful reject.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:801-813 `static void _add_idx_entry (...) { MOVIndex* lpIdx, *lpIndices; lpIndices = ( MOVIndex* )SMS_Realloc ( apMyStm -> m_pIdx, ( unsigned int * )( &apMyStm -> m_IdxAllocSize ), ( apMyStm -> m_nIdx + 1 ) * sizeof ( MOVIndex ) ); apMyStm -> m_pIdx = lpIndices; lpIdx = &lpIndices[ apMyStm -> m_nIdx++ ]; lpIdx -> m_Flags = afKF; ...}` and src/SMS_EE.c:45-53 `void* SMS_Realloc ( void* apData, unsigned int* apSize, unsigned int aMinSize ) { if ( aMinSize < *apSize ) return apData; *apSize = 17 * aMinSize / 16 + 32; return realloc ( apData, *apSize ); }`",
            "arithmetic": "Permanent index cost per sample = sizeof(MOVIndex) = 16 bytes, over-allocated by 17/16 => ~17 bytes/sample. Peak during header parse additionally holds, per trak and simultaneously for ALL traks (they are freed only in the _mov_read_header loop at src/SMS_ContainerMOV.c:997-1001, after every trak has been parsed): stsz 4 B/sample, stco 8 B/chunk, stts 8 B/entry, stsc 12 B/entry, stss 4 B/keyframe. Worked example, 2 h of 30 fps video (216,000 samples, 1 sample per chunk) + 48 kHz AAC (337,500 frames): index 17*216,000 = 3.67 MB + 17*337,500 = 5.74 MB, plus temporary stsz 0.86 MB + 1.35 MB and stco 1.73 MB + ~1.7 MB => roughly 15 MB peak.",
            "converter_impact": "Keep total sample count modest. Prefer multi-sample chunks (fewer stco entries) and avoid tiny GOPs/very high frame rates on long content. A 2-hour 30 fps + AAC file is near the practical edge of PS2 RAM; verify before promising it."
          },
          {
            "claim": "ctts (composition offsets) must be non-negative. The first negative duration wipes m_nCtts to 0 while LEAVING m_pCtts allocated, and _ReadPacket gates only on the pointer — so every PTS becomes DTS plus entry 0 of a table that was never fully written. If entry 0 itself is the negative one, entry 0 is uninitialised malloc memory.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:571-582 `for ( i = 0; i < lnEntries; ++i ) { int lCount = _read_int_be; int lDuration = _read_int_be; if ( lDuration < 0 ) { lpMyStm -> m_nCtts = 0; File_Skip (  apFileCtx, 8 * ( lnEntries - i - 1 )  ); break; } ... }`; src/SMS_ContainerMOV.c:1081-1090 `if ( lpMyStm -> m_pCtts ) { lpPkt -> m_PTS = lpPkt ->m_DTS + lpMyStm -> m_pCtts[ lpMyStm -> m_Sample2CTimeIdx ].m_Duration / lpMyStm -> m_TimeRate; ++lpMyStm -> m_Sample2CTimeSample; if ( lpMyStm -> m_Sample2CTimeIdx < lpMyStm -> m_nCtts && ... ) {` — with m_nCtts == 0 the cursor never advances off entry 0.",
            "arithmetic": "m_pCtts is malloc'd for lnEntries*8 bytes at src/SMS_ContainerMOV.c:568 BEFORE the loop, so a break at i == 0 leaves all 8*lnEntries bytes uninitialised.",
            "converter_impact": "FORBID version-1 / negative composition offsets (ffmpeg: do not use -movflags +negative_cts_offsets; ensure the ctts is version 0 with unsigned offsets). Safest of all: encode with -bf 0 so no ctts is written at all."
          },
          {
            "claim": "A video trak MUST have a non-empty stts whose first entry has duration > 0, and an mdhd timescale > 0, or the codec frame rate stays 0/0 and a NaN is rounded into the COP0 Compare register that drives the frame timer.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:454-461 `if ( lpStm -> m_pCodec -> m_Type == SMS_CodecTypeVideo && lnEntries && lpMyStm -> m_pStts[ 0 ].m_Duration > 0 && lpMyStm -> m_TimeScale > 0 ) { lpStm -> m_pCodec -> m_FrameRate = lpMyStm -> m_TimeScale; lpStm -> m_pCodec -> m_FrameRateBase = lpMyStm -> m_pStts[ 0 ].m_Duration; }`; src/SMS_MPEG4.c:513 `lFrameRate = ( float )apCtx -> m_FrameRate / ( float )apCtx -> m_FrameRateBase;` and src/SMS_MPEG4.c:522-524 `__asm__ __volatile__( \"mtc0 %0, $11\\n\\t\" :: \"r\"(  ( uint32_t )(  ( 1000.0F / lFrameRate ) * 147456.0F + 0.5F )  ) );`",
            "arithmetic": "fps = mdhd timescale / stts[0].sample_delta. Compare = (1000/fps) * 147456. At 25 fps that is 40 * 147456 = 5,898,240 (fits uint32). It only overflows 2^32 below fps ~= 0.0343, i.e. a first stts delta more than ~29 s long. 0 fps gives 1000/0 = inf and an undefined cast.",
            "converter_impact": "Emit constant frame rate with a single stts entry whose delta matches the declared timescale (e.g. timescale 12800, delta 512 for 25 fps). Never emit an empty stts or a zero first delta on a video trak."
          },
          {
            "claim": "Only the FIRST sample entry in stsd is used; every later entry is skipped. The lock-out is on m_Tag, which is set even when the 4CC maps to no codec.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:281-286 `if ( lpCodecCtx -> m_Tag ) { File_Skip (  apFileCtx, lSize - ( apFileCtx -> m_CurPos - lStartPos )  ); continue; } lpCodecCtx -> m_Tag = lFmt; lpCodecCtx -> m_ID  = SMS_CodecGetID ( lpStm -> m_pCodec -> m_Type, lFmt );`",
            "arithmetic": "",
            "converter_impact": "Emit exactly one sample entry per trak. A file with a fallback/alternate second entry will never fall back."
          },
          {
            "claim": "The trak's media type comes from hdlr, and only 'vide', 'soun' and 'm1a ' are recognised. Any other handler leaves m_Type == SMS_CodecTypeUnknown (-1), in which case stsd parses neither branch and the codec lookup falls through to the AUDIO tag table.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:227-234 `if (  lSubType == SMS_MKTAG( 'v','i','d','e' )  ) lpStm ->m_pCodec -> m_Type = SMS_CodecTypeVideo; else if (  lSubType == SMS_MKTAG( 's','o','u','n' )  ) lpStm ->m_pCodec -> m_Type = SMS_CodecTypeAudio; else if (  lSubType == SMS_MKTAG( 'm','1','a',' ' )  ) { ... m_ID = SMS_CodecID_MP2; }`; the untyped fall-through is src/SMS_ContainerMOV.c:315 `} else File_Skip ( apFileCtx, apFileCtx -> m_CurPos - lStartPos );`; src/SMS_Codec.c:74 `SMS_CodecTag* lpTag = aType == SMS_CodecTypeVideo ? s_CodecVideoTags : s_CodecAudioTags;` (Unknown uses the audio table).",
            "arithmetic": "",
            "converter_impact": "Every emitted trak must carry hdlr handler_type 'vide' or 'soun'. Do not emit 'text'/'sbtl'/'tmcd'/'meta' traks — they consume one of the 8 stream slots and are parsed as pseudo-audio."
          },
          {
            "claim": "Audio packet duration falls back to a bitrate division when the stts is not a single entry, and m_BitRate for audio comes only from the esds avgBitrate field — so a multi-entry audio stts combined with a zero avgBitrate divides by zero.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:981-983 `if ( lpStm -> m_pCodec -> m_Type == SMS_CodecTypeAudio && lpMyStm -> m_nStts == 1 ) lpStm -> m_pCodec -> m_FrameSize = lpMyStm -> m_pStts[ 0 ].m_Duration;`; src/SMS_Container.c:193-194 `lNum = lpCodecCtx -> m_FrameSize <= 1 ? ( apPkt -> m_Size * 8 * lpCodecCtx -> m_SampleRate ) / lpCodecCtx -> m_BitRate : lpCodecCtx -> m_FrameSize;`; m_BitRate is set only at src/SMS_ContainerMOV.c:390 `lpCodecCtx -> m_BitRate = _read_int_be ( apFileCtx );` inside the DecConfigDescr branch.",
            "arithmetic": "",
            "converter_impact": "Give the audio trak a single stts entry (constant 1024-sample AAC frames) AND a non-zero avgBitrate in the esds DecoderConfigDescriptor. Both are ffmpeg defaults, but a hand-built esds must not leave avgBitrate at 0."
          },
          {
            "claim": "SMS_M4AProbe hands a caller-owned SMS_AudioInfo pointer to MOVContext.m_pMDAT, and _mov_read_header unconditionally free()s that field at the end. On the M3U path the SMS_AudioInfo is a stack local, so this is a free() of a stack address.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMOV.c:1319 `lMyCont.m_pMDAT     = ( MOVMDat* )apInfo;` then src/SMS_ContainerMOV.c:1326 `retVal = _mov_read_header ( &lCont ) >= 0 && ...`, and inside it src/SMS_ContainerMOV.c:1005 `_free ( lpMOV -> m_pMDAT );` (src/SMS_ContainerMOV.c:116-118 `static void _free ( void* apPtr ) { if ( apPtr ) free ( apPtr ); }`). The caller: src/SMS_ContainerM3U.c:128 `SMS_AudioInfo     lInfo;` passed at src/SMS_ContainerM3U.c:141 `if (  lpMyCont -> Probe[ i ] ( lpFileCtx, &lInfo )  ) {` with src/SMS_ContainerM3U.c:480 `lpCont -> Probe   [ SMS_SUBCONTAINER_M4A  ] = SMS_M4AProbe;`",
            "arithmetic": "",
            "converter_impact": "Not a file-format constraint — a latent crash on the .m3u playlist path when an entry is .m4a or .mp4. Advise against generating .m3u playlists that reference the converted files until this is fixed; test that path explicitly."
          }
        ],
        "gaps": [
          "Runtime confirmation of the 84-vs-86 VisualSampleEntry shortfall. My conclusion (esds is never dispatched for video sample entries) is a static byte-count derivation plus a trace of the _mov_read_dflt break condition. It directly contradicts commit 99b1b5f's assertion that \"The MOV demuxer already routes esds into the codec context's m_pUserData\". I did not run the code and there is no test asset in the repo. This single fact drives two of the largest converter decisions (whether MPEG-1/2-in-MP4 is possible at all, and whether the VOL header may be stripped into esds), so it should be verified on hardware or in an emulator before the converter is built on it.",
          "Absolute heap ceiling. Several comments in SMS_ContainerMOV.c say \"32 MB heap\" (e.g. src/SMS_ContainerMOV.c:427 `/* 32MB heap: a large ( or bogus ) entry count can fail */`), but that is prose, not code. I found no linker-script or Makefile constant defining the available heap, so I can give per-sample byte costs but cannot convert them into an absolute maximum sample count / maximum duration.",
          "Whether the SMS MPEG-4 Part 2 decoder actually tolerates a missing VOL header in practice. Commit 99b1b5f claims it produces dropped frames and garbled macroblocks; src/SMS_MPEG4.c:682-685 shows the seeding block is gated on m_pUserData, but I did not read _decode_vop_header to confirm the guessing behaviour.",
          "Behaviour of fioLseek / fileXioLseek for files above 2 GiB. src/SMS_FileContext.c:1652-1654 assigns an int return into an unsigned int m_Size; I could not determine from this repo whether the underlying ps2sdk call saturates, returns negative, or fails, so the exact large-file boundary (2 GiB vs 4 GiB) is unresolved.",
          "Performance/SOFT ceilings for frame rate, bitrate and resolution below the 1024 hard cap. Nothing in the container or codec source expresses a throughput budget, and I have no profiling data — I deliberately did not guess at 'safe' fps/bitrate numbers.",
          "Whether SMS_Codec_MPEG12_Open's IPU path can consume MP4-framed MPEG-2 elementary-stream samples (no PES layer). Moot given the finding that the esds object types are unreachable for video, but unverified.",
          "MOV/MP4 edge cases I did not chase: fragmented MP4 (moof/mfra) — there is no 'moof' entry in s_MOVParseTbl (src/SMS_ContainerMOV.c:700-724) so fragments would be skipped as unknown atoms and the file would present zero samples, but I did not trace the exact end state; and edit lists ('elst' is dispatched to _mov_read_dflt at src/SMS_ContainerMOV.c:708, i.e. treated as a container and effectively ignored, so any non-trivial edit list is silently discarded)."
        ]
      },
      {
        "area": "MPEG Program Stream container path (.mpg / .mpeg) — src/SMS_ContainerMPEG_PS.c and everything it directly reaches",
        "findings": [
          {
            "claim": "The demuxer accepts EXACTLY three top-level PES stream-id groups. Everything else (0x1B9 program_end, 0x1F0-0x1FF, 0x1BE padding, 0x1BF private_2, 0x1BC PSM, 0x1BB sys-hdr, 0x1BA pack) is either consumed as metadata or silently resynced past. Accepted: 0x000001C0-0x000001DF (audio), 0x000001E0-0x000001EF (video), 0x000001BD (private_stream_1).",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:167-171 — `if (   !(  ( lStartCode >= 0x000001C0 && lStartCode <= 0x000001DF  ) || ( lStartCode >= 0x000001E0 && lStartCode <= 0x000001EF  ) || ( lStartCode == 0x000001BD                              ) )\n ) goto redo;`",
            "arithmetic": "",
            "converter_impact": "Emit video on 0x1E0 and audio on 0x1C0 (or a private_stream_1 substream). Never use stream ids outside these three ranges — packets on any other id are discarded, not just ignored."
          },
          {
            "claim": "Video stream-id -> codec mapping when there is no Program Stream Map: 0x1E0-0x1EF maps unconditionally to SMS_CodecID_MPEG2 with tag 'mpg2', and ONLY the first such stream id encountered ever becomes a stream (the lfVideo latch). MPEG-1 video is not distinguished here, but that is harmless because SMS_CodecOpen routes SMS_CodecID_MPEG1 and SMS_CodecID_MPEG2 to the same SMS_Codec_MPEG12_Open.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:697-702 — `} else if ( lStartCode >= 0x1E0 && lStartCode <= 0x1EF && !lfVideo ) {\n\n   lCodecType = SMS_CodecTypeVideo;\n   lID        = SMS_CodecID_MPEG2;\n   lTag       = SMS_MKTAG( 'm', 'p', 'g', '2' );`; src/SMS_Codec.c:98-99 — `case SMS_CodecID_MPEG1    :\n  case SMS_CodecID_MPEG2    : SMS_Codec_MPEG12_Open  ( apCtx ); break;`",
            "arithmetic": "",
            "converter_impact": "Emit exactly ONE video stream, on 0x1E0. A second video PID is parsed but never becomes a playable stream. MPEG-1 or MPEG-2 video both work; no separate handling needed."
          },
          {
            "claim": "Audio stream-id -> codec mapping without a PSM, complete and exhaustive: 0x1C0-0x1DF -> SMS_CodecID_MP3; private_stream_1 substream 0x80-0x87 -> SMS_CodecID_AC3; 0x88-0x8F and 0x98-0x9F -> SMS_CodecID_DTS; 0xA0-0xAF -> SMS_CodecID_PCM16BE. Substream ids 0x90-0x97, 0xB0-0xBF, and the DVD subpicture range 0x20-0x3F fall through with lCodecType == SMS_CodecTypeUnknown and are ignored entirely.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:703-725 — `} else if ( lStartCode >= 0x1C0 && lStartCode <= 0x1DF ) {\n   lCodecType = SMS_CodecTypeAudio;\n   lID        = SMS_CodecID_MP3;\n  } else if ( lStartCode >= 0x080 && lStartCode <= 0x087 ) {\n   lCodecType = SMS_CodecTypeAudio;\n   lID        = SMS_CodecID_AC3;\n  } else if (  ( lStartCode >= 0x088 && lStartCode <= 0x08F ) ||\n               ( lStartCode >= 0x098 && lStartCode <= 0x09F )\n  ) {\n   lCodecType = SMS_CodecTypeAudio;\n   lID        = SMS_CodecID_DTS;\n  } else if ( lStartCode >= 0xA0 && lStartCode <= 0xAF ) {\n   lCodecType = SMS_CodecTypeAudio;\n   lID        = SMS_CodecID_PCM16BE;\n  }`",
            "arithmetic": "",
            "converter_impact": "MP2/MP3 -> 0x1C0. AC3 -> private_stream_1 substream 0x80. DTS -> 0x88. LPCM -> 0xA0. Do NOT use 0x90-0x97 or 0xB0-0xBF for anything you want played."
          },
          {
            "claim": "MP3, AC3, DTS and LPCM are all GENUINELY handled end-to-end — each has a real decoder wired in SMS_CodecOpen, and each has a real parameter probe in the PS demuxer. There is no stub. Note that SMS_CodecID_MP3 is used for both Layer II and Layer III (the layer is read from the frame header by the decoder), and SMS_CodecID_MP2 is never produced by this demuxer.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:352-432 (switch on m_ID with real cases for SMS_CodecID_MP2/MP3 via MP123_DecodeHeader, SMS_CodecID_AC3 via AC3_SyncInfo, SMS_CodecID_DTS via DTS_SyncInfo, SMS_CodecID_PCM16BE); src/SMS_Codec.c:103-113 — `case SMS_CodecID_MP3    : apCtx -> m_Tag = SMS_MKTAG( 'm', 'p', '3', ' ' );\n       SMS_CodecID_MPA    :                                                   SMS_Codec_MP123_Open ( apCtx ); break;\n  case SMS_CodecID_AC3    : ... SMS_Codec_AC3_Open   ( apCtx ); break;\n  case SMS_CodecID_DTS    : ... SMS_Codec_DTS_Open   ( apCtx ); break;\n  case SMS_CodecID_PCM16BE:\n  case SMS_CodecID_PCM16LE: ... SMS_Codec_PCM_Open   ( apCtx ); break;`",
            "arithmetic": "",
            "converter_impact": "All four audio codecs are safe targets. MPEG-1 Layer II on 0x1C0 is the least risky default; AC3 on 0x80 is fully supported."
          },
          {
            "claim": "A Program Stream Map (0x1BC) IS parsed, and it OVERRIDES the stream-id heuristics — but it recognises only five stream_types: 0x01 (MPEG1 video), 0x02 (MPEG2 video), 0x03/0x04 (MPEG audio -> MP3), 0x81 (AC3). There is no PSM stream_type for DTS (0x8A) or LPCM (0x80). A PSM that declares a type outside this set makes the stream UNPLAYABLE, because the PSM branch is taken instead of the stream-id fallback and the switch falls through leaving lCodecType == SMS_CodecTypeUnknown.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:664-695 — `lType = lpMyCont -> m_Type[ lStartCode & 0xFF ];\n\n  if ( lType > 0 ) {\n\n   switch ( lType ) {\n\n    case STREAM_TYPE_VIDEO_MPEG1: ... case STREAM_TYPE_VIDEO_MPEG2: ... case STREAM_TYPE_AUDIO_MPEG1:\n    case STREAM_TYPE_AUDIO_MPEG2: ... case STREAM_TYPE_AUDIO_AC3: ...\n\n   }  /* end switch */\n\n  } else if ( lStartCode >= 0x1E0 ...` with src/SMS_ContainerMPEG_PS.c:35-39 — `#define STREAM_TYPE_VIDEO_MPEG1 0x01 ... #define STREAM_TYPE_AUDIO_AC3   0x81`",
            "arithmetic": "",
            "converter_impact": "SAFEST: emit NO program_stream_map at all — the stream-id heuristics cover more codecs (DTS, LPCM) than the PSM path does. If you must emit a PSM, restrict it to types 0x01/0x02/0x03/0x04/0x81 only."
          },
          {
            "claim": "If a PSM declares a type for elementary_stream_id 0xBD, the private_stream_1 substream-id byte and the 3-byte DVD private header are NOT stripped, and the raw start code stays 0x1BD (445 decimal) which matches none of the stream-id ranges — so the stream is dropped. The substream extraction is gated on m_Type[0xBD] being zero.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:256-274 — `if ( lStartCode == PRIVATE_STREAM_1 && !lpMyCont ->m_Type[ lStartCode & 0xFF ] ) {\n\n  if ( lLen < 1 ) goto redo;\n\n  lStartCode = File_GetByte ( lpFileCtx );\n  --lLen;\n\n  if ( lStartCode >= 0x80 && lStartCode <= 0xBF ) {\n\n   if ( lLen < 3 ) goto redo;\n\n   File_GetByte ( lpFileCtx );\n   File_GetByte ( lpFileCtx );\n   File_GetByte ( lpFileCtx );\n   lLen -= 3;\n\n  }  /* end if */\n\n }  /* end if */`",
            "arithmetic": "0x1BD = 445; accepted audio range starts at 0x1C0 = 448; 445 < 448 so no range matches.",
            "converter_impact": "Never declare stream id 0xBD in a program_stream_map. AC3/DTS/LPCM must be reached through the substream-id path with m_Type[0xBD] == 0."
          },
          {
            "claim": "The DVD private_stream_1 header layout is hard-coded as 4 bytes (substream id + 3) for AC3/DTS, and 7 bytes (4 + 3 more) for LPCM. The extra 3 LPCM bytes are skipped per packet at read time, and ONLY for SMS_CodecID_PCM16BE.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:519-522 — `if ( lpStm -> m_pCodec -> m_ID == SMS_CodecID_PCM16BE ) {\n     File_Skip ( lpFileCtx, 3 );\n     lLen -= 3;\n    }  /* end if */`",
            "arithmetic": "AC3/DTS: 1 (substream id) + 3 (frame count + first-access-unit pointer) = 4 bytes consumed in _read_header. LPCM: 4 + 3 = 7 bytes.",
            "converter_impact": "Emit the standard DVD private_stream_1 header: 4 bytes for AC3/DTS, 7 bytes for LPCM. A non-standard header length shifts every payload byte and will desync the decoder."
          },
          {
            "claim": "LPCM is decoded as 16-bit big-endian ONLY. The quantization-word-length field (bits 7-6 of the second LPCM header byte) is read but never inspected; the decoder is an unconditional 16-bit byte-swap. Sample rate comes from a 4-entry table {48000, 96000, 44100, 32000} and channel count is 1 + (byte & 7).",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:411-424 — `static const int sl_PCMFreq[ 4 ] ... = {\n     48000, 96000, 44100, 32000\n    };\n\n    int lVal, lFreq;\n\n    if ( aLen > 2 ) {\n\n     lVal  = lpPtr[ 1 ];\n     lFreq = ( lVal >> 4 ) & 3;\n\n     apStm -> m_SampleRate           = apStm -> m_pCodec -> m_SampleRate = sl_PCMFreq[ lFreq ];\n     apStm -> m_pCodec -> m_Channels = 1 + ( lVal & 7 );`; src/SMS_PCM.c:35-36 — `else if ( apCtx -> m_ID == SMS_CodecID_PCM16BE )\n  _Decode = memcpy_swap16;`",
            "arithmetic": "Channels = 1 + (b & 7) so 1..8 channels are representable; sample rate index is 2 bits so exactly 4 values.",
            "converter_impact": "If emitting LPCM, use 16-bit samples only. 20-bit and 24-bit LPCM will be byte-swapped as if 16-bit and play as noise. Prefer 48000 Hz."
          },
          {
            "claim": "Free-format MPEG audio (bitrate_index == 0) is rejected at probe time, because the probe zeroes m_FreeFmtFrameSize and MP123_DecodeHeader returns failure in that case.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:344 — ` lInfo.m_FreeFmtFrameSize = 0;` together with src/SMS_MP123.c:5337-5339 — `} else {\n\n  if ( !apInfo -> m_FreeFmtFrameSize ) return 1;` (the caller at src/SMS_ContainerMPEG_PS.c:359-360 requires `!MP123_DecodeHeader ( ... )`)",
            "arithmetic": "",
            "converter_impact": "Never emit free-format MPEG audio. Use a standard bitrate index (any of the 14 valid values)."
          },
          {
            "claim": "A .mpg is REJECTED OUTRIGHT unless BOTH a video stream with a parsed sequence header AND an audio stream with parsed parameters are found. Video-only and audio-only MPEG program streams are refused by this demuxer.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:774-782 — `if ( !lfVideoParam || !lfAudio ) {\n\n   apCont -> m_pCtx = NULL;\n\n   SMS_DestroyContainer ( apCont, 0 );\n\n   retVal = 0;\n   goto error;\n\n  }  /* end if */`",
            "arithmetic": "",
            "converter_impact": "ALWAYS mux an audio track, even a silent one, and always a video track. A video-only .mpg will be reported as an unsupported file."
          },
          {
            "claim": "lfAudio is ASSIGNED (not OR-ed) from each audio stream's parameter probe, so the LAST audio stream discovered decides whether the whole file is accepted. A file with two audio streams where the second one's parameters cannot be parsed is rejected even though the first one was fine.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:752 — `} else lfAudio = _fill_audio_parameters ( lpFileCtx, lpStm, lLen );`",
            "arithmetic": "",
            "converter_impact": "Emit exactly ONE audio stream. Multiple audio tracks turn acceptance into a coin flip that depends on which stream the probe sees last."
          },
          {
            "claim": "An MPEG sequence_header_code (00 00 01 B3) must be discoverable from the first video PES packet, otherwise lfVideoParam stays 0 and the file is rejected. Width, height and the widescreen flag are read as the 12/12/4 bits immediately following that start code.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:293-304 — `if (  _next_start_code ( lpFileCtx ) == 0x000001B3  ) {\n\n   char*          lpBuffer = ( char* )malloc ( aLen );\n   SMS_BitContext lBitCtx;\n\n   lpFileCtx -> Read ( lpFileCtx, lpBuffer, aLen );\n   SMS_InitGetBits ( &lBitCtx, ( unsigned char* )lpBuffer, aLen );\n\n   apStm -> m_pCodec -> m_Width  = SMS_GetBits ( &lBitCtx, 12 );\n   apStm -> m_pCodec -> m_Height = SMS_GetBits ( &lBitCtx, 12 );\n\n   if (  SMS_GetBits ( &lBitCtx, 4 ) == 3  ) apStm -> m_pCodec -> m_fWS = 1;`",
            "arithmetic": "12 + 12 + 4 = 28 bits, read from a 4-byte aligned cache, so the buffer must hold at least 4 bytes past the start code.",
            "converter_impact": "Put a sequence header at the very start of the video elementary stream, inside the first video PES packet. Repeat it at every GOP (standard ffmpeg behaviour is fine)."
          },
          {
            "claim": "The sequence-header search gives up after 1024 PES headers, and each individual start-code scan gives up after 100000 bytes.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:318 — `if (  i++ == 1024 || FILE_EOF( lpFileCtx )  ) goto end;`; src/SMS_ContainerMPEG_PS.c:25 — `#define MAX_SYNC_SIZE   100000` used at :74 — ` lN     = MAX_SYNC_SIZE;` inside `_next_start_code`",
            "arithmetic": "MAX_SYNC_SIZE = 100000 bytes of raw scan per _next_start_code call; loop bound 1024 PES headers.",
            "converter_impact": "Never leave more than ~100 KB of the file without a 00 00 01 start code. Do not front-load the file with a huge padding stream or a giant private_stream_2 blob."
          },
          {
            "claim": "Stream detection stops at the first of: 1024 PES headers examined, EOF, or 1 MiB of file consumed. Any stream that first appears after that window is invisible to SMS.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:650 — ` while (  i++ < 1024 && !FILE_EOF( lpFileCtx )  ) {`; src/SMS_ContainerMPEG_PS.c:26 — `#define MAX_DETECT_SIZE 1024 * 1024`; src/SMS_ContainerMPEG_PS.c:762-764 — `if (  apCont -> m_nStm < SMS_MAX_STREAMS && ( !lfAudio || !lfVideoParam )  )\n   continue;\n  else if ( lpFileCtx -> m_CurPos >= MAX_DETECT_SIZE ) break;`",
            "arithmetic": "MAX_DETECT_SIZE = 1024 * 1024 = 1048576 bytes.",
            "converter_impact": "Interleave every stream from the very beginning of the file. Every stream must have at least one PES packet inside the first 1 MiB."
          },
          {
            "claim": "Maximum 7 recognized elementary streams. SMS_MAX_STREAMS is 8, m_pStm is a fixed 8-pointer array with int64_t m_Duration immediately after it, the write at :743 is UNGUARDED (the check at :762 only controls `continue`, and control falls through to the loop bottom anyway when it is false), and four separate loops walk m_pStm looking for a NULL terminator that does not exist at 8 streams. With exactly 8 streams, m_pStm[8] aliases the low word of m_Duration, which is set to 0x7FFFFFFFFFFFFFFF at :788 — so the terminator scan at :559 dereferences 0xFFFFFFFF.",
            "kind": "HARD",
            "evidence": "include/SMS_Container.h:56,79-80 — `#define SMS_MAX_STREAMS 8` / `SMS_Stream* m_pStm[ SMS_MAX_STREAMS ];\n int64_t m_Duration;`; src/SMS_ContainerMPEG_PS.c:743 — `apCont -> m_pStm[ apCont -> m_nStm++ ] = lpStm;`; src/SMS_ContainerMPEG_PS.c:559 — `for (  lIt = lppStm; *lIt; ++lIt  ) if (  ( int )( *lIt ) -> m_ID == lStartCode  ) break;`; src/SMS_ContainerMPEG_PS.c:788 — `apCont -> m_Duration = 0x7FFFFFFFFFFFFFFFLL;`",
            "arithmetic": "m_pStm occupies bytes 0..31 (8 pointers x 4 bytes on EE). m_Duration is at offset 32. &m_pStm[8] == offset 32 == &m_Duration. Little-endian low word of 0x7FFFFFFFFFFFFFFF is 0xFFFFFFFF. Safe stream count = 7, because m_pStm[7] must remain NULL as the terminator.",
            "converter_impact": "FORBID more than 7 recognized elementary streams; clamp hard at 2 (one video + one audio). Also note deduplication is by stream id only — every distinct id in the first 1 MiB counts, including ones on the private_stream_1 substream ranges."
          },
          {
            "claim": "Max PES packet size is 65535 bytes, imposed by the 16-bit PES_packet_length field. There is no larger fixed buffer that a legal PES can overflow: video packets are allocated at exactly (payload + carry) rounded down to 16 and go into a 2 MiB ring, audio packets go into a 512 KiB ring. So an 'oversized packet' failure mode does not exist for a spec-legal program stream.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:173 — ` lLen = File_GetShortBE ( lpFileCtx );`; src/SMS_Container.c:53 — `SMS_AVPacket* lpPkt = aSize ? SMS_RingBufferAlloc ( apRB, aSize + 64 ) : &s_EmptyPacket;`; src/SMS_RingBuffer.c:74-76 — `aSize = ( aSize + 63 ) & 0xFFFFFFC0;\n\n if ( aSize <= apRB -> m_Capacity ) {`; src/SMS_Player.c:54-55 — `#define SMS_VP_BUFFER_SIZE ( 1024 * 1024 * 2 )\n#define SMS_AP_BUFFER_SIZE ( 1024 *  512     )`",
            "arithmetic": "PES_packet_length is 16 bits -> max 65535. Video ring ceiling = 2097152 - 64 = 2097088 bytes; audio ring ceiling = 524288 - 64 = 524224. 65535 + 15 (carry) + 64 = 65614, which is 3.1% of the audio ring and 3.1% of... precisely 65614/524288 = 12.5% of the audio ring and 3.1% of the video ring. Ample headroom.",
            "converter_impact": "Use the normal 2048-byte pack / ~2034-byte PES payload. Any legal PES length is safe. No clamping needed beyond the 65535 the field already enforces."
          },
          {
            "claim": "A PES packet whose remaining payload length reaches 0 during header parsing is DISCARDED (goto redo), and during the detection pass a zero return from _read_header ABORTS detection entirely.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:177-186 — `while ( 1 ) {\n\n  if ( lLen < 1 ) goto redo;\n\n  lChr = File_GetByte ( lpFileCtx );\n  --lLen;\n\n  if ( lChr != 0xFF ) break;\n\n }`; src/SMS_ContainerMPEG_PS.c:656-658 — `lLen = _read_header ( apCont, &lStartCode, &lPTS, &lDTS );\n\n  if ( !lLen ) break;`",
            "arithmetic": "",
            "converter_impact": "Never emit a PES packet with zero payload bytes after the header, and never emit PES_packet_length == 0 (unbounded). A single such packet in the probe window silently truncates stream detection."
          },
          {
            "claim": "PES_scrambling_control must be 00. Any nonzero value in bits 5-4 of the MPEG-2 PES flags byte causes the packet to be dropped and the parser to resync.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:214-216 — `} else if (  ( lChr & 0xC0 ) == 0x80  ) {\n\n  if ( lChr & 0x30 ) goto redo;`",
            "arithmetic": "0x30 = bits 5 and 4 = PES_scrambling_control in the '10'-prefixed MPEG-2 PES header byte.",
            "converter_impact": "Emit unscrambled PES only (this is the ffmpeg default). Reject any input that is CSS/scrambled rather than passing it through."
          },
          {
            "claim": "PES_header_data_length must not exceed the remaining PES payload length, or the packet is dropped.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:218-222 — `lFlags = File_GetByte ( lpFileCtx );\n  lHLen  = File_GetByte ( lpFileCtx );\n  lLen  -= 2;\n\n  if ( lHLen > lLen ) goto redo;`",
            "arithmetic": "",
            "converter_impact": "Standard muxers satisfy this. Validate it if you hand-build PES headers, especially when adding stuffing bytes inside the optional header."
          },
          {
            "claim": "BOTH MPEG-1 System Streams and MPEG-2 Program Streams are handled. All four MPEG-1 PES header forms are decoded: the '01' STD-buffer prefix, the '0010' PTS-only form, the '0011' PTS+DTS form, and the 0x0F no-timestamp marker — alongside the MPEG-2 '10' form. Pack headers and system headers of either version are skipped by pure start-code resync, with no version-specific field parsing at all.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:188-254 — `if (  ( lChr & 0xC0 ) == 0x40  ) { ... }\n\n if (  ( lChr & 0xF0 ) == 0x20  ) { ... lDTS  = lPTS = _get_pts ( lpFileCtx, lChr ); ... } else if (  ( lChr & 0xF0 ) == 0x30  ) { ... } else if (  ( lChr & 0xC0 ) == 0x80  ) { ... } else if ( lChr != 0xF ) goto redo;`; src/SMS_ContainerMPEG_PS.c:149 — `if ( lStartCode == PACK_START_CODE || lStartCode == SYSHDR_START_CODE ) goto redo;`",
            "arithmetic": "",
            "converter_impact": "Either `-f mpeg` (MPEG-1 System Stream) or `-f vob`/MPEG-2 PS will be parsed. Neither is preferred by the container layer; choose based on the video codec you are emitting."
          },
          {
            "claim": "There is no limit on system header or Program Stream Map size or content, and no maximum number of PSM elementary-stream entries. The system header is not parsed at all (it is skipped by resync, not by its length field). The PSM ES-map loop is bounded only by the 16-bit map length, and its writes are index-safe because the target array is 256 entries indexed by an unsigned char.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:45 — ` unsigned char m_Type[ 256 ];`; src/SMS_ContainerMPEG_PS.c:113-126 — `lMLen = File_GetShortBE ( apFileCtx );\n\n while ( lMLen >= 4 ) {\n\n  unsigned char lType = File_GetByte    ( apFileCtx );\n  unsigned char lID   = File_GetByte    ( apFileCtx );\n  uint16_t      lILen = File_GetShortBE ( apFileCtx );\n\n  apCont -> m_Type[ lID ] = lType;\n\n  File_Skip ( apFileCtx, lILen );\n  lMLen -= 4 + lILen;\n\n }`",
            "arithmetic": "elementary_stream_map_length is 16 bits -> max 65535; each entry is at least 4 bytes -> at most 16383 entries. lID is unsigned char -> index 0..255, always inside m_Type[256]. lMLen is int, so the -= underflow goes negative and the `>= 4` test terminates the loop.",
            "converter_impact": "No clamping needed on system-header or PSM size. But see the separate finding: emitting a PSM is a net negative because it narrows codec support."
          },
          {
            "claim": "_fill_audio_parameters declares a variable-length array sized by the untrusted PES payload length, on the calling thread's stack — up to ~65529 bytes.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:336-346 — `static int _fill_audio_parameters ( FileContext* apFileCtx, SMS_Stream* apStm, int aLen ) {\n\n int         retVal = 0;\n int         lPos   = apFileCtx -> m_Pos;\n char        lBuf[ aLen ];\n ... apFileCtx -> Read ( apFileCtx, lBuf, aLen );`",
            "arithmetic": "aLen = PES_packet_length (max 65535) minus consumed header bytes. Worst case with a minimal MPEG-1 header (1 byte, 0x0F): 65535 - 1 = 65534. With the private_stream_1 4-byte header: 65535 - 1 - 4 = 65530. So the VLA can reach ~65.5 KB of stack.",
            "converter_impact": "Keep audio PES payloads at the conventional ~2 KB (2048-byte pack). Do not emit multi-kilobyte audio PES packets; a 64 KB audio PES puts a 64 KB VLA on the probe thread's stack."
          },
          {
            "claim": "An audio PES packet whose payload is 1, 2 or 3 bytes causes an INFINITE LOOP in _fill_audio_parameters: aLen is decremented below zero and the `while ( aLen ... )` test never becomes false, so the scan runs off the end of the stack buffer forever.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:348-350 and :438-439 — ` aLen -= 4;\n\n while ( aLen && !retVal ) {` ... `  ++lpPtr;\n  --aLen;\n\n }  /* end while */`",
            "arithmetic": "payload 1 -> aLen = -3; payload 2 -> -2; payload 3 -> -1. Each iteration does --aLen, so aLen goes -4, -5, ... and never equals 0. Payload 0 is caught earlier by `if ( !lLen ) break;` at :658; payload 4 gives aLen == 0 and the loop is skipped (probe fails cleanly, returning 0).",
            "converter_impact": "Enforce a MINIMUM audio PES payload of 5 bytes on the FIRST packet of each audio stream (in practice, require >= 5 on all of them). A 1-3 byte audio PES hangs SMS during file probing — the browser locks up, not just the player."
          },
          {
            "claim": "The audio codec parameters must be discoverable inside the FIRST PES packet of the audio stream. _fill_audio_parameters reads exactly one payload, scans it byte-by-byte for a frame sync, and returns; it never advances to the next packet. Failure here sets lfAudio = 0 and rejects the whole file.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:346-441 — a single `apFileCtx -> Read ( apFileCtx, lBuf, aLen );` followed by `while ( aLen && !retVal ) { switch ( apStm -> m_pCodec -> m_ID ) { ... } ++lpPtr; --aLen; }`, then `end:\n apFileCtx -> Seek ( apFileCtx, lPos );`",
            "arithmetic": "Scan positions tried: 0 .. (aLen-4-1). For a 2034-byte payload that is 2030 candidate offsets — enough to find any frame sync in a typical packet.",
            "converter_impact": "Start each audio stream on a frame boundary: the first audio PES payload must begin with (or contain) a complete MP3/MP2 header, an AC3 0x0B77 syncword, a DTS syncword, or an LPCM header. Use data_alignment_indicator and start the stream at a frame start."
          },
          {
            "claim": "AC3 detection additionally requires bsid < 12 and frmsizecod < 38, so E-AC3 (bsid 16) and any out-of-range frame-size code fail the probe.",
            "kind": "HARD",
            "evidence": "src/SMS_AC3.c:390 — `if ( apBuf[ 0 ] != 0x0B || apBuf[ 1 ] != 0x77 || apBuf[ 5 ] >= 0x60 ) return 0;` and src/SMS_AC3.c:399 — `if (  ( lFrmSizeCod = apBuf[ 4 ] & 63 ) >= 38  ) return 0;`",
            "arithmetic": "apBuf[5] >= 0x60 means bsid = apBuf[5] >> 3 >= 12, so bsid must be <= 11. frmsizecod < 38 -> bitrate index frmsizecod>>1 <= 18 -> max 640 kbps.",
            "converter_impact": "Emit legacy AC3 (bsid 8) only, at <= 640 kbps. Reject or transcode E-AC3 / AC3+ input."
          },
          {
            "claim": "The player refuses any video wider or taller than 1024 pixels, using the width/height the PS demuxer read from the sequence header.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:1676-1681 — `if ( lpVideoCtx -> m_Width  > 1024 ||\n             lpVideoCtx -> m_Height > 1024\n        ) {\n         s_Player.m_pErrorMsg = &STR_RESOLUTION_TOO_BIG;\n         goto destroy;\n        }`",
            "arithmetic": "Sequence-header horizontal_size_value and vertical_size_value are 12 bits each, so the format can express 0..4095; the player clamps acceptance at 1024.",
            "converter_impact": "Clamp output resolution to <= 1024x1024. Practically, target 720x480 / 720x576 or below."
          },
          {
            "claim": "The seek keyframe test for a picture start code reads a NATIVE little-endian 32-bit word and then extracts bits as if it were big-endian, so the picture_coding_type check inspects the wrong bits. The GOP (0x1B8) and sequence-header (0x1B3) tests in the same condition are byte-order independent and DO work.",
            "kind": "SOFT",
            "evidence": "src/SMS_ContainerMPEG_PS.c:609-614 — `if ( lCode == 0x000001B8 || lCode == 0x000001B3 || (\n          lCode == 0x00000100 && (\n           (  File_GetUInt ( lpFileCtx ) >> 19  ) & 3\n          ) == 1\n         )`; include/SMS_FileContext.h:110 — `retVal = SMS_unaligned32 ( apCtx -> m_pPos );`; include/SMS.h:156-158 — `static inline uint32_t SMS_unaligned32 ( const void* apData ) {\n return (  ( const SMS_Unaligned32* )apData  ) -> m_Val;` (a plain native load; the codebase byte-swaps explicitly elsewhere via SMS_bswap32, e.g. src/SMS_ContainerMPEG_PS.c:357)",
            "arithmetic": "Bitstream order after the picture start code: temporal_reference (10 bits) then picture_coding_type (3 bits). In a big-endian u32 those type bits are at positions 21..19, so (val >> 19) & 7 is correct. A little-endian load reverses the bytes, so (val >> 19) & 3 samples bits of byte 2 instead of byte 1 — unrelated data.",
            "converter_impact": "Emit a group_of_pictures_header (00 00 01 B8) immediately before EVERY I-frame, and keep GOPs short (about 0.5 s / 12-15 frames). That makes seeks land on the GOP test, which is correct, instead of relying on the broken picture-type test. Closed GOPs are strongly preferred."
          },
          {
            "claim": "Duration and start time are derived purely from PTS values: the first PTS of each stream from the head of the file, and the largest PTS found in the LAST 131071 bytes. There is no index and no scan of the whole file.",
            "kind": "SOFT",
            "evidence": "src/SMS_ContainerMPEG_PS.c:807-810 — `lpFileCtx -> Seek (\n    lpFileCtx, lpFileCtx -> m_Size > 0x0001FFFF ?\n               lpFileCtx -> m_Size - 0x0001FFFF : 0\n   );`; src/SMS_ContainerMPEG_PS.c:825-826 — `apCont -> m_StartTime = ( int )SMS_Rescale ( lPTS, SMS_TIME_BASE, 90000 );\n   apCont -> m_Duration  = ( int )SMS_Rescale ( lDTS, SMS_TIME_BASE, 90000 );`",
            "arithmetic": "0x0001FFFF = 131071 bytes. SMS_TIME_BASE = 1000 (include/SMS.h:183), so duration is in milliseconds: ms = pts_90k * 1000 / 90000. int32 max 2147483647 ms is ~24.8 days, so no practical duration ceiling. Seek position is computed as file_size * aPos / (Duration - StartTime) at :586-588.",
            "converter_impact": "Write PTS on essentially every PES packet, and make sure the final 128 KB of the file contains PTS for every stream. Do NOT let the PTS wrap (33 bits at 90 kHz = 26.5 h) and do not restart the PTS clock mid-file — both make the duration and the whole seek bar wrong. Constant bitrate helps, because seek is a linear byte-offset estimate."
          },
          {
            "claim": "There is NO signature or magic-number check. SMS_GetContainerMPEG_PS begins parsing immediately; a pack_start_code at offset 0 is not required. The .mpg / .mpeg extension only reorders the probe list, it does not gate the demuxer.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:639-656 — the function goes straight from `if (  ( int )apCont -> m_pFileCtx <= 0  ) return retVal;` to the detection loop calling `_read_header`, with no magic check; src/SMS_FileDir.c:132-133 — `else if (  !stricmp ( lpExt, s_pMPG  )  )\n   retVal = SMS_CONTAINER_MPEG_PS;` and :169-170 — `else if (  !stricmp ( lpExt, s_pMPEG )  )\n    retVal = SMS_CONTAINER_MPEG_PS;`",
            "arithmetic": "",
            "converter_impact": "Name output .mpg or .mpeg (case-insensitive) so the PS demuxer is probed first. Acceptance depends entirely on content, not on a header signature — so a well-formed pack header at offset 0 is good practice but not load-bearing."
          },
          {
            "claim": "Video packets are always delivered to the decoder in multiples of 16 bytes, with the remainder (0-15 bytes) carried across packets in a 15-byte context buffer and flushed at EOF. The arithmetic is provably safe — the carry can never exceed 15.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:46 — ` unsigned char m_Buff[  15 ]; `; src/SMS_ContainerMPEG_PS.c:497-507 — `int lTotalLen = lLen + lpMyCont -> m_BufLen;\n    int lPackLen  = lTotalLen & ~15;\n    int lnRead    = lPackLen - lpMyCont -> m_BufLen;\n    int lnRem     = lLen - lnRead;\n\n    if ( lPackLen ) {\n\n     lpPkt = apCont -> AllocPacket ( lpStm -> m_pPktBuf, lPackLen );\n     memcpy (  lpPkt -> m_pData, lpMyCont -> m_Buff, lpMyCont -> m_BufLen );\n     lpFileCtx -> Read ( lpFileCtx, lpPkt -> m_pData + lpMyCont -> m_BufLen, lnRead );\n     lpFileCtx -> Read ( lpFileCtx, lpMyCont -> m_Buff, lpMyCont -> m_BufLen = lnRem );`",
            "arithmetic": "lnRem = lLen - lnRead = lLen - (lPackLen - m_BufLen) = lTotalLen - (lTotalLen & ~15) = lTotalLen mod 16, which is in [0,15] — exactly the capacity of m_Buff[15]. When lPackLen != 0 it is a multiple of 16 and therefore >= 16 > 15 >= m_BufLen, so lnRead is never negative. No overflow is reachable.",
            "converter_impact": "No constraint on the converter. Video PES payload length need not be a multiple of 16 — the demuxer handles the realignment for the IPU."
          },
          {
            "claim": "_fill_audio_parameters saves and restores FileContext::m_Pos, but the demuxer's logical position is m_CurPos. m_Pos is only advanced by Read (not by File_GetByte, and not by Fill), so after probing an audio stream the file is rewound by however many bytes were consumed via File_GetByte since the last Seek, and the detection loop re-scans that region.",
            "kind": "SOFT",
            "evidence": "src/SMS_ContainerMPEG_PS.c:339 — ` int         lPos   = apFileCtx -> m_Pos;` and :443 — ` apFileCtx -> Seek ( apFileCtx, lPos );` versus the correct pattern in _fill_video_parameters at :289 — ` unsigned int lPos      = lpFileCtx -> m_CurPos;`; include/SMS_FileContext.h:93 — `return apCtx -> m_pPos < apCtx -> m_pEnd || apCtx -> Fill ( apCtx ) > 0 ? ++apCtx -> m_CurPos, *apCtx -> m_pPos++ : 0;` (only m_CurPos); src/SMS_FileContext.c:1295-1296 — `apCtx -> m_Pos       += lLen;\n   apCtx -> m_CurPos    += lLen;` (Read advances both); src/SMS_FileContext.c:1367-1368 (STIO_Fill advances neither)",
            "arithmetic": "Lag = number of bytes read via File_GetByte / File_GetShortBE since the last Seek. In the detection loop that is roughly the PES header size (about 10-20 bytes) per packet processed, accumulated since file open.",
            "converter_impact": "No direct converter action, but it explains non-deterministic-looking probe behaviour: detection may re-walk the same bytes. It reinforces the advice to place all streams within the first few hundred KB rather than relying on the full 1 MiB window."
          }
        ],
        "gaps": [
          "Exact main-thread stack size could not be determined. SMS_InitPlayer (and therefore SMS_GetContainerMPEG_PS) runs on the thread that executes the GUI event loop in src/SMS_GUI.c:987, not on the 4096-byte s_Stack thread (src/SMS_GUI.c:92) and not on the 16384-byte player worker threads (src/SMS_Player.c:1784-1805). No linker script or crt0 stack definition exists in this repo, so I cannot state the ceiling at which the ~65 KB VLA at src/SMS_ContainerMPEG_PS.c:340 actually overflows. The VLA arithmetic itself is confirmed; the overflow threshold is UNCONFIRMED.",
          "Whether a PSM-declared AC3 stream (stream_type 0x81 on some elementary_stream_id other than 0xBD) actually decodes is UNCONFIRMED. In that path the 4-byte private_stream_1 header is not stripped, and I did not trace whether the AC3 decoder's own resync at src/SMS_AC3.c:2708 recovers from the 4-byte prefix.",
          "IPU / libmpeg-level video constraints are NOT covered here — MPEG profile/level, chroma format (4:2:0 vs 4:2:2), intra_dc_precision, GOP structure limits, macroblock-rate ceilings, and maximum video bitrate all live in src/SMS_MPEG12.c, src/SMS_IPU*.c and include/libmpeg*.h, which I did not audit. The only video constraint I can prove from the container path is the 1024x1024 player check.",
          "Whether the SPU / audio renderer actually supports the 96000 Hz entry in the LPCM sample-rate table (src/SMS_ContainerMPEG_PS.c:411-413) is UNCONFIRMED — I did not audit src/SMS_SPU.c or the audio renderer's resampling path.",
          "MPEG-1 pack-header start-code emulation was only reasoned about, not proven from source. The demuxer skips pack and system headers by pure start-code resync (src/SMS_ContainerMPEG_PS.c:149) rather than by length, so it depends on those headers containing no 00 00 01 sequence. I verified from the MPEG-2 field layout that marker bits prevent this, but I have no source in this repo that enforces or documents it.",
          "A tangential but converter-relevant hazard I could not fully resolve: src/SMS_Container.c:129-140 uses `SMS_ContID` which returns -1 for an unrecognized extension, and the guard `(lID = SMS_ContID(...)) && (lID < 12)` passes for -1, giving `lCC[0] = s_CCreator[-1]`. Separately, the reordering loop leaves lCC[12] uninitialized while `while (lCC[i])` reads it. This governs what happens when an .mpg is REJECTED by this demuxer and probing continues. I did not determine whether either is reachable in a real build."
        ]
      },
      {
        "area": "Video decoder ceilings (MPEG-1/2 via IPU + libmpeg; MPEG-4 Part 2 / MSMPEG4v3 software) — SMS at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014",
        "findings": [
          {
            "claim": "GLOBAL HARD CEILING: any video stream whose container-reported width OR height exceeds 1024 is refused before decoding starts, with the error string STR_RESOLUTION_TOO_BIG. This gate is codec-independent — it runs after SMS_CodecOpen and before codec Init, so it applies equally to MPEG-1/2 (IPU), MPEG-4 Part 2 and MSMPEG4v3.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:1674-1681 — `SMS_CodecContext* lpVideoCtx = lpStm -> m_pCodec;` / `if ( lpVideoCtx -> m_Width  > 1024 ||` / `     lpVideoCtx -> m_Height > 1024` / `) {` / ` s_Player.m_pErrorMsg = &STR_RESOLUTION_TOO_BIG;` / ` goto destroy;` / `}  /* end if */`. String id: include/SMS_Locale.h:284 `#define STR_RESOLUTION_TOO_BIG   SMS_STRING( 260 )`.",
            "arithmetic": "Strict `>` comparison, so 1024 itself is allowed; 1025 is rejected. Matches the GS texture-size limit: src/SMS_Utils.S:286-299 `PowerOf2` computes ceil(log2(x)) into TW/TH, and GS TEX0.TW/TH saturate at 10 (=1024). For W=1024: x-1=1023, plzcw=21, TW=31-21=10. For W=1025: TW=11, which is out of range for the GS.",
            "converter_impact": "FORBID width>1024 or height>1024 for every codec. Clamp/scale to <=1024x1024. Do not treat this as a soft target — the player refuses the file outright."
          },
          {
            "claim": "The effective usable resolution is far below 1024x1024 because the decoded frame is uploaded to GS VRAM as a texture that must fit under the display framebuffers. At 1024x1024 in 32-bit color the texture needs the ENTIRE 4 MiB of VRAM and the computed texture base becomes 0, guaranteeing overlap with the display buffer.",
            "kind": "HARD",
            "evidence": "src/SMS_IPU.c:735 `unsigned int lTBW = ( aWidth + 63 ) >> 6;`; :751-752 `lImgSize = (   ( lTBW << 6 ) * (  ( aHeight + 31 ) & ~31  ) * ( 4 >> lf16 )   ) >> 8;` / `lVRAM    = 0x4000 - lImgSize;`; :754-757 `if ( !lf16 && g_GSCtx.m_VRAMFontPtr && lVRAM < g_GSCtx.m_VRAMPtr ) { lf16 = 1; goto retry; }`.",
            "arithmetic": "Units are 256-byte GS blocks; 0x4000 blocks = 4 MiB. lImgSize = (ceil(W/64)*64) * ((H+31)&~31) * bytes_per_pixel / 256, bpp=4 (PSMCT32) or 2 (PSMCT16). 1024x1024@32bpp: 1024*1024*4/256 = 16384 = 0x4000 -> lVRAM = 0. 1024x768@32bpp: 1024*768*4/256 = 12288 -> lVRAM = 4096. 640x480@32bpp: 640*480*4/256 = 4800 -> lVRAM = 11584. 720x576@32bpp: TBW=12 so 768*576*4/256 = 6912 -> lVRAM = 9472. The display framebuffer top (g_GSCtx.m_VRAMPtr, src/SMS_GS_1.c:137,162 = ZBP<<5) is 150*32 = 4800 blocks for a 640x480 PSMCT24 display (src/SMS_GS_0.S:556 `mult $v0, $t3, $t2` with t3=(W+63)>>6=10, t2=(H+31)>>5=15). So the texture must satisfy lImgSize <= 16384-4800 = 11584 blocks, i.e. ceil(W/64)*64 * ((H+31)&~31) <= 741,376 pixels in 32-bit color. Examples that fit: 720x576 (442,368), 1024x576 (589,824), 1024x704 (720,896). Does NOT fit: 1024x736+ (753,664).",
            "converter_impact": "Target padded_W*padded_H <= ~741k pixels (padded_W = W rounded up to 64, padded_H = H rounded up to 32) to stay safe in 32-bit color. 720x576 and 1024x576 are safe; 1024x768 and 1024x1024 are not. Note the automatic 16-bit fallback at SMS_IPU.c:754 only fires when the DISPLAY is already 16-bit (g_GSCtx.m_VRAMFontPtr is non-zero only for PSMCT16, src/SMS_GS_1.c:168-170) — otherwise VRAM overlap happens silently."
          },
          {
            "claim": "MPEG-4 Part 2 / MSMPEG4v3 (software path) allocate a fixed set of buffers all sized from MBW=ceil(W/16) and MBH=ceil(H/16). Total heap for the video decoder alone reaches ~19.6 MiB at 1024x1024 and ~6.0 MiB at 640x480, on top of a fixed 2.5 MiB of packet ring buffers.",
            "kind": "HARD",
            "evidence": "src/SMS_MPEGInit.c:28-29 `g_MPEGCtx.m_MBW = ( aWidth  + 15 ) / 16;` / `g_MPEGCtx.m_MBH = ( aHeight + 15 ) / 16;`. Allocations: src/SMS_VideoBuffer.c:33-36,59 `lMBW = ((aWidth+15)>>4)+2; lMBH = ((aHeight+15)>>4)+2; lFrameSize = lMBW*lMBH*sizeof(SMS_MacroBlock);` ... `memalign(128, lFrameSize + lDMASize)` x3 (loop `for (i=0;i<3;++i)`); src/SMS_VideoBuffer.c:286 `memalign( 128, lSize + ((10 + 12*lMBW*lMBH) << 3) )` x3 (SMS_MPEGCSC m_Buf[3], include/SMS_MPEG4.h:29); src/SMS_MPEG.c:42-50 per-picture tables x MAX_PICTURE_COUNT=3; src/SMS_MPEG4.c:496,501 `calloc(1, lYCSize*sizeof(int16_t)*16)` and `calloc(1, lYCSize*sizeof(int16_t))`; src/SMS_MPEGInit.c:48,58,59,60; src/SMS_MPEG4.c:657-658. Fixed rings: src/SMS_Player.c:54-55 `#define SMS_VP_BUFFER_SIZE ( 1024 * 1024 * 2 )` / `#define SMS_AP_BUFFER_SIZE ( 1024 *  512 )`.",
            "arithmetic": "sizeof(SMS_MacroBlock)=16*16+8*8+8*8=384 (include/SMS.h:198-204). With W'=(W+15)&~15, H'=(H+15)&~15, MBStride=MBW+1, B8Stride=2*MBW+1:\n  A) YUV ref frames = 3*((MBW+2)*(MBH+2)*384 + MBH*16)\n  B) RGB output    = 3*(W'*H'*4 + (10+12*(W'/16)*(H'/16))*8)\n  C) 3 pictures    = 3*[(MBStride*MBH+2) + MBStride*MBH + (MBStride*(MBH+1)+1)*4 + 2*(2*(B8Stride*MBH*2+2)*2) + 2*(B8Stride*MBH*2)]\n  D) AC/DC pred    = (B8Stride*(2*MBH+1) + 2*MBStride*(MBH+1)) * 34\n  E) misc + 256 KiB bitstream buffer, F) CBP/PredDir = 2*MBStride*MBH\nTotals (32-bit RGB): 320x240 -> 1,813,185 B (1.73 MiB); 512x384 -> 4,140,678 (3.95); 640x480 -> 6,275,660 (5.98); 720x480 -> 7,017,070 (6.69); 720x576 -> 8,344,954 (7.96); 848x480 -> 8,203,326 (7.82); 1024x576 -> 11,716,466 (11.17); 1024x768 -> 15,480,542 (14.76); 1024x1024 -> 20,499,310 (19.55). In 16-bit output mode the RGB term halves: 640x480 -> 4.23 MiB, 1024x1024 -> 13.55 MiB. Add SMS_VP_BUFFER_SIZE (2 MiB) + SMS_AP_BUFFER_SIZE (0.5 MiB), plus the program image, IOP module blob (include/SMS_Data.h g_DataBuffer), GUI textures and audio decoder state.",
            "converter_impact": "Budget-wise 720x576 (~8 MiB) and 848x480 (~7.8 MiB) are comfortable; 1024x768 (~14.8 MiB) and 1024x1024 (~19.6 MiB) leave very little of a 32 MiB console. Recommend a converter default ceiling of 720x576 for MPEG-4/MSMPEG4v3 and a hard warning above ~1024x576."
          },
          {
            "claim": "MPEG-1/2 (IPU + libmpeg) allocates only 3 YUV macroblock frames plus the 3 RGB output frames; it does NOT use SMS_VideoBufferInit. Its footprint is materially smaller than the software path at the same resolution.",
            "kind": "HARD",
            "evidence": "src/libmpeg.c:165-177 `lSize = lMBWidth * ( lMBHeight + 1 ) * sizeof ( _MPEGMacroBlock8 ) + sizeof ( _MPEGMacroBlock8 );` then three `memalign ( 128, lSize )` for m_pFwdFrame/m_pBckFrame/m_pAuxFrame, plus `malloc( sizeof(_MPEGMBXY) * (m_MBCount = m_MBWidth*m_MBHeight) )`. src/SMS_MPEG12.c:27 `SMS_FrameBuffer m_Pics[ 3 ];` and :149-152 `SMS_FrameBufferInit ( s_MPEGState.m_Pics, 3, lWidth, lHeight, s_MPEGState.m_f16 );`. src/SMS_MPEG12.c:50-76 `_hwctl` handles only SMS_HWC_Init/SMS_HWC_Reset — no SMS_VideoBufferInit call.",
            "arithmetic": "sizeof(_MPEGMacroBlock8)=384 (include/libmpeg_internal.h:68-74). Totals (32-bit RGB): 320x240 -> 1,378,632 B (1.31 MiB); 640x480 -> 5,464,272 (5.21); 720x480 -> 6,147,132 (5.86); 720x576 -> 7,365,912 (7.02); 1024x576 -> 10,475,376 (9.99); 1024x1024 -> 18,564,464 (17.70). Plus the same 2 MiB + 0.5 MiB fixed rings.",
            "converter_impact": "MPEG-2 at 720x576 costs ~7.0 MiB vs ~8.0 MiB for MPEG-4 at the same size, and offloads decode to the IPU. Prefer MPEG-2 for DVD-resolution targets."
          },
          {
            "claim": "libmpeg (MPEG-1/2) forces both dimensions up to whole macroblocks, and for INTERLACED MPEG-2 it forces height up to a multiple of 32, then overwrites m_SI.m_Width/m_Height with the rounded values.",
            "kind": "HARD",
            "evidence": "src/libmpeg.c:148-161 — `lMBWidth  = ( s_MPEG12Ctx.m_SI.m_Width + 15 ) / 16;` / `lMBHeight = s_MPEG12Ctx.m_fMPEG2 && !s_MPEG12Ctx.m_fProgSeq ? 2 * (  ( s_MPEG12Ctx.m_SI.m_Height + 31 ) / 32  )` / `                                                            : ( s_MPEG12Ctx.m_SI.m_Height + 15 ) / 16;` ... `s_MPEG12Ctx.m_SI.m_Width  = lMBWidth  << 4;` / `s_MPEG12Ctx.m_SI.m_Height = lMBHeight << 4;`",
            "arithmetic": "Interlaced example: H=240 -> 2*ceil(240/32) = 2*8 = 16 MB rows = 256 lines, i.e. 16 extra lines vs. the 240 the IPU display path was configured for (src/SMS_Player.c:161-162 uses the CONTAINER dimensions for IPU_InitContext). H=480 -> 2*15 = 30 -> 480 (exact). H=576 -> 2*18 = 36 -> 576 (exact).",
            "converter_impact": "For interlaced MPEG-2 use a height that is a multiple of 32 (480, 576 are fine; 240, 272, 528 are not). For progressive, a multiple of 16 is sufficient. Any other value is padded and the padding rows are decoded but cropped by the display path."
          },
          {
            "claim": "libmpeg stores per-macroblock X/Y as unsigned char, capping the macroblock grid at 255x255 (4080x4080 pixels) before silent wraparound. Not binding under the 1024 gate.",
            "kind": "HARD",
            "evidence": "include/libmpeg_internal.h:61-66 — `typedef struct _MPEGMBXY {` / ` unsigned char m_X;` / ` unsigned char m_Y;` / `} _MPEGMBXY;`  and src/libmpeg.c:180-181 `s_MPEG12Ctx.m_pMBXY[ i ].m_X = i % lMBWidth;` / `s_MPEG12Ctx.m_pMBXY[ i ].m_Y = i / lMBWidth;`",
            "arithmetic": "m_Y wraps once MBH > 255, i.e. height > 4080. m_X wraps once MBW > 255, i.e. width > 4080. Both are far above the 1024 gate, so this is documentation only.",
            "converter_impact": "No action; recorded so the 1024 cap is not mistakenly attributed to this."
          },
          {
            "claim": "libmpeg parses the MPEG-2 slice header with an 8-bit slice_vertical_position and never reads slice_vertical_position_extension, so vertical_size > 2800 would decode to the wrong rows. Not binding under the 1024 gate.",
            "kind": "HARD",
            "evidence": "src/libmpeg.c:1276 `if ( lMBType < _MPEG_CODE_SLICE_MIN || lMBType > _MPEG_CODE_SLICE_MAX ) return -1;` (include/libmpeg_internal.h:41-42: 0x00000101..0x000001AF) and src/libmpeg.c:1280-1291 — after `_MPEG_GetBits ( 32 )` the very next read is `s_MPEG12Ctx.m_QScale = _MPEG_GetBits ( 5 );`, with no 3-bit extension read, then `lMBA = (  ( lMBType & 255 ) - 1  ) * s_MPEG12Ctx.m_MBWidth + lMBAI - 1;`",
            "arithmetic": "0x1AF - 0x100 = 0xAF = 175 slice rows max -> 175*16 = 2800 lines. MPEG-2 requires slice_vertical_position_extension only above 2800.",
            "converter_impact": "No action under the 1024 cap."
          },
          {
            "claim": "B-FRAMES: fully supported by the MPEG-1/2 IPU path and by the MPEG-4 Part 2 software path (with a one-frame reorder delay). NOT supported by MSMPEG4v3 — a non-I/non-P picture type causes the picture header parser to bail before reading qscale, and decoding then proceeds on stale state.",
            "kind": "HARD",
            "evidence": "MPEG-1/2: src/libmpeg.c:676-682 `if ( s_MPEG12Ctx.m_PictCodingType == _MPEG_PT_B ) { lpData = s_MPEG12Ctx.m_pAuxFrame; *apPTS = s_MPEG12Ctx.m_AuxPTS; } else {...}` plus full backward-MC in `_mpeg12_get_refs` (src/libmpeg.c:1160-1212). MPEG-4: src/SMS_MPEG4.c:3359-3361 `if ( g_MPEGCtx.m_PicType == SMS_FT_B_TYPE || g_MPEGCtx.m_LowDelay )` / `  lpFrame = g_MPEGCtx.m_CurPic.m_pBuf;` / ` else lpFrame = g_MPEGCtx.m_LastPic.m_pBuf;` and direct-mode at src/SMS_MPEG4.c:1504-1580. MSMPEG4v3: src/SMS_MSMPEG4.c:2126-2130 `g_MPEGCtx.m_PicType = SMS_GetBits ( lpBitCtx, 2 ) + 1;` / `if ( g_MPEGCtx.m_PicType != SMS_FT_I_TYPE &&` / `     g_MPEGCtx.m_PicType != SMS_FT_P_TYPE` / `) return 1;` — and the caller at src/SMS_MSMPEG4.c:2308 only aborts on a NEGATIVE return: `if (  _msmpeg4_decode_picture_header () < 0  ) goto end;`",
            "arithmetic": "",
            "converter_impact": "B-frames are safe for MPEG-2 and MPEG-4 Part 2. For MSMPEG4v3 (DIV3/MP43/AP41/COL1...) emit I and P only — the format has no B-frames anyway, but the decoder does not fail cleanly if one appears."
          },
          {
            "claim": "QPEL (quarter-pel MC) is fully implemented in the MPEG-4 path: all 16 sub-positions exist for put / put-no-round / avg, both 16x16 and 8x8, in hand-written MMI assembly.",
            "kind": "HARD",
            "evidence": "src/SMS_MPEG4.c:959 `g_MPEGCtx.m_QuarterSample = lVerID != 1 ? SMS_GetBit ( apBitCtx ) : 0;`; dispatch at src/SMS_MPEG.c:727-733 `} else if ( g_MPEGCtx.m_QuarterSample ) { _qpel_motion ( apDestMB, apRefPic, aPixOp, aQPelOp, ... );` and src/SMS_MPEG.c:582-584 `aQPelOp[ 0 ][ lDXY ] ( &apDestMB -> m_Y[ 0 ][ 0 ], ... );`. Tables fully populated at src/SMS_DSP.c:414-429 (m_PutQPelPixTab[0][0..15]), :431-446 (m_PutNoRndQPelPixTab[0]), :448-463 (m_PutQPelPixTab[1]), :465-480 (m_PutNoRndQPelPixTab[1]), :482-491+ (m_AvgQPelPixTab). Implementations in src/SMS_DSP_QPel.S (124 KB of assembly).",
            "arithmetic": "",
            "converter_impact": "QPel may be enabled. Note it costs significantly more CPU than half-pel on a 294 MHz EE (SOFT/performance, not correctness) — for 640x480+ prefer disabling it."
          },
          {
            "claim": "GMC is implemented for 0, 1, 2 and 3 sprite warp points only. FOUR warp points (perspective GMC) is silently unimplemented — the switch has no case 4, so the sprite offsets/deltas from the PREVIOUS frame are reused.",
            "kind": "HARD",
            "evidence": "src/SMS_MPEG4.c:1196-1291 — `switch ( g_MPEGCtx.m_nSpriteWarpPts ) {` with only `case 0:`, `case 1:`, `case 2:`, `case 3:` and no `case 4:` / `default:`. MC entry points exist and are real: src/SMS_MPEG.c:471-476 `_gmc_motion` -> `DSP_GMCn_16`/`DSP_GMCn_8` (implemented src/SMS_DSP.c:75-187 and :189-325) and src/SMS_MPEG.c:478-548 `_gmc1_motion` -> `DSP_GMC1_16`/`DSP_GMC1_8` (src/SMS_DSP_MMI.S:1435, :1517). Selection at src/SMS_MPEG.c:719-725 `if ( g_MPEGCtx.m_MCSel ) { if ( g_MPEGCtx.m_RealSpriteWarpPts == 1 ) _gmc1_motion(...); else _gmc_motion(...); }`",
            "arithmetic": "",
            "converter_impact": "If GMC is emitted at all, restrict to <=3 warp points. Safest: disable GMC entirely (ffmpeg `-flags -gmc`, or avoid `-vtag` profiles that enable sprites)."
          },
          {
            "claim": "BUFFER OVERFLOW: no_of_sprite_warping_points is read as an unclamped 6-bit field (0..63) and used directly to index a 4-entry stack array. A stream declaring 5..63 warp points overruns the stack frame of _mpeg4_decode_sprite_trajectory.",
            "kind": "HARD",
            "evidence": "src/SMS_MPEG4.c:893 `g_MPEGCtx.m_nSpriteWarpPts = SMS_GetBits ( apBitCtx, 6 );` (no range check anywhere — grep for m_nSpriteWarpPts shows only this write and the reads below). src/SMS_MPEG4.c:1134 `int lD[ 4 ][ 2 ] = {  { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }  };` and :1143,1160-1161 `for ( i = 0; i < g_MPEGCtx.m_nSpriteWarpPts; ++i ) { ... lD[ i ][ 0 ] = lX; lD[ i ][ 1 ] = lY; }`",
            "arithmetic": "lD is 4*2*4 = 32 bytes. With nSpriteWarpPts = 63 the loop writes lD[0..62], i.e. 63*8 = 504 bytes — 472 bytes past the array, straight over the saved registers / return address of the stack frame.",
            "converter_impact": "Never emit >3 warp points (the MPEG-4 spec itself allows 0..4, so even a spec-legal 4-point sprite is mishandled and a corrupt/hostile file crashes). Converter should strip sprite/GMC VOLs."
          },
          {
            "claim": "STATIC SPRITE (video_object_layer_sprite_usage == 1) is parsed but never decoded: no sprite buffer is ever allocated or reconstructed, and mcsel is only read for GMC_SPRITE, so S-VOPs in a static-sprite stream produce no sprite prediction.",
            "kind": "HARD",
            "evidence": "Parsed: src/SMS_MPEG4.c:876-897 (`STATIC_SPRITE` = 1, `GMC_SPRITE` = 2 at :31-32) reads m_SpriteWidth/Height/Left/Top and m_LowLatencySprite. Used: src/SMS_MPEG4.c:2057-2061 `if ( ... g_MPEGCtx.m_VolSpriteUsage == GMC_SPRITE && ... ) g_MPEGCtx.m_MCSel = SMS_GetBit ( lpBitCtx );` — the STATIC_SPRITE case never sets m_MCSel; grep for m_SpriteWidth/m_SpriteHeight/m_SpriteLeft/m_SpriteTop across src/ shows writes only, no reads.",
            "arithmetic": "",
            "converter_impact": "Forbid static sprites entirely."
          },
          {
            "claim": "INTERLACED MPEG-4 IS BROKEN THREE WAYS. (1) Field motion compensation is never performed: _MPEG_Motion switches only on SMS_MV_TYPE_16X16 and SMS_MV_TYPE_8X8, with no case for SMS_MV_TYPE_FIELD and no default. (2) g_MPEGCtx.m_pPFieldMVTbl is NEVER allocated anywhere in the tree, yet field MBs write through it -> NULL-pointer store. (3) interlaced_dct is parsed but never read, so field-DCT coefficient de-interleaving is not done.",
            "kind": "HARD",
            "evidence": "(1) src/SMS_MPEG.c:715-813 — `switch ( g_MPEGCtx.m_MVType ) {` / `  case SMS_MV_TYPE_16X16:` ... `break;` / `  case SMS_MV_TYPE_8X8: {` ... `} break;` / ` }  /* end switch */` — nothing else. SMS_MV_TYPE_FIELD is set at src/SMS_MPEG4.c:1534, :2091 and :2282. (2) include/SMS_MPEG.h:138 declares `int16_t ( *m_pPFieldMVTbl[ 2 ][ 2 ] )[ 2 ];`; the only zeroing is src/SMS_MPEGInit.c:21 `memset (  &g_MPEGCtx, 0, sizeof ( g_MPEGCtx )  );`; a repo-wide grep for m_pPFieldMVTbl returns only include/SMS_MPEG.h:138, the WRITES at src/SMS_H263.c:182-183 `g_MPEGCtx.m_pPFieldMVTbl[ i ][ 0 ][ lMBXY ][ 0 ] = g_MPEGCtx.m_MV[ 0 ][ i ][ 0 ];` and the READS at src/SMS_MPEG4.c:1552-1557 — never a malloc/calloc. (3) grep m_InterlacedDCT: only writes at src/SMS_MPEG4.c:2072, :2225, :2369 — zero reads.",
            "arithmetic": "Path to the NULL store: a P-VOP with progressive_sequence==0 hits src/SMS_MPEG4.c:2088-2091 (`} else if (  !g_MPEGCtx.m_ProgSeq && SMS_GetBit ( lpBitCtx )  ) { ... g_MPEGCtx.m_MVType = SMS_MV_TYPE_FIELD;`), then src/SMS_MPEG4.c:3133 `if ( g_MPEGCtx.m_PicType != SMS_FT_B_TYPE ) SMS_H263_UpdateMotionVal ();` reaches src/SMS_H263.c:158 `if ( g_MPEGCtx.m_MVType != SMS_MV_TYPE_8X8 ) {` ... :172 else-branch ... :182 store through the NULL base pointer.",
            "converter_impact": "FORBID interlaced MPEG-4 Part 2 output unconditionally. Always encode progressive (ffmpeg: no `-flags +ilme+ild`, and deinterlace interlaced sources with yadif). Interlaced MPEG-4 is not merely 'wrong-looking' — it dereferences a null pointer."
          },
          {
            "claim": "INTERLACED MPEG-2 IS FULLY SUPPORTED by libmpeg: field pictures, frame/field/16x8/dual-prime motion types, top-field-first, frame_pred_frame_dct and alternate scan are all implemented.",
            "kind": "HARD",
            "evidence": "src/libmpeg.c:586-591 `s_MPEG12Ctx.m_PictStruct = _MPEG_GetBits ( 2 ); s_MPEG12Ctx.m_fTopFF = _MPEG_GetBits ( 1 ); s_MPEG12Ctx.m_fFPFrmDCT = _MPEG_GetBits ( 1 ); s_MPEG12Ctx.m_fConsMV = _MPEG_GetBits ( 1 );`; motion types dispatched at src/libmpeg.c:1056-1212 with distinct code for `_MPEG_MC_FRAME`, `_MPEG_MC_FIELD`, `_MPEG_MC_16X8` and `_MPEG_MC_DMV` (incl. `_mpeg12_dual_prime_vector`, src/libmpeg.c:791-824); field-aware block writers at src/libmpeg.c:34-41 `PutBlockOp[3]` / `AddBlockOp[2][2]` -> `_MPEG_put_block_il`, `_MPEG_add_block_ilfl` etc. (src/libmpeg_core.S:36-41).",
            "arithmetic": "",
            "converter_impact": "If the source is interlaced and you must preserve it, use MPEG-2, not MPEG-4. Height must be a multiple of 32 for interlaced MPEG-2 (see the libmpeg rounding finding)."
          },
          {
            "claim": "MPEG-1/2 FRAME RATE IS RESTRICTED TO THE 8 STANDARD CODES. frame_rate_code 0 and 9..15 index zero entries in the frame-rate table and produce a divide-by-zero float, which is then cast to unsigned and written to the COP0 Compare register that drives the frame-pacing timer.",
            "kind": "HARD",
            "evidence": "src/libmpeg.c:43-48 `static float s_FrameRate[ 16 ] = {` / `  0.0F, ( ( 23.0F*1000.0F)/1001.0F ), 24.0F, 25.0F, ( (30.0F*1000.0F)/1001.0F ), 30.0F, 50.0F, ( (60.0F*1000.0F)/1001.0F ), 60.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F };` and :268-271 `lFrameRate = ( 1000.0F / s_FrameRate[ s_MPEG12Ctx.m_FRCode = _MPEG_GetBits ( 4 ) ] );` / `lFrameRateInt = ( unsigned )( lFrameRate * 147456.0F + 0.5F );` then :273-283 `mtc0 %0, $11`.",
            "arithmetic": "Table entry 1 is coded as 23.0*1000/1001 = 22.977, NOT 24000/1001 = 23.976 — so frame_rate_code 1 (23.976 fps) yields ms/frame = 1000/22.977 = 43.52 instead of 41.71, a ~4.3% pacing error. Legal codes: 1..8 = {22.977(sic), 24, 25, 29.97, 30, 50, 59.94, 60}.",
            "converter_impact": "For MPEG-1/2 emit only frame_rate_code 1..8. Prefer 25 (code 3), 29.97 (code 4) or 30 (code 5); AVOID code 1 (23.976) because the table constant is wrong and playback will run ~4% slow. Never emit a reserved code."
          },
          {
            "claim": "MPEG-4 frame rate comes exclusively from the CONTAINER (m_FrameRate / m_FrameRateBase), not from the bitstream, and is converted to a COP0 Compare cycle count. A zero or missing container frame rate produces a divide-by-zero.",
            "kind": "HARD",
            "evidence": "src/SMS_MPEG4.c:513-525 — `lFrameRate = ( float )apCtx -> m_FrameRate / ( float )apCtx -> m_FrameRateBase;` / `g_MPEGCtx.m_MSPerFrame = ( int )( lFrameRate + 0.5F );` / ... / `__asm__ __volatile__( \"mtc0 %0, $11\\n\\t\" :: \"r\"(  ( uint32_t )(  ( 1000.0F / lFrameRate ) * 147456.0F + 0.5F )  ) );`  No guard on m_FrameRateBase == 0.",
            "arithmetic": "COP0 Count runs at 147.456 MHz, so cycles/frame = ms_per_frame * 147456. Overflow of the 32-bit Compare needs (1000/fps)*147456 >= 2^32, i.e. fps <= 0.0343 — not practically reachable. Note m_MSPerFrame is assigned the FPS, not ms/frame, but grep shows g_MPEGCtx.m_MSPerFrame is never read, so it is inert.",
            "converter_impact": "Always write a valid, non-zero frame rate into the AVI `dwRate/dwScale` or MP4 timescale. Do not rely on the VOL vop_time_increment_resolution to set playback speed — SMS ignores it for pacing."
          },
          {
            "claim": "ASPECT RATIO IS IGNORED BY BOTH DECODERS. MPEG-4's pixel_aspect_ratio is parsed into m_SampleAspectRatio and never read; libmpeg discards MPEG-1/2 aspect_ratio_information outright. The only aspect signal that reaches the display is a single boolean widescreen flag, set only by the MPEG-PS demuxer when aspect_ratio_information == 3.",
            "kind": "HARD",
            "evidence": "MPEG-4: src/SMS_MPEG4.c:800-807 `g_MPEGCtx.m_AspectRatio = SMS_GetBits ( apBitCtx, 4 ); ... g_MPEGCtx.m_SampleAspectRatio = s_pixel_aspect[ g_MPEGCtx.m_AspectRatio ];` — a repo-wide grep for m_SampleAspectRatio finds only these writes. MPEG-1/2: src/libmpeg.c:266 `_MPEG_GetBits ( 4 );  /* aspect_ratio_information    */` (value discarded). Widescreen flag: src/SMS_ContainerMPEG_PS.c:304 `if (  SMS_GetBits ( &lBitCtx, 4 ) == 3  ) apStm -> m_pCodec -> m_fWS = 1;` -> src/SMS_Player.c:163 `lfWS = lpCodecCtx -> m_fWS;` -> src/SMS_IPU.c:774,423,511. Display AR otherwise: src/SMS_IPU.c:419 `float lAR = ( float )aWidth / ( float )g_IPUCtx.m_Height;` with :423 `if ( g_IPUCtx.m_fWS && anIdx < 4 ) lAR += ( 16.0F / 9.0F - 4.0F / 3.0F );`",
            "arithmetic": "",
            "converter_impact": "ENCODE SQUARE PIXELS. Pre-scale anamorphic sources to their display aspect (e.g. 720x576 PAL 16:9 -> re-scale to 1024x576 or accept 4:3 stretching). For MPEG-PS specifically you can set aspect_ratio_information == 3 to get the built-in 16:9 correction, but nothing else works. For AVI/MP4 MPEG-4 there is NO way to signal anamorphic — m_fWS is never set on those paths (grep shows m_fWS is written only in SMS_ContainerMPEG_PS.c)."
          },
          {
            "claim": "FRAME PACING / A-V SYNC WINDOW: a frame is only presented when its PTS is within [audio_pts - 200 ms, audio_pts + 80 ms]. Frames outside that window are held or dropped.",
            "kind": "SOFT",
            "evidence": "src/SMS_IPU.c:272-281 — `if ( !s_fStopSync && g_IPUCtx.m_pAudioPTS ) {` / `  int64_t lAudioPTS = *g_IPUCtx.m_pAudioPTS;` / `  int64_t lDiff     = lVPTS - lAudioPTS;` / `  if ( lDiff > 80 || !lAudioPTS )` / `   return;` / `  else if ( lDiff > -200 && !s_fRefresh ) return;` / ` } else if ( !s_fRefresh ) return;`",
            "arithmetic": "",
            "converter_impact": "Keep audio and video timestamps tightly aligned; an A-V offset beyond +80 ms of video-ahead or -200 ms of video-behind makes the renderer stall or drop. Avoid variable frame rate."
          },
          {
            "claim": "2:2 PULLDOWN is applied automatically on PAL/576p output for sources between 23.8 and 25.0 fps, rewriting the stream to exactly 25 fps and rescaling audio timebases.",
            "kind": "SOFT",
            "evidence": "src/SMS_Player.c:1537-1560 — `if ( s_Player.m_VideoIdx >= 0 && !( s_Player.m_Flags & SMS_FLAGS_SPDIF ) && ( g_Config.m_PlayerFlags & SMS_PF_PDW22 ) && (  lVideoMode == GSVideoMode_PAL || lVideoMode == GSVideoMode_DTV_640x576P )` / `) {` ... `float lFPS = ( float )lRate / ( float )lBase;` / `if ( lFPS > 23.8 && lFPS < 25.0F ) { s_Player.m_Flags |= SMS_FLAGS_PDW22; SMSContainer_SetPTSInfo ( lpVideoStm, 1, 25 ); lpVideoStm -> m_pCodec -> m_FrameRate = 25; ... }`",
            "arithmetic": "",
            "converter_impact": "A 23.976/24 fps source on a PAL console will be sped up to 25 fps (with matching audio resample) if the user has the PDW22 option on. If you want deterministic output, convert to 25 fps yourself for PAL targets or to 29.97/30 for NTSC."
          },
          {
            "claim": "PER-PACKET SIZE CEILING: a single compressed video access unit must fit in the 2 MiB video packet ring, otherwise the allocator returns NULL. This is the only bitrate-shaped hard limit in the pipeline.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:54 `#define SMS_VP_BUFFER_SIZE ( 1024 * 1024 * 2 )`; src/SMS_Player.c:1688-1695 `s_pVPBuff = SMS_SyncMalloc ( SMS_VP_BUFFER_SIZE ); lpStm -> m_pPktBuf = lpVideoBuffer = SMS_RingBufferInit ( ..., SMS_VP_BUFFER_SIZE );`; src/SMS_Container.c:53 `SMS_AVPacket* lpPkt = aSize ? SMS_RingBufferAlloc ( apRB, aSize + 64 ) : &s_EmptyPacket;`; src/SMS_RingBuffer.c:74-76 `aSize = ( aSize + 63 ) & 0xFFFFFFC0;` / `if ( aSize <= apRB -> m_Capacity ) {` — otherwise retVal stays NULL (:69, :110).",
            "arithmetic": "Max coded frame = 2,097,152 - 64 = 2,097,088 bytes before rounding, i.e. any single frame must be < ~2 MiB. Also, because the ring holds queued packets, sustained throughput is limited by how many packets fit: at 2 MiB of ring, a 10 Mbit/s stream buffers ~1.7 s of video.",
            "converter_impact": "Keep the maximum single-frame size well under 2 MiB (any sane bitrate does). More usefully: keep the peak bitrate modest so the 2 MiB ring can absorb read jitter from CD/DVD/USB/SMB — 1.5-4 Mbit/s is a sensible converter target."
          },
          {
            "claim": "DivX packed-bitstream deferral only engages for trailing chunks smaller than 256 KiB (BITSTREAM_BUFFER_SIZE); the pre-allocated deferral buffer is exactly 256 KiB.",
            "kind": "HARD",
            "evidence": "include/SMS_MPEG.h:97 `#define BITSTREAM_BUFFER_SIZE 1024 * 256`; src/SMS_MPEGInit.c:60 `g_MPEGCtx.m_pBSBuf = calloc ( 1, BITSTREAM_BUFFER_SIZE );`; src/SMS_MPEG4.c:3323 `if ( lBufSize - lCurPos > 5 && lBufSize - lCurPos < BITSTREAM_BUFFER_SIZE ) {` and :3349 `g_MPEGCtx.m_pBSBuf = realloc ( g_MPEGCtx.m_pBSBuf, lBufSize - lCurPos + 8 );`",
            "arithmetic": "1024*256 = 262,144 bytes, allocated unconditionally per session regardless of resolution.",
            "converter_impact": "Do NOT emit DivX packed bitstream (`-flags +global_header`/packed-B-frames). It costs a fixed 256 KiB and takes a fragile second code path. ffmpeg: avoid the `DX50`-with-packed-bframes muxing style."
          },
          {
            "claim": "THE AVI/MOV FourCC SILENTLY ENABLES BUG-COMPATIBILITY MODES THAT BREAK MOTION-VECTOR CLAMPING. A file tagged 'XVID' or 'XVIX' whose user data does not contain an 'XviD<build>' string gets m_XviDBuild = -1, which satisfies every `<= N` comparison and turns on SMS_BUG_EDGE, SMS_BUG_QPEL_CHROMA and SMS_BUG_DC_CLIP. 'DIVX' with VoType==0 and VolCtlPar==0 gets m_DivXVer = 400, which also turns on SMS_BUG_EDGE.",
            "kind": "HARD",
            "evidence": "src/SMS_MPEG4.c:3251-3255 `if ( g_MPEGCtx.m_XviDBuild == 0 && g_MPEGCtx.m_DivXVer == 0 )` / `  if ( apCtx -> m_Tag == SMS_FourCC_XVID || apCtx -> m_Tag == SMS_FourCC_XVIX ) g_MPEGCtx.m_XviDBuild = -1;`; :3257-3262 `if ( apCtx -> m_Tag == SMS_FourCC_DIVX && g_MPEGCtx.m_VoType == 0 && g_MPEGCtx.m_VolCtlPar == 0 ) g_MPEGCtx.m_DivXVer = 400;`; :3281-3283 `if ( g_MPEGCtx.m_XviDBuild && g_MPEGCtx.m_XviDBuild <= 1 ) g_MPEGCtx.m_Bugs |= SMS_BUG_QPEL_CHROMA;` / `if ( g_MPEGCtx.m_XviDBuild && g_MPEGCtx.m_XviDBuild <=12 ) g_MPEGCtx.m_Bugs |= SMS_BUG_EDGE;` / `if ( g_MPEGCtx.m_XviDBuild && g_MPEGCtx.m_XviDBuild <=32 ) g_MPEGCtx.m_Bugs |= SMS_BUG_DC_CLIP;`; :3287 `if ( g_MPEGCtx.m_DivXVer && g_MPEGCtx.m_DivXVer < 500 ) g_MPEGCtx.m_Bugs |= SMS_BUG_EDGE;`. Effect of SMS_BUG_EDGE: src/SMS_MPEG4.c:2693-2698 `if ( g_MPEGCtx.m_Bugs & SMS_BUG_EDGE ) { g_MPEGCtx.m_HEdgePos = g_MPEGCtx.m_Width; g_MPEGCtx.m_VEdgePos = g_MPEGCtx.m_Height; }` versus the correct MB-unit values at src/SMS_MPEGInit.c:33-34 `g_MPEGCtx.m_HEdgePos = g_MPEGCtx.m_MBW - 1;` / `g_MPEGCtx.m_VEdgePos = g_MPEGCtx.m_MBH - 1;`. Consumer: src/SMS_MPEG.c:441-454 `CLIP_MV()` compares the MACROBLOCK index lMBX against m_HEdgePos. Version strings parsed at src/SMS_MPEG4.c:1045-1093.",
            "arithmetic": "With SMS_BUG_EDGE at 640x480: the clamp becomes `lMBX > 640` instead of `lMBX > 39`. With fcode=7 the MV range is +/-1024 px = +/-64 MB, so lMBX can reach 39+64 = 103 unclamped. Each MB step is 384 bytes and each row is (MBW+2)*384 = 16,128 bytes, so the reference read can land 103*384 = 39,552 bytes past the row start, i.e. ~2.5 rows past the frame — beyond the 42*32*384 = 516,096-byte allocation when MBY is near the bottom. Out-of-bounds READ (garbage prediction), not a write.",
            "converter_impact": "Choose the FourCC deliberately. Prefer 'FMP4', 'mp4v' or 'DX50' — none of these hit the version heuristics (src/SMS_Codec.c:33-43 maps all of them to SMS_CodecID_MPEG4). AVOID tagging ffmpeg output as 'XVID'/'XVIX' (ffmpeg writes a 'Lavc...' user-data string, not 'XviD<build>', so it always trips the -1 path) and avoid 'DIVX'."
          },
          {
            "claim": "MSMPEG4v3 DIVIDE-BY-ZERO for very short frames: slice_height is computed as MBH / (code - 0x16) with code in 0x17..0x1F, so the divisor can be up to 9. If MBH < divisor the result is 0, and the outer loop then computes m_MBY % 0.",
            "kind": "HARD",
            "evidence": "src/SMS_MSMPEG4.c:2139-2143 — `lCode = SMS_GetBits ( lpBitCtx, 5 );` / `if ( lCode < 0x17 ) return -1;` / `g_MPEGCtx.m_SliceHeight = g_MPEGCtx.m_MBH / ( lCode - 0x16 );` and src/SMS_MSMPEG4.c:2319-2324 `while ( g_MPEGCtx.m_MBY < g_MPEGCtx.m_MBH ) { if (  g_MPEGCtx.m_MBX || g_MPEGCtx.m_MBY % g_MPEGCtx.m_SliceHeight || ... ) break;` plus src/SMS_MSMPEG4.c:2197 `if ( g_MPEGCtx.m_ResyncMBY + g_MPEGCtx.m_SliceHeight == g_MPEGCtx.m_MBY ) return;`",
            "arithmetic": "lCode range 0x17..0x1F -> divisor 1..9. m_SliceHeight == 0 whenever MBH < divisor, i.e. MBH <= 8 -> height <= 128 pixels. With m_SliceHeight == 0, src/SMS_MSMPEG4.c:2197 also returns on the very first row (ResyncMBY + 0 == MBY), so no macroblocks are decoded at all.",
            "converter_impact": "For MSMPEG4v3 keep height >= 144 (MBH >= 9). Small thumbnails/QCIF-height clips are unsafe."
          },
          {
            "claim": "SCRATCHPAD BUDGET: the software (MPEG-4/MSMPEG4) path places the per-macroblock intra flag table in EE scratchpad starting at 0x70001B30, consuming MBH*(MBW+1) bytes out of 9,424 free bytes. It does not overflow within the 1024x1024 cap. The MPEG-1/2 path's scratchpad use is fixed and resolution-independent.",
            "kind": "HARD",
            "evidence": "include/SMS.h:120 `#  define SMS_SPR_FREE        (  ( uint8_t*        )0x70001B30  )`; src/SMS_Player.c:1630 `g_pSPRTop = SMS_SPR_FREE;`; src/SMS_MPEGInit.c:64-67 `g_MPEGCtx.m_pMBIntraTbl = g_pSPRTop;` / `g_pSPRTop += g_MPEGCtx.m_MBH * g_MPEGCtx.m_MBStride;` / `g_pSPRTop  = ( unsigned char* )(  ( unsigned int )( g_pSPRTop + 15 ) & 0xFFFFFFF0  );`. MPEG-1/2 fixed map: src/libmpeg.c:108-113 `s_MPEG12Ctx.m_MC[ 0 ].m_pSPRBlk = ( void* )0x70000000; ... m_MC[ 1 ].m_pSPRMC  = ( void* )0x70002400;`.",
            "arithmetic": "EE scratchpad is 16 KiB (0x70000000-0x70003FFF; src/SMS_Spectrum.c:74 uses 0x70003800, confirming the region extends past 0x3800). Free above SMS_SPR_FREE = 0x70004000 - 0x70001B30 = 0x24D0 = 9,424 bytes. Worst case under the gate (1024x1024): MBW=64, MBH=64, MBStride=65 -> 64*65 = 4,160 bytes, 5,264 to spare. Overflow would need MBH*(MBW+1) > 9424, e.g. MBW=96/MBH=97 (1536x1552) — unreachable. MPEG-1/2: MC[1].m_pSPRMC region ends at 0x70002400 + 4*1536 = 0x70003C00 (4 motions * 1536 bytes, capped at src/libmpeg_core.S:1397-1398 `addiu $at, $zero, 4` / `pminw $a2, $a2, $at`), all within 16 KiB and independent of resolution.",
            "converter_impact": "No converter action. Recorded so scratchpad is not mistakenly cited as the resolution ceiling."
          },
          {
            "claim": "SCRATCHPAD COLLISION: the DXSB (DVD-style bitmap subtitle) decoder scribbles a whole subtitle scanline over 0x70001B30 — the exact address of the MPEG-4/MSMPEG4 per-MB intra table.",
            "kind": "HARD",
            "evidence": "src/SMS_DXSB.c:209 `unsigned char* lpDst, *lpPixmap, *lpTmp = SMS_SPR_FREE;` and :274 `mips_memset (  lpTmp + lX, ( lVal & 3 ) + 3, lLen  );` where lX advances up to lW (the subtitle header width, :199 `lW = lpHdr -> m_Width`). Colliding allocation: src/SMS_MPEGInit.c:64 `g_MPEGCtx.m_pMBIntraTbl = g_pSPRTop;` with g_pSPRTop == SMS_SPR_FREE (src/SMS_Player.c:1630). Consumers of the clobbered table: src/SMS_MPEG.c:1131-1133 `if ( g_MPEGCtx.m_pMBIntraTbl[ lMBXY ] ) SMS_MPEG_CleanIntraTblEntries (); } else g_MPEGCtx.m_pMBIntraTbl[ lMBXY ] = 1;`",
            "arithmetic": "A typical 720-wide subtitle writes 720 bytes at 0x70001B30, covering m_pMBIntraTbl[0..719] — i.e. the first ~11 MB rows at 640x480 (MBStride=41).",
            "converter_impact": "Avoid muxing DXSB subtitles alongside MPEG-4/MSMPEG4v3 video, or expect AC/DC-prediction corruption. (MPEG-1/2 is unaffected: it uses a different scratchpad map and no m_pMBIntraTbl.)"
          },
          {
            "claim": "NO CHROMA-FORMAT CHECK for MPEG-2: chroma_format is parsed from the sequence extension and stored, but never validated or acted on, before the bitstream is handed to the IPU (which is 4:2:0-only hardware).",
            "kind": "HARD",
            "evidence": "src/libmpeg.c:439 `s_MPEG12Ctx.m_SI.m_ChromaFmt = _MPEG_GetBits ( 2 );` — a repo-wide grep for m_ChromaFmt finds only this write plus the default at :104 `s_MPEG12Ctx.m_SI.m_ChromaFmt = MPEG_CHROMA_FORMAT_420;`, and no read. Decoding proceeds unconditionally: src/SMS_MPEG12.c:177-179 `DMA_SendA ( DMAC_TO_IPU, ..., ( lpPkt -> m_Size + 15 ) >> 4 );` and the 6-block-per-MB layout is hardcoded in include/libmpeg_internal.h:68-74 `_MPEGMacroBlock8 { unsigned char m_Y[16][16]; unsigned char m_Cb[8][8]; unsigned char m_Cr[8][8]; }`.",
            "arithmetic": "",
            "converter_impact": "Emit 4:2:0 (yuv420p) only. 4:2:2 / 4:4:4 MPEG-2 will not be rejected — it will be fed to hardware that cannot parse it."
          },
          {
            "claim": "MPEG-4 VOL DIMENSIONS CAN DIVERGE FROM THE ALLOCATION. SMS_MPEGContext_Init sizes every buffer from the CONTAINER width/height, then _decode_vol_header overwrites g_MPEGCtx.m_Width/m_Height from the bitstream with no re-init and no 1024 re-check.",
            "kind": "HARD",
            "evidence": "src/SMS_MPEG4.c:653 `SMS_MPEGContext_Init ( apCtx -> m_Width, apCtx -> m_Height );` (which sets m_MBW/m_MBH/m_LineSize at src/SMS_MPEGInit.c:28-29,69-70) versus src/SMS_MPEG4.c:855-865 `lW = SMS_GetBits ( apBitCtx, 13 ); ... lH = SMS_GetBits ( apBitCtx, 13 ); ... if ( lW && lH ) { g_MPEGCtx.m_Width  = lW; g_MPEGCtx.m_Height = lH; }`. The source comment at src/SMS_MPEG4.c:677-681 states this is deliberate. The 1024 gate (src/SMS_Player.c:1676) runs before Init and only sees the container values.",
            "arithmetic": "m_MBW/m_MBH/m_LineSize/m_MBStride stay at the container values, so the MB loops and buffer strides are unaffected. But m_Width/m_Height are used by _mpeg4_decode_sprite_trajectory (src/SMS_MPEG4.c:1133,1139-1140), by _mpeg_alloc_picture (src/SMS_MPEG.c:56-57) and, when SMS_BUG_EDGE is set, as the MV clamp bounds (src/SMS_MPEG4.c:2695-2696) — so a VOL that disagrees with the container widens the clamp arbitrarily (13-bit field: up to 8191).",
            "converter_impact": "Make the VOL width/height EXACTLY match the container (AVI BITMAPINFOHEADER / MP4 stsd) dimensions. Any mismatch bypasses the 1024 gate and can widen the motion-vector clamp."
          },
          {
            "claim": "REFERENCE-FRAME POOL IS FIXED AT 3 for every codec: 3 MPEG picture slots, 3 YUV macroblock frames, 3 RGB output frames. There is no room for more than 2 simultaneous reference pictures.",
            "kind": "HARD",
            "evidence": "include/SMS_MPEG.h:96 `#define MAX_PICTURE_COUNT     3`; src/SMS_MPEGInit.c:58 `g_MPEGCtx.m_pPic = calloc (  1, MAX_PICTURE_COUNT * sizeof ( SMS_Frame )  );`; include/SMS_VideoBuffer.h:43 `SMS_FrameBuffer  m_VFrm[ 3 ];` with src/SMS_VideoBuffer.c:57 `for ( i = 0; i < 3; ++i ) {`; include/SMS_MPEG4.h:29 `SMS_FrameBuffer m_Buf[ 3 ];`; src/SMS_MPEG12.c:27 `SMS_FrameBuffer   m_Pics[ 3 ];`. Exhaustion is unguarded: src/SMS_MPEG.c:130-145 `SMS_MPEGContext_FindUnusedPic` can `return NULL;`, and the caller at src/SMS_MPEG.c:181-183 does `else lpPic = SMS_MPEGContext_FindUnusedPic (); lpPic -> m_Ref = ...;` with no NULL check.",
            "arithmetic": "Steady state with B-frames: LastPic (ref) + NextPic (ref) + CurPic (B, released each FrameStart at src/SMS_MPEG.c:171-175) = exactly 3. Any number of CONSECUTIVE B-frames is fine because non-reference pictures are recycled every frame. A third simultaneous reference would return NULL and crash.",
            "converter_impact": "Consecutive B-frames are fine (max_b_frames can be 1-3). Do NOT enable B-pyramid / B-frames-as-references — the pool physically cannot hold a third reference and the failure is an unchecked NULL dereference."
          },
          {
            "claim": "m_LineStride is a uint16_t holding (MBW+2)*384, capping width at 2688 pixels. m_LineSize is likewise uint16_t. Neither is binding under the 1024 gate.",
            "kind": "HARD",
            "evidence": "include/SMS_MPEG.h:238-239 `uint16_t          m_LineSize;` / `uint16_t          m_LineStride;`; src/SMS_MPEGInit.c:69-70 `g_MPEGCtx.m_LineSize   = (  ( g_MPEGCtx.m_Width + 15 ) >> 4  ) + 2;` / `g_MPEGCtx.m_LineStride = g_MPEGCtx.m_LineSize * 384;`",
            "arithmetic": "(MBW+2)*384 <= 65535 -> MBW+2 <= 170 -> MBW <= 168 -> W <= 2688. At W=1024, MBW=64: 66*384 = 25,344, comfortably inside 16 bits.",
            "converter_impact": "No action; recorded to rule it out as the ceiling."
          },
          {
            "claim": "IPU CSC is issued in batches of at most 1023 macroblocks in both the software and hardware paths; this is a hardware register-field limit, not a resolution limit.",
            "kind": "HARD",
            "evidence": "Software path: src/SMS_VideoBuffer.c:52-55 `retVal -> m_CSCParam.m_nBlk[ 0 ] = lnBlk / 1023;` / `retVal -> m_CSCParam.m_nRem[ 0 ] = lnBlk % 1023;` / `retVal -> m_CSCParam.m_QWCB      = 1023 * ( 64 >> lShift );`. Hardware path: src/libmpeg_core.S:312,319 `addiu $t0, $zero, 1023` / `pminw $t0, $t0, $a2`.",
            "arithmetic": "At 1024x1024 the frame is 64*64 = 4,096 MBs -> 4 full batches of 1023 plus a remainder of 4. The IPU CSC MBC field is 11 bits, so 1023 is the maximum per command. No resolution ceiling follows.",
            "converter_impact": "No action."
          },
          {
            "claim": "THE FRAME BUFFER CARRIES EXACTLY ONE MACROBLOCK OF EDGE PADDING, and motion vectors are clamped to that padding in macroblock units — so MC never reads out of bounds UNLESS SMS_BUG_EDGE is active.",
            "kind": "HARD",
            "evidence": "Padding: src/SMS_VideoBuffer.c:33-37 `lMBW = (  ( aWidth  + 15 ) >> 4  ) + 2;` / `lMBH = (  ( aHeight + 15 ) >> 4  ) + 2;` / `lFrameSize   = lMBW * lMBH * sizeof ( SMS_MacroBlock );` / `lFrameStride = lMBW + 1;` and :61 `unsigned char* lpSrc = ( unsigned char* )( lpBase + lFrameStride );`. Edge fill: src/SMS_MPEG.c:220-405 `SMS_MPEG_FrameEnd` replicates the top row, bottom row, left and right columns and the four corners. Clamp: src/SMS_MPEG.c:441-469 `CLIP_MV()` / `CLIP_MVC()` against m_HEdgePos/m_VEdgePos.",
            "arithmetic": "Allocation is (MBW+2)*(MBH+2) macroblocks; m_pData starts at index MBW+3 (one full padded row plus one MB), so there is exactly one MB of border on every side. Last MB written is at index MBH*(MBW+2)+MBW, which is < (MBW+2)*(MBH+2). Clamp bounds are m_HEdgePos = MBW-1 and m_VEdgePos = MBH-1 in MB units (src/SMS_MPEGInit.c:33-34).",
            "converter_impact": "Only 16 pixels of edge extension exist. Motion vectors that point more than one macroblock outside the frame are clamped (correct per spec for unrestricted MV), so no converter action is needed — except to avoid the FourCCs that enable SMS_BUG_EDGE (see that finding)."
          },
          {
            "claim": "MPEG-4 B-frame timing fields m_PPTime / m_PBTime are uint16_t; if the vop_time_increment distance between successive non-B VOPs exceeds 65535 units the B-frame is dropped as SMS_FRAME_SKIPED.",
            "kind": "SOFT",
            "evidence": "include/SMS_MPEG.h:242-243 `uint16_t          m_PPTime;` / `uint16_t          m_PBTime;`; src/SMS_MPEG4.c:2553 `g_MPEGCtx.m_PPTime = ( uint16_t )(  g_MPEGCtx.m_Time - g_MPEGCtx.m_LastNonBTime  );`; :2562-2565 `if ( g_MPEGCtx.m_PPTime <= g_MPEGCtx.m_PBTime || g_MPEGCtx.m_PPTime <= g_MPEGCtx.m_PPTime - g_MPEGCtx.m_PBTime || g_MPEGCtx.m_PPTime <= 0 ) return SMS_FRAME_SKIPED;`",
            "arithmetic": "units_per_frame = vop_time_increment_resolution / fps. Overflow needs P-to-P distance * units_per_frame >= 65536. With resolution=30000 at 30 fps (1000 units/frame) that is 66 frames apart; with resolution=65535 at 25 fps (2621 units/frame) it is 25 frames apart. Normal max_b_frames of 1-3 is nowhere near it.",
            "converter_impact": "Keep vop_time_increment_resolution modest (e.g. equal to the frame rate, or 1000-30000) rather than 65535, and keep max_b_frames <= 3."
          },
          {
            "claim": "MPEG-1/2 packets are DMA'd whole to the IPU with a 16-bit QWC field, capping a single demuxed packet at 1,048,560 bytes.",
            "kind": "HARD",
            "evidence": "src/SMS_MPEG12.c:177-179 `DMA_SendA ( DMAC_TO_IPU, ( void* )(  ( unsigned int )lpPkt -> m_pData & 0x1FFFFFFF  ), ( lpPkt -> m_Size + 15 ) >> 4 );`; the QWC register field is 16 bits — include/SMS_DMA.h:20 `volatile unsigned int m_QWC  __attribute__(   (  aligned( 16 )  )   );` used with the 16-bit DMA tag QWC at include/SMS_DMA.h:54 `u64           QWC  : 16 __attribute__ (  ( packed )  );`",
            "arithmetic": "65535 QWC * 16 bytes = 1,048,560 bytes per transfer. MPEG PES packets are at most 65,541 bytes, so the demuxer never approaches this — but it is the hard ceiling on the MPEG-1/2 input chunk size.",
            "converter_impact": "No action for standard MPEG-PS. Do not construct oversized elementary-stream chunks."
          }
        ],
        "gaps": [
          "HEAP SIZE: nothing in the source sets an EE heap size (no linker script or _heap_size symbol found in Makefile or the tree). The exact number of bytes malloc can hand out on a 32 MiB console is therefore NOT source-confirmed. I gave exact per-resolution allocation totals instead; the 'runs out of memory at X' threshold depends on the unmeasured size of the program image, g_DataBuffer (include/SMS_Data.h, the packed IOP modules), GUI textures and audio-decoder state, none of which I audited.",
          "SOFT/PERFORMANCE CEILINGS: I found no code that measures or enforces decode throughput. I cannot say from source at what resolution/bitrate the software MPEG-4 decoder starts dropping frames on a 294 MHz EE. src/SMS_VSync.c:20 SMS_EstimateVSync measures GS upload cost only, not decode cost. Any statement like '640x480 MPEG-4 stutters' would be a guess — treat all performance guidance as unverified.",
          "MIPS DIVIDE-BY-ZERO SEMANTICS: for the MSMPEG4v3 `m_MBY % m_SliceHeight` case (src/SMS_MSMPEG4.c:2322) I confirmed the divisor can be 0, but whether gcc emits a trap-on-zero (teq) for that expression on this toolchain is not visible in the source. The arithmetic is confirmed; the exact failure mode (trap vs. undefined result) is not.",
          "4:2:2 / 4:4:4 MPEG-2 FAILURE MODE: confirmed there is no check on chroma_format, but I did not trace what the IPU BDEC actually does when handed non-4:2:0 data. Result could be garbage, a hang in _MPEG_WaitBDEC, or a silent misdecode — UNCONFIRMED which.",
          "DXSB SUBTITLE WIDTH BOUND: src/SMS_DXSB.c:274 writes lW bytes at 0x70001B30 with no visible bound on lpHdr->m_Width. Whether a large subtitle width can run past the 16 KiB scratchpad end (0x70004000, i.e. more than 9,424 bytes) depends on the _DXSB_Header field type and demuxer validation, which I did not audit.",
          "MOV/MP4 AND ASF CONTAINER DIMENSION SOURCES: I confirmed the width/height origin for MPEG-PS (src/SMS_ContainerMPEG_PS.c:301-302) but did not audit SMS_ContainerAVI.c / SMS_ContainerMOV.c / SMS_ContainerASF.c to confirm exactly which header field feeds m_Width/m_Height for those. The 1024 gate reads whatever they set.",
          "MPEG-4 LEVEL/PROFILE: no profile or level check exists anywhere in the MPEG-4 path (no equivalent of libmpeg's m_Profile/m_Level parsing). I found no code that would reject an ASP@L5 stream on profile grounds — the only gates are the 1024 dimension cap and the feature gaps listed above.",
          "INTRA-FRAME / GOP STRUCTURE: I found no code that limits GOP length or requires periodic keyframes for decoding correctness. Seek behavior (which may require keyframes) is in SMS_PlayerControl.c, which was outside this area's file list."
        ]
      },
      {
        "area": "Audio decoder ceilings (channels / sample rate / bitrate / SPU output path) — SMS PS2 media player",
        "findings": [
          {
            "claim": "The AC3 decoder can emit ONLY 1 or 2 output channels. The float-to-int converter is selected from a 2-entry function-pointer table indexed by (channels-1); there is no 3+ channel entry and no 3+ channel writer anywhere in the file. 5.1 AC3 CANNOT be passed through as 5.1 PCM.",
            "kind": "HARD",
            "evidence": "src/SMS_AC3.c:2576-2578 `static void ( *s_FloatToInt[ 2 ] ) ( sample_t*, int16_t* ) = {\\n  _ac3_float_to_int_1, _ac3_float_to_int_2\\n };`  and src/SMS_AC3.c:2585 `void ( *_ac3_float_to_int ) ( sample_t*, int16_t* ) = s_FloatToInt[ anChannels - 1 ];`",
            "arithmetic": "Table has 2 entries -> only anChannels==1 and anChannels==2 are in bounds. anChannels>=3 reads s_FloatToInt[2..] past the array and calls whatever pointer-sized garbage follows it.",
            "converter_impact": "FORBID any expectation of >2-channel AC3 PCM output. 'Keep original 5.1 AC3' is only meaningful in SPDIF passthrough mode (see the SPDIF finding); for internal decoding the converter must accept that the result is 2 channels."
          },
          {
            "claim": "5.1 AC3 IS decoded (all 5 full-bandwidth channels are parsed and inverse-transformed) and then DOWNMIXED to 2 channels using the Dolby-Surround matrix. The player forces the request to stereo before the decoder ever runs.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:104-106 `if (  !( g_Config.m_PlayerFlags & SMS_PF_SPDIF ) || ( g_Config.m_PlayerFlags & SMS_PF_PDW22 )  ) {\\n   if ( apStm -> m_pCodec -> m_Channels > 2 ) apStm -> m_pCodec -> m_Channels = 2;` ; src/SMS_AC3.c:2589-2590 `else if ( anChannels == 2 )\\n  lFlags = AC3_DOLBY;` ; the mix is done by src/SMS_AC3.c:2446-2449 `_ac3_downmix (\\n   lpSamples, s_AC3Ctx.m_ACMmod, s_AC3Ctx.m_Output, ... );` with the 3/2->2 kernel at src/SMS_AC3.c:1689 `_ac3_mix32toS`",
            "arithmetic": "",
            "converter_impact": "5.1 AC3 input is SAFE and plays — as Dolby-Surround-matrixed stereo. The converter may leave 5.1 AC3 untouched; it must not promise discrete 5.1 on output. Note the fold-down is matrixed (Lt/Rt), not a plain Lo/Ro sum."
          },
          {
            "claim": "The AC3 LFE channel is parsed and then THROWN AWAY on every stereo/mono decode. The AC3_LFE bit can never be set in the output flags because the caller passes only AC3_MONO or AC3_DOLBY.",
            "kind": "HARD",
            "evidence": "src/SMS_AC3.c:2587-2591 `if ( anChannels == 1 )\\n  lFlags = AC3_MONO;\\n else if ( anChannels == 2 )\\n  lFlags = AC3_DOLBY;` (neither constant contains AC3_LFE==16); src/SMS_AC3.c:581 `if ( s_AC3Ctx.m_LFEOn && ( *apFlags & AC3_LFE )  ) s_AC3Ctx.m_Output |= AC3_LFE;` ; src/SMS_AC3.c:2403 `} else _ac3_coeff_get ( lpSamples + 1280, &s_AC3Ctx.m_LFEExpBAP, &lQuant, 0, 0, 7 );` — decoded into scratch at s_Samples+1280 and never read again.",
            "arithmetic": "AC3_MONO=1, AC3_DOLBY=10 (include/SMS_AC3.h:24,33); AC3_LFE=16 (include/SMS_AC3.h:35). 1&16==0, 10&16==0.",
            "converter_impact": "The .1 is always discarded, silently, with no level compensation. If bass content matters, the converter should pre-fold LFE into L/R itself when transcoding rather than relying on SMS."
          },
          {
            "claim": "When SPDIF passthrough is enabled (and the PAL 22 speed-correction flag is off), the AC3 frame is NOT decoded at all — it is byte-swapped into a fixed 6144-byte IEC-61937 burst and handed to the IOP verbatim. This is the only path by which original 5.1 AC3 leaves the console intact.",
            "kind": "HARD",
            "evidence": "src/SMS_AC3.c:347-349 `if (  !( g_Config.m_PlayerFlags & SMS_PF_SPDIF )  )\\n  DecodeFrame = _ac3_decode;\\n else DecodeFrame = _ac3_spdif;` ; src/SMS_AC3.c:2620-2631 `static int _ac3_spdif ( SMS_RingBuffer* apRB, int anChannels, int aLen ) {\\n int retVal = 6144; ... (  ( uint16_t* )lpOutBuf  )[ 0 ] = 0xF872;` ; src/SMS_Player.c:108-109 `s_Player.m_Flags |= SMS_FLAGS_SPDIF;\\n   apStm -> m_pCodec -> m_Channels = 5;`",
            "arithmetic": "IEC-61937 burst = 6144 bytes = 1536 frames x 2 x 16-bit, matching one AC3 frame's 1536 samples. Padding: `memset ( lpOutBuf + 8 + aLen, 0, 6144 - 8 - aLen );` (src/SMS_AC3.c:2663). Largest legal AC3 frame is 3840 bytes (32 kHz @ 640 kbit/s), so 6144-8-3840 = 2296 bytes of pad — never negative.",
            "converter_impact": "Keeping original 5.1 AC3 works ONLY for users with SPDIF enabled. m_Channels=5 here is a marker, not a real channel count (_ac3_spdif ignores its anChannels argument). The converter should keep AC3 frames bit-exact and standard-sized if SPDIF users are a target."
          },
          {
            "claim": "AC3 accepted sample rates: 48000, 44100, 32000 Hz, plus half/quarter/eighth of those for bsid 9/10/11. fscod==3 (reserved) is rejected. bsid >= 12 (i.e. E-AC3 / AC3 Annex D, bsid 16) is rejected outright at sync.",
            "kind": "HARD",
            "evidence": "src/SMS_AC3.c:390 `if ( apBuf[ 0 ] != 0x0B || apBuf[ 1 ] != 0x77 || apBuf[ 5 ] >= 0x60 ) return 0;` ; src/SMS_AC3.c:404-410 `switch ( apBuf[ 4 ] & 0xC0 ) {\\n  case 0x00: *apSampleRate = 48000 >> lHalf; return 4 * lBitRate;\\n  case 0x40: *apSampleRate = 44100 >> lHalf; return 2 * (  320 * lBitRate / 147 + ( lFrmSizeCod & 1 )  );\\n  case 0x80: *apSampleRate = 32000 >> lHalf; return 6 * lBitRate;\\n }` ; src/SMS_AC3.c:177-179 `static uint8_t s_HalfRate[ 12 ] = { 0,0,0,0,0,0,0,0,0, 1, 2, 3 };`",
            "arithmetic": "apBuf[5] >= 0x60 means bsid = apBuf[5]>>3 >= 12 -> rejected. s_HalfRate is indexed by bsid (0..11); lHalf is 0 for bsid<=8, 1/2/3 for bsid 9/10/11 -> rates down to 48000>>3 = 6000 Hz. fscod==3 (0xC0) falls out of the switch and returns 0 -> no sync.",
            "converter_impact": "Emit AC3 at 48000/44100/32000 with bsid 8. FORBID E-AC3 / DD+ entirely — it will not even sync, the file behaves as if it has no audio."
          },
          {
            "claim": "AC3 bitrate is limited to the 19-entry standard table (32..640 kbit/s) and frmsizecod >= 38 is rejected. The 4096-byte input buffer has headroom over the largest legal frame.",
            "kind": "HARD",
            "evidence": "src/SMS_AC3.c:376-380 `static int s_Rate[ 19 ] = {\\n   32,  40,  48,  56,  64,  80,  96, 112,\\n  128, 160, 192, 224, 256, 320, 384, 448,\\n  512, 576, 640\\n };` ; src/SMS_AC3.c:399 `if (  ( lFrmSizeCod = apBuf[ 4 ] & 63 ) >= 38  ) return 0;` ; include/SMS_AC3.h:84 `uint8_t          m_InBuf[ 4096 ];`",
            "arithmetic": "Max frame size across all fscod at 640 kbit/s: 48 kHz -> 4*640 = 2560 B; 44.1 kHz -> 2*(320*640/147 + 1) = 2*(1393+1) = 2788 B; 32 kHz -> 6*640 = 3840 B. Max 3840 < 4096, i.e. 256 bytes of headroom. No overflow possible for a legal frame.",
            "converter_impact": "Any AC3 bitrate 32..640 kbit/s is safe. No clamp needed beyond the spec."
          },
          {
            "claim": "AC3 mono (acmod 1) is accepted and produces a genuine 1-channel PCM stream (256 int16 per block, 3072 bytes per frame).",
            "kind": "HARD",
            "evidence": "src/SMS_AC3.c:2587-2588 `if ( anChannels == 1 )\\n  lFlags = AC3_MONO;` ; the mono writer src/SMS_AC3.c:2496-2531 `_ac3_float_to_int_1:` loops `addiu $at, $zero, 16` times storing 4x`sd` (32 B) per iteration.",
            "arithmetic": "16 iterations x 32 bytes = 512 bytes = 256 int16 per block; 6 blocks = 1536 samples = 3072 bytes. Matches src/SMS_AC3.c:2602-2603 `anChannels += 7; retVal = 6 << ( anChannels + 1 );` -> 6 << 9 = 3072.",
            "converter_impact": "Mono AC3 is fine. Note the mono path is only taken if the CONTAINER reports 1 channel; a mono AC3 inside a container claiming 2 channels decodes as duplicated stereo."
          },
          {
            "claim": "There is NO guard against AC3 channel counts of 3..6 reaching the function-pointer table when the container reports 0 channels. Player.c clamps only values > 2, so a container that declares 0 channels lets AC3_Decode install the real stream count (up to 6) and index s_FloatToInt out of bounds.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:105 `if ( apStm -> m_pCodec -> m_Channels > 2 ) apStm -> m_pCodec -> m_Channels = 2;` (0 is not > 2, so it survives); src/SMS_AC3.c:2725-2727 `if ( apCtx -> m_Channels == 0 )\\n\\n      apCtx -> m_Channels = s_AC3Ctx.m_nChannels;` ; src/SMS_AC3.c:2721-2723 `s_AC3Ctx.m_nChannels  = g_AC3Channels[ s_AC3Ctx.m_Flags & 7 ];\\n\\n     if ( s_AC3Ctx.m_Flags & AC3_LFE ) ++s_AC3Ctx.m_nChannels;` ; src/SMS_AC3.c:2753 `if (   !(  retVal = DecodeFrame ( apOutput, apCtx -> m_Channels, lLen )  )   ) continue;`",
            "arithmetic": "g_AC3Channels = {2,1,2,3,3,4,4,5} (src/SMS_AC3.c:38); +1 for LFE -> 6 for 5.1. s_FloatToInt[6-1] = s_FloatToInt[5] is 5 pointers (20 bytes) past the end of a 2-entry table.",
            "converter_impact": "When writing AVI/ASF/MOV headers for an AC3 track, ALWAYS write a non-zero nChannels in the WAVEFORMATEX (src/SMS_Container.c:294 `apCodecCtx -> m_Channels = File_GetUShort ( apFileCtx );`). A 0 there is a crash, not a fallback."
          },
          {
            "claim": "The DTS decoder has the identical 2-entry converter table — DTS is also hard-limited to 1 or 2 output channels, and 5.1 DTS is downmixed.",
            "kind": "HARD",
            "evidence": "src/SMS_DTS.c:11556-11558 `static void ( *s_FloatToInt[ 2 ] ) ( short*, float* ) = {\\n  _dts_float_to_int_1, _dts_float_to_int_2\\n };` ; src/SMS_DTS.c:11565 `void ( *_dts_float_to_int ) ( short*, float* ) = s_FloatToInt[ anChannels - 1 ];` ; src/SMS_DTS.c:11493-11502 `int DTS_Channels ( int aFlags ) { if ( aFlags & DTS_LFE ) return 6; ... }` clamped by src/SMS_Player.c:105.",
            "arithmetic": "Output size src/SMS_DTS.c:11583-11584 `anChannels += 7; retVal = lnBlocks << ( anChannels + 1 );` -> stereo = lnBlocks << 10. sample_blocks max = (127+1)>>3 = 16 (src/SMS_DTS.c:10720), so max 16*1024 = 16384 bytes per frame.",
            "converter_impact": "Same as AC3: no discrete multichannel DTS output. DTS also has a SPDIF passthrough path (src/SMS_DTS.c:11639 `_dts_spdif`)."
          },
          {
            "claim": "MONO DTS PRODUCES NO AUDIO: _dts_float_to_int_1 is a stub that returns immediately without writing anything, so the ring-buffer block handed to the SPU is whatever was in memory.",
            "kind": "HARD",
            "evidence": "src/SMS_DTS.c:11511-11513 `\"_dts_float_to_int_1:\\\\n\\\\t\"\\n \"jr        $ra\\\\n\\\\t\"\\n \"nop\\\\n\\\\t\"` — the whole function body; it is reached via src/SMS_DTS.c:11565 when anChannels==1, and src/SMS_DTS.c:11585-11593 still allocates `retVal + 80` bytes and declares `*( int* )lpOutBuf = retVal;` before calling it.",
            "arithmetic": "anChannels==1 -> retVal = lnBlocks << 9 bytes of uninitialised ring buffer declared as valid PCM.",
            "converter_impact": "FORBID mono DTS. If a source has a 1-channel DTS track, the converter must transcode it (to AC3/MP3/stereo DTS) or the user gets noise/silence. This is the one clearly stubbed audio path in the tree."
          },
          {
            "claim": "DTS prim_channels is read as a 3-bit field +1 (range 1..8) with NO clamp, while every per-channel array in dts_state_t is sized DTS_PRIM_CHANNELS_MAX == 5. A DTS stream declaring 6, 7, or 8 primary channels overruns them.",
            "kind": "HARD",
            "evidence": "src/SMS_DTS.c:10756 `state -> prim_channels = bitstream_get ( state, 3 ) + 1;` followed immediately by src/SMS_DTS.c:10758-10759 `for ( i = 0; i < state -> prim_channels; ++i ) {\\n  state -> subband_activity[ i ] = bitstream_get ( state, 5 ) + 2;` ; src/SMS_DTS.c:26 `#define DTS_PRIM_CHANNELS_MAX  5` ; src/SMS_DTS.c:109 `unsigned char  subband_activity    [ DTS_PRIM_CHANNELS_MAX ];`",
            "arithmetic": "3-bit field -> 0..7, +1 -> 1..8. Arrays hold 5. Writes at i=5,6,7 run 3 elements past the end of subband_activity/vq_start_subband/joint_intensity/... and 3*32 = 96 elements past the end of scale_factor/bitalloc/prediction_mode. 5.1 DTS (amode 9) uses exactly 5 and is safe; 6.1/7.1 (amode 13/14/15, dts_channels[] = 7,7,8 at src/SMS_DTS.c:145-148) is not.",
            "converter_impact": "FORBID DTS with more than 5 primary channels (i.e. anything above 5.1 core). Cap DTS at 5.1 core, or transcode. There is no runtime rejection — it silently corrupts the decoder state struct."
          },
          {
            "claim": "DTS accepted sample rates are exactly {8000, 16000, 32000, 11025, 22050, 44100, 12000, 24000, 48000, 96000, 192000}; the five zero table entries (indices 0,4,5,9,10) are rejected at sync.",
            "kind": "HARD",
            "evidence": "src/SMS_DTS.c:10105-10110 `static const int sl_SampleRate[ 16 ] ... = {\\n  0x00000000, 0x00001F40, 0x00003E80, 0x00007D00,\\n  0x00000000, 0x00000000, 0x00002B11, 0x00005622,\\n  0x0000AC44, 0x00000000, 0x00000000, 0x00002EE0,\\n  0x00005DC0, 0x0000BB80, 0x00017700, 0x0002EE00\\n };` and src/SMS_DTS.c:10144 `if ( !*sample_rate ) return 0;`",
            "arithmetic": "0x1F40=8000, 0x3E80=16000, 0x7D00=32000, 0x2B11=11025, 0x5622=22050, 0xAC44=44100, 0x2EE0=12000, 0x5DC0=24000, 0xBB80=48000, 0x17700=96000, 0x2EE00=192000.",
            "converter_impact": "Use 48000 (or 44100) for DTS. Rates are passed straight to the SPU with no resampling on the EE, so 96000/192000 will be requested of the IOP verbatim."
          },
          {
            "claim": "DTS input buffer is 24576 bytes and the largest frame the sync code can report is ~18724 bytes, so no overflow.",
            "kind": "HARD",
            "evidence": "src/SMS_DTS.c:23 `#define BUFFER_SIZE 24576` ; src/SMS_DTS.c:118 `unsigned char  m_InBuf             [ BUFFER_SIZE ]` ; src/SMS_DTS.c:10130-10132 `frame_size = bitstream_get ( state, 14 ) + 1;\\n\\n if ( !state -> word_mode ) frame_size = frame_size * 8 / 14 * 2;`",
            "arithmetic": "14-bit field max 16383, +1 = 16384. 14-bit word mode: 16384*8/14*2 = 9362*2 = 18724. 18724 < 24576 -> 5852 bytes headroom.",
            "converter_impact": "No practical DTS frame-size limit to enforce."
          },
          {
            "claim": "The AAC decoder ALWAYS outputs 2 channels for any stream with 2 or more channels; >2 channels is handled by a fixed 5-in / 2-out downmix (pcm_synN) that reads exactly internal_channel[0..4]. There is no discrete multichannel AAC output.",
            "kind": "HARD",
            "evidence": "src/SMS_AAC.c:8197-8205 `output_channels = 2;\\n\\n if ( channels > 2 )\\n  hDecoder -> downMatrix = 1;\\n else if ( channels == 1 ) {\\n  if ( apCtx -> m_Channels == 2 )\\n   hDecoder -> upMatrix = 1;\\n  else output_channels = 1;\\n }` ; src/SMS_AAC.c:7951-7957 default branch fills `lppChan[ 0 ]`..`lppChan[ 4 ]` then `pcm_synN ( sample_buffer, lppChan, buffer_size );` ; the kernel at src/SMS_Utils.S:556-566 loads exactly five pointers (`lw $t5, 4($a1)` .. `lw $a1, 0($a1)`) and stores one interleaved stereo quad per iteration (`pinteh $t2, $t3, $t2` / `sq $t2, 0($a0)`).",
            "arithmetic": "lInfo.samples = 1024 * output_channels = 2048; output bytes = samples<<1 = 4096 (8192 with SBR doubling at src/SMS_AAC.c:8220-8221).",
            "converter_impact": "5.1 AAC also folds to stereo, using channels 0-4 (LFE dropped). Safe to feed, but do not expect multichannel."
          },
          {
            "claim": "AAC at sample-rate index 12 (7350 Hz) passes the raw-ADTS container probe but indexes two 12-entry tables out of bounds in the decoder, and get_sample_rate() returns 0 for it.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAAC.c:17-20 `static const int s_SampleRates[ 16 ] ... = {\\n 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,\\n 16000, 12000, 11025,  8000,  7350\\n };` (index 12 is non-zero so src/SMS_ContainerAAC.c:44 `if (  ( lSR = s_SampleRates[ lSR ] )  )` accepts it); src/SMS_AAC.c:5768 `if ( sr_index < 12 ) return sample_rates[ sr_index ];\\n return 0;` ; src/SMS_AAC.c:2006-2008 `static const uint8_t num_swb_1024_window[] ... = {\\n 41, 41, 47, 49, 49, 51, 47, 47, 43, 43, 43, 40\\n };` read at src/SMS_AAC.c:6204 `ics -> num_swb = num_swb_1024_window[ sf_index ];`",
            "arithmetic": "num_swb_1024_window and num_swb_128_window both have exactly 12 entries (0..11). adts.sf_index is a raw 4-bit read (src/SMS_AAC.c:5953) with no range check, and index 12 survives the container probe. Indices 13-15 are caught because s_SampleRates[13..15] are 0.",
            "converter_impact": "FORBID 7350 Hz AAC. Restrict AAC sample rates to the 12 valid values 96000..8000. (MP4/M4A is safe: src/SMS_AAC.c:6098 `if ( mp4ASC -> samplingFrequency == 0 ) return -2;` rejects index >= 12.)"
          },
          {
            "claim": "Raw ADTS AAC with channel_configuration > 6 is coerced to 2 at init, and 7/8-channel raw AAC is remapped rather than rejected.",
            "kind": "SOFT",
            "evidence": "src/SMS_AAC.c:6044 `*channels   = ( adts.channel_configuration > 6 ) ? 2 : adts.channel_configuration;` ; src/SMS_AAC.c:8189-8191 `if ( hDecoder -> channelConfiguration == 0 ) hDecoder -> channelConfiguration = channels;\\n  if ( channels == 8 ) hDecoder -> channelConfiguration = 7;\\n  if ( channels == 7 ) hDecoder -> channelConfiguration = 0;`",
            "arithmetic": "",
            "converter_impact": "7.1 AAC will not crash but the channel mapping fed to the 5-channel downmix is not the real layout. Prefer <= 6 channels; 2 is safest."
          },
          {
            "claim": "MPEG audio (MP1/MP2/MP3) is structurally 1 or 2 channels only — nChannels is derived directly from the mode field and can never exceed 2.",
            "kind": "HARD",
            "evidence": "src/SMS_MP123.c:5306 `apInfo -> m_nChannels  = apInfo -> m_Mode == SMS_MPA_MONO ? 1 : 2;` ; the decode buffers are sized to match, e.g. include/SMS_MP123.h:79 `} m_Ch[ 2 ];` and src/SMS_MP123.c:6407 `lnAlloc   = lnGran * g_MPACtx.m_Info.m_nChannels * 1152;`",
            "arithmetic": "MPEG-1 stereo Layer III: 2 granules * 2 ch * 1152 = 4608 bytes = 1152 frames x 4 bytes. Correct, no over/under-allocation.",
            "converter_impact": "Nothing to enforce; the format itself caps at 2. Mono is fully supported."
          },
          {
            "claim": "MPEG audio accepted sample rates: 44100/48000/32000 (MPEG-1), 22050/24000/16000 (MPEG-2 LSF), 11025/12000/8000 (MPEG-2.5). The reserved index 3 is rejected before decode.",
            "kind": "HARD",
            "evidence": "src/SMS_MP123.c:49 `const uint16_t g_mpa_freq_tab[ 3 ] = { 44100, 48000, 32000 };` ; src/SMS_MP123.c:5294 `lSampleRate     = g_mpa_freq_tab[ lSampleRateIdx ] >> ( apInfo -> m_LSF + lMPEG25 );` ; src/SMS_MP123.c:4795 `if (   (  ( aHdr >> 10 ) & 3   ) == 3          ) return 0;`",
            "arithmetic": "LSF+MPEG25 is 0 (MPEG-1), 1 (MPEG-2), or 2 (MPEG-2.5) -> right shift by 0/1/2.",
            "converter_impact": "All standard MPEG audio rates are fine. MPEG-2.5 is supported."
          },
          {
            "claim": "MPEG audio bitrates are the full standard tables (Layer I up to 448, Layer II up to 384, Layer III up to 320 kbit/s for MPEG-1); free-format is supported but the coded frame is hard-clamped to 1792 bytes.",
            "kind": "HARD",
            "evidence": "src/SMS_MP123.c:50-61 `const uint16_t g_mpa_bitrate_tab[ 2 ][ 3 ][ 15 ] = { { { 0, 32, 64, ... 448 }, { 0, 32, 48, ... 384 }, { 0, 32, 40, ... 320 } }, ... };` ; include/SMS_MP123.h:26 `#define SMS_MPA_MAX_CODED_FRAME_SIZE 1792` ; src/SMS_MP123.c:6684 `if ( g_MPACtx.m_Info.m_FrameSize > SMS_MPA_MAX_CODED_FRAME_SIZE ) g_MPACtx.m_Info.m_FrameSize = SMS_MPA_MAX_CODED_FRAME_SIZE;`",
            "arithmetic": "Largest legal frame: Layer II 384 kbit/s @ 32 kHz = 144000*384/32000 + 1 pad = 1729 B. Layer III 320 @ 32 kHz = 1440 B. Layer I 448 @ 32 kHz = (12000*448/32000 + 1)*4 = 676 B. All < 1792. A free-format frame larger than 1792 is TRUNCATED, not rejected -> corrupt output rather than a clean failure.",
            "converter_impact": "Any standard MP1/MP2/MP3 bitrate is safe. AVOID free-format MPEG audio; if the frame exceeds 1792 bytes it is silently truncated."
          },
          {
            "claim": "Ogg Vorbis files with MORE THAN 2 CHANNELS ARE REJECTED at container probe — the file will not open at all.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerOGG.c:70 `retVal = lInfo.channels < 3 ? 1 : -1;` combined with src/SMS_ContainerOGG.c:96 `if (   (  retVal = SMS_OGGVProbe ( lpFileCtx, &lInfo  )  ) > 0   ) {` — the -1 fails the `> 0` test and no stream is created.",
            "arithmetic": "",
            "converter_impact": "HARD CAP Vorbis at 2 channels. A 5.1 .ogg is an unplayable file, not a downmix."
          },
          {
            "claim": "The Vorbis decoder itself only ever writes 1 or 2 channels, and its output-size formula `lnSamples << lnChannels` is only correct for 1 and 2 channels — for 3+ it would declare 2^N bytes per sample while writing 4.",
            "kind": "HARD",
            "evidence": "src/SMS_OGG.c:5244-5246 `if ( s_OGGVCtx.m_Info.channels == 1 )\\n  s_OGGVCtx.PCMSyn = _pcm_syn1;\\n else s_OGGVCtx.PCMSyn = _pcm_syn2;` ; src/SMS_OGG.c:5492 `lnAlloc   = lnSamples << lnChannels;` ; src/SMS_OGG.c:5490 `if ( lnSamples > 1024 ) lnSamples = 1024;`",
            "arithmetic": "1 ch: lnSamples<<1 = 2 B/sample (correct). 2 ch: lnSamples<<2 = 4 B/frame (correct). 6 ch would be lnSamples<<6 = 64 B/frame declared vs 4 B written, and 1024<<6 = 65536 + 80 bytes requested from a 44672-byte ring buffer. Unreachable only because the container probe rejects >2 first.",
            "converter_impact": "Reinforces the 2-channel Vorbis cap: the guard in the container is the ONLY thing preventing a ring-buffer blowout."
          },
          {
            "claim": "FLAC files with MORE THAN 2 CHANNELS ARE REJECTED at container probe.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerFLAC.c:114 `if (  _do_probe ( lpFileCtx, &lData.m_Info, &lData.m_MinBlockSz ) && lData.m_Info.m_nChannels < 3 ) {` — otherwise SMS_GetContainerFLAC returns 0.",
            "arithmetic": "STREAMINFO channel field is 3 bits + 1 (src/SMS_ContainerFLAC.c:66 `apInfo -> m_nChannels  = SMS_GetBits ( &lCtx,  3 ) + 1;`), so 1..8 are representable; 3..8 are rejected.",
            "converter_impact": "HARD CAP FLAC at 2 channels. This guard is load-bearing: the decoder has only two channel buffers (see next finding)."
          },
          {
            "claim": "The FLAC decoder has exactly two channel buffers and a 2-entry synthesis dispatch table; >2 channels would write through an out-of-array pointer and index the table out of bounds.",
            "kind": "HARD",
            "evidence": "src/SMS_FLAC.c:40 `int32_t*       m_pDec[ 2 ];` ; src/SMS_FLAC.c:372 `for ( i = 0; i < decorrelation; ++i ) if (  !_decode_subframe ( apCtx, lpCtx, i )  ) return 0;` where `decorrelation = apCtx -> m_Data.m_Info.m_nChannels;` (src/SMS_FLAC.c:370) and _decode_subframe writes `apCtx -> m_pDec[ channel ][ i ]` ; src/SMS_FLAC.c:438-441 `static void ( *_synth_indep[ 2 ] ) ( short*, int32_t**, int, int ) ... = {\\n  _pcm_synth_1, _pcm_synth_2\\n };\\n _synth_indep[ apCtx -> m_Data.m_Info.m_nChannels - 1 ] (`",
            "arithmetic": "m_pDec has 2 entries; channel==2 reads the struct member immediately after it (m_nAlloc, an int) as a pointer and writes blocksize*4 bytes through it. _synth_indep[2] with nChannels==3 is 1 pointer past a 2-entry table.",
            "converter_impact": "Never rely on FLAC surviving >2 channels; the container gate is the only protection and it is per-file, so a >2ch FLAC embedded in another container would not be gated."
          },
          {
            "claim": "A FLAC file whose STREAMINFO max-frame-size field is ZERO hangs the audio decoder thread in an infinite loop.",
            "kind": "HARD",
            "evidence": "src/SMS_FLAC.c:488-500 `while ( lSize > 0 ) {\\n\\n  int i, lISize = min ( lSize, lpCtx -> m_Data.m_MaxFrameSz - lpCtx -> m_BSize );\\n ... memcpy ( &lpCtx -> m_pBS[ lpCtx -> m_BSize ], lpData, lISize );\\n  lpCtx -> m_BSize += lISize;\\n\\n  if ( lpCtx -> m_BSize == lpCtx -> m_Data.m_MaxFrameSz ) {` and the loop tail src/SMS_FLAC.c:575-577 `next:\\n  lSize  -= lISize;\\n  lpData += lISize;`",
            "arithmetic": "MaxFrameSz==0 and m_BSize==0 -> lISize = min(lSize, 0-0) = 0 -> lSize -= 0 each iteration, so `lSize > 0` never becomes false. Additionally src/SMS_FLAC.c:399 `realloc ( lpCtx -> m_pBS, 0 )` leaves a zero-length (possibly NULL) bitstream buffer that src/SMS_FLAC.c:504 dereferences.",
            "converter_impact": "MANDATORY: when producing FLAC, write a non-zero max-frame-size (and max-block-size) in STREAMINFO. Zero is legal per the FLAC spec ('unknown') and is what several encoders emit when streaming — SMS deadlocks on it."
          },
          {
            "claim": "FLAC bit depth above 24 produces a negative shift count (undefined behaviour); the per-frame sample-size table caps at 24 but the STREAMINFO value (5 bits + 1, i.e. up to 32) is used whenever the frame's sample_size_code is 0.",
            "kind": "HARD",
            "evidence": "src/SMS_FLAC.c:518 `int      lShift     = 24 - lpCtx -> m_Data.m_BPS;` used at src/SMS_FLAC.c:424 `*apSamples++ = ( lpInp[ i ] << aShift ) >> 8;` ; src/SMS_FLAC.c:300-302 `static const uint8_t sample_size_table[] ... = {\\n  0, 8, 12, 0, 16, 20, 24, 0\\n };` ; src/SMS_ContainerFLAC.c:70 `apBlockSz[ 4 ] = SMS_GetBits ( &lCtx, 5 ) + 1;`",
            "arithmetic": "BPS=16 -> lShift=8 -> (x<<8)>>8 = x (identity, correct). BPS=24 -> lShift=0 -> x>>8 (correct truncation to 16-bit). BPS=32 -> lShift = -8 -> `x << -8`, undefined.",
            "converter_impact": "CLAMP FLAC to 16-bit (ideally) or 24-bit. FORBID 32-bit FLAC."
          },
          {
            "claim": "FLAC accepted sample rates: STREAMINFO carries a 20-bit value used verbatim; per-frame codes select from {8000,16000,22050,24000,32000,44100,48000,96000} or an 8/16-bit escape. Nothing validates the value against anything the SPU can do.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerFLAC.c:65 `apInfo -> m_SampleRate = SMS_GetBits ( &lCtx, 20 );` ; src/SMS_FLAC.c:309-312 `static const int sample_rate_table[] ... = {\\n      0,     0,     0,     0, 8000, 16000, 22050, 24000,\\n  32000, 44100, 48000, 96000,    0,     0,     0,     0\\n };` ; src/SMS_FLAC.c:349-359 selects it, `else return 0;` for codes 1-3 and 15.",
            "arithmetic": "20-bit field -> up to 1048575 Hz can be declared and is passed straight through to SPU_InitContext.",
            "converter_impact": "Use 44100 or 48000. High rates are not rejected on the EE — they are handed to the IOP audio server as-is, so a 96 kHz+ FLAC is at the mercy of AUDSRV/SPU2 (unverifiable here)."
          },
          {
            "claim": "Mono is accepted by every codec except DTS (where it is a stub). AC3: yes. MP1/2/3: yes. AAC: yes (only when the container also reports != 2 channels). Vorbis: yes. FLAC: yes. PCM: yes (passthrough).",
            "kind": "HARD",
            "evidence": "AC3 src/SMS_AC3.c:2587-2588; MPA src/SMS_MP123.c:5306 and the mono synth branch src/SMS_MP123.c:6504-6506 `} else for ( lSS = 0; lSS < 18; ++lSS )\\n\\n   lpSamples += MP123_Synth (  sl_HybridOut[ 0 ][ lSS ], 0, ( void* )lpSamples, 1  );` ; AAC src/SMS_AAC.c:8201-8204 `else if ( channels == 1 ) {\\n  if ( apCtx -> m_Channels == 2 )\\n   hDecoder -> upMatrix = 1;\\n  else output_channels = 1;\\n }` ; Vorbis src/SMS_OGG.c:5244-5245; FLAC src/SMS_FLAC.c:438-441 `_pcm_synth_1`; DTS stub src/SMS_DTS.c:11511-11513.",
            "arithmetic": "",
            "converter_impact": "Mono is a safe target everywhere except DTS. For a mono source, output mono AC3/MP3/AAC/FLAC — never mono DTS."
          },
          {
            "claim": "THE SPU OUTPUT PATH DOES NOT ITSELF LIMIT CHANNELS OR RATE. The EE passes {frequency, 16 bits, channel count, volume, resample base, resample ratio} to the IOP audio RPC verbatim; the format contract is 16-bit interleaved and the true hardware ceiling lives in AUDSRV, which is a prebuilt 8989-byte blob with no source in this tree.",
            "kind": "HARD",
            "evidence": "src/SMS_SPU.c:304-320 `SPUContext* SPU_InitContext ( int anChannels, int aFreq, int aVolume, int aBase, int aRatio ) {\\n\\n SPU_Destroy ();\\n\\n s_Buffer[ 0 ] = aFreq;\\n s_Buffer[ 1 ] = 16;\\n s_Buffer[ 2 ] = anChannels;\\n s_Buffer[ 3 ] = aVolume;\\n s_Buffer[ 4 ] = aBase;\\n s_Buffer[ 5 ] = aRatio;\\n ... SifCallRpc ( &s_ClientDataA, 0, 0, s_Buffer, 32, s_Buffer, 4, NULL, NULL );` ; the 16-bit-interleaved contract is implied by src/SMS_SPU.c:323 `s_SPUCtx.m_BufTime = (  ( float )s_Buffer[ 0 ] * 1000.0F  ) / (  ( aFreq << 1 ) * anChannels  );` ; the blob is include/SMS_Data.h:26 `#define SMS_AUDSRV_SIZE      8989` loaded from src/SMS_IOP.c:179 `{ s_pAudSrv,  &g_DataBuffer[ SMS_AUDSRV_OFFSET   ], SMS_AUDSRV_SIZE,   0, NULL }`.",
            "arithmetic": "m_BufTime = bytes*1000 / (rate*2*channels) confirms bytes-per-frame = 2 * channels, i.e. signed 16-bit interleaved.",
            "converter_impact": "The channel/rate ceiling the converter must respect is the DECODER's, not the SPU's — every decoder above already caps at 2. Treat 'SPU output = 16-bit stereo interleaved at the codec's native rate' as the contract."
          },
          {
            "claim": "NOTHING IS RESAMPLED ON THE EE. The codec's native sample rate is handed to the audio server unchanged; the only rate adjustment is the PAL 25->23.976 speed correction, expressed as a base/ratio pair passed to the IOP, and it is 1/1 (no-op) unless the video is 23.98-25 fps PAL material.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:188-191 `s_Player.m_pSPUCtx = SPU_InitContext (\\n  s_Player.m_AudioChannels, s_Player.m_AudioSampleRate, SPU_Index2Volume ( g_Config.m_PlayerVolume ),\\n  s_Player.m_PDW22Base, s_Player.m_PDW22Ratio\\n );` ; defaults src/SMS_Player.c:1627-1628 `s_Player.m_PDW22Base  = 1;\\n s_Player.m_PDW22Ratio = 1;` ; set only at src/SMS_Player.c:1548-1558 `if ( lFPS > 23.8 && lFPS < 25.0F ) { ... s_Player.m_PDW22Ratio = lRate;\\n   s_Player.m_PDW22Base  = lBase * 25;`",
            "arithmetic": "",
            "converter_impact": "Choose a sample rate the SPU can actually play natively (48000/44100 are the safe choices). Do not assume SMS will resample an odd rate for you — it will not."
          },
          {
            "claim": "The decoded-audio ring buffer is 44672 bytes; every decoder's largest single allocation fits with room to spare, so no decoder can overrun it at its supported channel counts.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:286 `s_pAudioBuffer  = SMS_RingBufferInit (  UNCACHED_SEG( SMS_AUDIO_BUFFER ), SMS_AUDIO_BUFFER_SIZE  );` ; include/SMS_Data.h:55-56 `#define SMS_AUDIO_BUFFER      (   ( unsigned char* )(  ( unsigned int )( SMS_SYNTH_BUFFER + 4352 + 63 ) & ~63  )   )\\n#define SMS_AUDIO_BUFFER_SIZE ( &g_DataBuffer[ SMS_DATA_BUFFER_SIZE ] - SMS_AUDIO_BUFFER )`",
            "arithmetic": "Resolving the offset chain in include/SMS_Data.h:32-49: SMS_SMSUTILS_OFFSET = 217488, SMS_DATA_BUFFER_SIZE = 266560. AUDIO_BUFFER offset = (217488+4352+63) & ~63 = 221888. Size = 266560 - 221888 = 44672 bytes. Largest per-call allocations: DTS 16384+80, AAC(SBR) 8192+80, AC3/AC3-SPDIF 6144+80, MP2/MP3 4608+80, FLAC/PCM/Vorbis 4096+80.",
            "converter_impact": "No converter-side constraint, but it is the reason the >2-channel Vorbis path (lnSamples<<6 = 65616 bytes) would be fatal if the container gate were ever bypassed."
          },
          {
            "claim": "PCM16 is the ONLY audio path with no channel clamp anywhere — it is a straight memcpy and forwards whatever channel count the container declared (up to 8 for MPEG-PS LPCM) to the audio server.",
            "kind": "HARD",
            "evidence": "src/SMS_PCM.c:69 `_decode ( lpSamples, lpBuf, 4096 );` with `_Decode = memcpy` (src/SMS_PCM.c:34); src/SMS_ContainerMPEG_PS.c:423 `apStm -> m_pCodec -> m_Channels = 1 + ( lVal & 7 );` ; src/SMS_Player.c:100-114 `_check_ac3` clamps only SMS_CodecID_AC3, SMS_CodecID_DTS and SMS_CodecID_AAC — PCM is not mentioned.",
            "arithmetic": "3-bit field + 1 -> 1..8 channels forwarded unmodified to SPU_InitContext.",
            "converter_impact": "Do NOT emit >2-channel LPCM. It is the one format that will reach the IOP with a channel count above 2, and what happens there is not verifiable from this tree."
          },
          {
            "claim": "No audio codec tag is recognised without a working decoder. Every entry in the audio tag table maps to a real Open function; the only stubbed audio code path found is DTS mono.",
            "kind": "HARD",
            "evidence": "src/SMS_Codec.c:57-70 the tag table `{ SMS_CodecID_MP2, 0x00000050 }, { SMS_CodecID_MP3, 0x00000055 }, { SMS_CodecID_AC3, 0x00002000 }, { SMS_CodecID_DTS, 0x00002001 }, { SMS_CodecID_OGGV, 0x0000674F }, { SMS_CodecID_WMA_V1, 0x00000160 }, { SMS_CodecID_WMA_V2, 0x00000161 }, { SMS_CodecID_PCM16LE, 0x00000001 }, { SMS_CodecID_AAC, 0x6134706D }, { SMS_CodecID_AAC, 0x000000FF }, { SMS_CodecID_FLAC, 0x43614C66 }` ; src/SMS_Codec.c:103-115 every one of those IDs has a matching `SMS_Codec_*_Open ( apCtx )` call, and src/SMS_Codec.c:121 `return apCtx -> m_pCodec != NULL;` makes an unhandled ID a clean failure.",
            "arithmetic": "",
            "converter_impact": "Any of MP2/MP3/AC3/DTS/Vorbis/WMA1/WMA2/PCM16LE/AAC/FLAC is a valid target. The unhandled-default at src/SMS_Codec.c:117 `default: break;` returns 0 (stream not opened) rather than crashing, so an unknown tag degrades cleanly."
          }
        ],
        "gaps": [
          "THE IOP SIDE IS NOT AUDITABLE FROM THIS TREE. AUDSRV is shipped as an 8989-byte prebuilt blob inside g_DataBuffer (include/SMS_Data.h:26, src/SMS_IOP.c:160/179) — there is no C source for it under iop/ or irx/. So the ACTUAL SPU2 ceiling (max sample rate, whether >2 channels is even accepted, what the base/ratio resampler does, buffer sizing) is UNCONFIRMED. Everything I state about output is the EE-side contract only: 16-bit interleaved, channel count and frequency passed through unmodified.",
          "I could not determine what AUDSRV does with anChannels == 5, the marker value the player sets for AC3/DTS SPDIF passthrough (src/SMS_Player.c:109). It is clearly not a literal 5-channel PCM request since _ac3_spdif emits a 2-channel-shaped 6144-byte IEC-61937 burst, but the meaning of the 5 is in the blob.",
          "I did not verify the behaviour of SMS_RingBufferAlloc when a request exceeds the whole 44672-byte buffer (blocking, wrap corruption, or hang). It matters only for paths I showed to be unreachable (>2-channel Vorbis), so I did not chase it.",
          "WMA (SMS_WMA.c) was outside the requested file list; I only note that src/SMS_WMA.c:1242 `lpCtx -> nb_channels = apCtx -> m_Channels;` takes the container's count directly and that _check_ac3 does NOT clamp WMA. Its true channel ceiling is UNCONFIRMED.",
          "I did not trace the AVI/ASF/MOV/MP4 header parsers exhaustively, so the full set of containers that can hand a 0 or >2 channel count to the AC3/DTS decoders is not enumerated. I confirmed the mechanism (src/SMS_Container.c:294 reads nChannels straight from WAVEFORMATEX) and the one guard (src/SMS_Player.c:105, which only catches > 2).",
          "The DTS >5-primary-channel overflow (src/SMS_DTS.c:10756) is proven from the array sizes and the missing clamp, but I did not confirm by tracing a real 6.1/7.1 DTS bitstream that amode values 13-15 actually drive prim_channels above 5 in practice — the header field is independent of amode in this parser.",
          "Free-format MPEG audio frames larger than 1792 bytes are truncated rather than rejected (src/SMS_MP123.c:6684). I did not determine whether the resulting desync recovers or produces persistent garbage."
        ]
      },
      {
        "area": "SMS (PS2) player-wide and I/O limits — packet buffers, stream table, file-offset width, read path/bandwidth, index & duration",
        "findings": [
          {
            "claim": "MAXIMUM VIDEO PACKET PAYLOAD = 2,097,088 bytes (2 MiB - 64). The downstream tool's \"2 MiB video\" is the RING BUFFER capacity, not the packet ceiling — it is off by the 64-byte packet header. A packet of exactly 2 MiB (2,097,152) FAILS to allocate.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:54 `#define SMS_VP_BUFFER_SIZE ( 1024 * 1024 * 2 )`\nsrc/SMS_Player.c:1688 `s_pVPBuff = SMS_SyncMalloc ( SMS_VP_BUFFER_SIZE );`\nsrc/SMS_Player.c:1689-1695 `lpStm -> m_pPktBuf = lpVideoBuffer = SMS_RingBufferInit ( ... ), SMS_VP_BUFFER_SIZE );`\nsrc/SMS_Container.c:53 `SMS_AVPacket* lpPkt = aSize ? SMS_RingBufferAlloc ( apRB, aSize + 64 ) : &s_EmptyPacket;`\nsrc/SMS_Container.c:62 `lpPkt -> m_pData = (  ( unsigned char* )lpPkt  ) + 64;`\nsrc/SMS_RingBuffer.c:69 `void* retVal  = NULL;`\nsrc/SMS_RingBuffer.c:74 `aSize = ( aSize + 63 ) & 0xFFFFFFC0;`\nsrc/SMS_RingBuffer.c:76 `if ( aSize <= apRB -> m_Capacity ) {`  (no else — retVal stays NULL)",
            "arithmetic": "Capacity = 1024*1024*2 = 2,097,152. Allocation request for a payload of N bytes = ((N + 64) + 63) & ~63. Requirement: ((N+127) & ~63) <= 2097152.  N = 2097088: (2097088+127)&~63 = 2097215&~63 = 2097152 -> OK.  N = 2097089: (2097089+127)&~63 = 2097216&~63 = 2097216 > 2097152 -> SMS_RingBufferAlloc returns NULL.  Therefore max N = 2,097,152 - 64 = 2,097,088 bytes.",
            "converter_impact": "Assert/clamp single video packet (AVI chunk, MOV sample, ASF media object) <= 2,097,088 bytes, NOT 2,097,152. Recommend forbidding output above that value. Note the 2 MiB ring is SHARED with every DXSB subtitle stream (src/SMS_Player.c:1754 `s_Player.m_pCont -> m_pStm[ lSubStm[ i ] ] -> m_pPktBuf  = lpVideoBuffer;`), so subtitle packets consume the same pool."
          },
          {
            "claim": "MAXIMUM AUDIO PACKET PAYLOAD = 524,224 bytes (512 KiB - 64). Same 64-byte-header correction as video. One 512 KiB ring is shared by ALL audio streams in the file.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:55 `#define SMS_AP_BUFFER_SIZE ( 1024 *  512     )`\nsrc/SMS_Player.c:1722-1723 `s_pAPBuff = SMS_SyncMalloc ( SMS_AP_BUFFER_SIZE );` / `lpStm -> m_pPktBuf = lpAudioBuffer = SMS_RingBufferInit ( s_pAPBuff, SMS_AP_BUFFER_SIZE );`\nsrc/SMS_Player.c:1725-1726 `lpStm -> m_pPktBuf = lpAudioBuffer;` / `SMS_RingBufferAddRef ( lpAudioBuffer );`  (2nd..Nth audio stream share the SAME buffer)\nsrc/SMS_Container.c:53 `SMS_RingBufferAlloc ( apRB, aSize + 64 )`",
            "arithmetic": "Capacity = 1024*512 = 524,288. ((N+64)+63)&~63 <= 524288.  N = 524224: (524224+127)&~63 = 524288 -> OK.  N = 524225: -> 524352 > 524288 -> NULL. Max N = 524,288 - 64 = 524,224 bytes.",
            "converter_impact": "Clamp single audio packet <= 524,224 bytes. In practice no real AAC/AC3/MP3 frame approaches this (ADTS <= 8191, AC3 <= 3840), so this is only a guard against pathological muxing (e.g. one giant LPCM chunk)."
          },
          {
            "claim": "An oversized packet is NOT a graceful reject — SMS_RingBufferAlloc returns NULL and the demuxers diverge: AVI returns 0 (player spins in its read loop, no progress, no error), MOV/ASF return -1 (playback silently ends early), MPEG-PS does not NULL-check at all.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:824 `if (  !lpPkt || lpBuf -> Read ( lpBuf, lpPkt -> m_pData, lSize ) != lSize  ) return 0;`\nsrc/SMS_ContainerMOV.c:1068 `if ( !lpPkt ) return -1;`\nsrc/SMS_ContainerASF.c:584 `if ( !lpPkt ) break;`  (falls to `return -1;` at :629)\nsrc/SMS_ContainerMPEG_PS.c:504-505 `lpPkt = apCont -> AllocPacket ( lpStm -> m_pPktBuf, lPackLen );` / `memcpy (  lpPkt -> m_pData, lpMyCont -> m_Buff, lpMyCont -> m_BufLen );`  (no NULL check)\nsrc/SMS_Player.c:1367-1369 `lSize = s_Player.m_pCont -> ReadPacket ( s_Player.m_pCont, &lPktIdx );` / `if ( lSize <= 0 ) continue;`",
            "arithmetic": "",
            "converter_impact": "Treat the packet-size ceiling as a hard REJECT in the converter, not a warning: there is no in-player error path. The MPEG-PS NULL-deref is unreachable by size because PES lengths are a 16-bit BE field (src/SMS_ContainerMPEG_PS.c:172 `lLen = File_GetShortBE ( lpFileCtx );` -> lLen <= 65535)."
          },
          {
            "claim": "MAXIMUM STREAMS = 8, confirmed. SMS_MAX_STREAMS is 8 and the container's stream table is a fixed 8-entry array.",
            "kind": "HARD",
            "evidence": "include/SMS_Container.h:56 `#define SMS_MAX_STREAMS 8`\ninclude/SMS_Container.h:79 `SMS_Stream*          m_pStm[ SMS_MAX_STREAMS ];`",
            "arithmetic": "",
            "converter_impact": "Confirm the downstream assertion of 8. But see the MPEG-PS finding below — for that container the safe number is 7, and 9+ is an out-of-bounds WRITE."
          },
          {
            "claim": "MPEG-PS (.mpg/.vob) is the ONE container that does not enforce the 8-stream cap on the array write. A Program Stream with 9+ distinct elementary-stream IDs writes past m_pStm[7] into SMS_Container::m_Duration. Reachable with an ordinary DVD VOB (1 video + 8 audio tracks).",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:743 `apCont -> m_pStm[ apCont -> m_nStm++ ] = lpStm;`   (unguarded)\nsrc/SMS_ContainerMPEG_PS.c:762-764 `if (  apCont -> m_nStm < SMS_MAX_STREAMS && ( !lfAudio || !lfVideoParam )  )` / `continue;` / `else if ( lpFileCtx -> m_CurPos >= MAX_DETECT_SIZE ) break;`   -- neither branch taken at m_nStm==8, so the `while (  i++ < 1024 && !FILE_EOF( lpFileCtx )  )` loop (:649) iterates again and can add a 9th stream\ninclude/SMS_Container.h:79-80 `SMS_Stream*          m_pStm[ SMS_MAX_STREAMS ];` / ` int64_t              m_Duration;`   -- m_pStm[8] aliases the low 32 bits of m_Duration",
            "arithmetic": "m_pStm occupies bytes 0..31 of SMS_Container (8 pointers x 4 bytes on EE). m_Duration (int64_t) begins at byte 32. m_pStm[8] == the low word of m_Duration on little-endian EE.",
            "converter_impact": "For MPEG-PS output: FORBID more than 8 distinct elementary stream IDs. Counted IDs are video 0x1E0-0x1EF (only the first is kept), MPEG audio 0x1C0-0x1DF, AC3 0x80-0x87, DTS 0x88-0x8F/0x98-0x9F, LPCM 0xA0-0xAF (src/SMS_ContainerMPEG_PS.c:696-726). Subpicture 0x20-0x3F are ignored and do not count."
          },
          {
            "claim": "At EXACTLY 8 MPEG-PS streams the demuxer's NULL-terminated stream scans read m_pStm[8] (= m_Duration low word). If the file had no usable DTS, m_Duration is the sentinel 0x7FFFFFFFFFFFFFFF, so the scan dereferences the pointer 0xFFFFFFFF.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerMPEG_PS.c:486 `} else for (  lIt = lppStm; ( lpStm = *lIt ); ++lIt  ) if (  ( int )lpStm -> m_ID == lStartCode  ) break;`\nsrc/SMS_ContainerMPEG_PS.c:788 `apCont -> m_Duration = 0x7FFFFFFFFFFFFFFFLL;`\nsrc/SMS_ContainerMPEG_PS.c:826 `apCont -> m_Duration  = ( int )SMS_Rescale ( lDTS, SMS_TIME_BASE, 90000 );`",
            "arithmetic": "m_Duration = 0x7FFFFFFFFFFFFFFF -> low word (m_pStm[8]) = 0xFFFFFFFF, high word (m_pStm[9]) = 0x7FFFFFFF. The scan sees a non-NULL m_pStm[8] and reads ((SMS_Stream*)0xFFFFFFFF)->m_ID.",
            "converter_impact": "Cap MPEG-PS output at 7 elementary streams, not 8, to keep the terminator inside the array. Other containers (AVI/ASF/MOV) index by count, not by NULL terminator, and are safe at 8."
          },
          {
            "claim": "AVI with more than 8 streams is rejected outright at header parse (the whole file becomes \"unsupported\"), because avih's dwStreams pre-creates every stream.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:437 `lCount       = File_GetUInt ( lpFileCtx );      /* dwStreams          */`\nsrc/SMS_ContainerAVI.c:440-442 `for ( i = 0; i < lCount; ++i )` / `if (  !_NewStream ( apCtx )  ) goto error;`\nsrc/SMS_ContainerAVI.c:100 `if ( apCont -> m_nStm < SMS_MAX_STREAMS ) {`   (else _NewStream returns NULL)",
            "arithmetic": "",
            "converter_impact": "AVI: dwStreams must be <= 8. A 9-stream AVI does not degrade — it fails to open."
          },
          {
            "claim": "ASF with more than 8 streams destroys the container and reports failure; MOV/MP4 silently SKIPS every trak past the 8th.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerASF.c:203 `if (  apCont -> m_nStm >= SMS_MAX_STREAMS  ) goto fail;` -> src/SMS_ContainerASF.c:355-357 `fail:` / ` SMS_DestroyContainer ( apCont, 0 );` / ` return 0;`\nsrc/SMS_ContainerMOV.c:182-183 `if (  apCtx -> m_pBase -> m_nStm >= SMS_MAX_STREAMS  ) {` / `  File_Skip ( apFileCtx, ( uint32_t )apAtom -> m_Size );`",
            "arithmetic": "",
            "converter_impact": "MP4/MOV: traks beyond the 8th are dropped silently — put the video + intended audio traks FIRST in the moov, or just emit <= 8 traks. ASF: hard reject."
          },
          {
            "claim": "FILE SIZE: the smallest ceiling in the chain is 2 GiB - 1, NOT ~4 GiB. Every file position SMS sends to the IOP goes through the 32-bit-SIGNED lseek entry point; SMS never calls the 64-bit variant.",
            "kind": "HARD",
            "evidence": "src/SMS_FileContext.c:1318 `if ( lpPriv -> m_fXio ) aPos = fileXioLseek ( lpPriv -> m_FD, aPos, SEEK_SET );`\nsrc/SMS_FileContext.c:1321 ` aPos = fioLseek ( lpPriv -> m_FD, aPos, SEEK_SET );`\nsrc/SMS_FileContext.c:1479 `   fileXioLseek ( lpPriv -> m_FD, apCtx -> m_CurPos, SEEK_SET );`\nIOP-side ABI proven in-tree: iop/SMSUdpfs/udpfs/src/udpfs_ioman.c:233-236 `static int udpfs_lseek(iomanX_iop_file_t *f, int offset, int whence)` / `    return (int)udpfs_lseek64(f, (s64)offset, whence);`  -- the 32-bit slot sign-extends its `int offset`\niop/SMSUdpfs/udpfs/src/udpfs_ioman.c:216 `static s64 udpfs_lseek64(iomanX_iop_file_t *f, s64 offset, int whence)`  -- a 64-bit slot EXISTS and is registered (:419), but a repo-wide grep shows no `fileXioLseek64` call anywhere in src/\nLegacy ioman ABI, same width: iop/SMSSMB/src/SMSMB.c:225 `(  int ( * ) ( iop_io_file_t*, unsigned long, int               )  )_DrvLSeek,   /* io_lseek   */`",
            "arithmetic": "An `unsigned int` FileContext position >= 0x80000000 passed into an `int offset` parameter becomes negative (e.g. 0x80000000 -> -2147483648), then sign-extends to a negative s64 at the driver. Highest expressible positive offset = 0x7FFFFFFF = 2,147,483,647.",
            "converter_impact": "CORRECT the downstream ~4 GiB assertion to 2 GiB. Hard-cap output files at 2,147,483,647 bytes; recommend a practical cap of 2,000,000,000 to leave headroom. This is not a user-seek-only problem: AVI's own demux loop seeks on every skipped chunk (src/SMS_ContainerAVI.c:715 `lpFileCtx -> Seek ( lpFileCtx, lpFileCtx -> m_CurPos + lSize );`) and MOV seeks backwards between samples (src/SMS_ContainerMOV.c:1073 `else lpFileCtx -> Seek ( lpFileCtx, lpSample -> m_Pos );`), so a >2 GiB file breaks during ordinary playback."
          },
          {
            "claim": "The EE-side representation ceiling above the lseek layer is 4 GiB - 1 (all FileContext offsets/sizes are `unsigned int`), and file sizes >= 4 GiB WRAP silently rather than erroring.",
            "kind": "HARD",
            "evidence": "include/SMS_FileContext.h:66-69 ` unsigned int   m_Size;` / ` unsigned int   m_Pos;` / ` unsigned int   m_CurPos;`\ninclude/SMS_FileContext.h:80 ` int  ( *Seek    ) ( struct FileContext*, unsigned int                 );`\ninclude/SMS_FileContext.h:26 `#define FILE_EOF( f ) (  ( f ) -> m_CurPos >= ( f ) -> m_Size  )`\nsrc/SMS_FileContext.c:1652 `    retVal -> m_Size = lXio ? fileXioLseek ( lFD, 0, SEEK_END ) : fioLseek ( lFD, 0, SEEK_END );`",
            "arithmetic": "lseek's 32-bit result is assigned to `unsigned int m_Size`, which is a well-defined mod-2^32 conversion. For a 3 GiB file udpfs returns (int)3221225472 = -1073741824 and m_Size becomes the correct 3221225472. For a 5 GiB file: 5368709120 mod 2^32 = 1073741824 -> m_Size = 1 GiB, silently wrong; FILE_EOF then fires after 1 GiB.",
            "converter_impact": "Do not rely on any >4 GiB behaviour at all — it wraps, it does not fail. Combined with the finding above, the enforceable ceiling is 2 GiB."
          },
          {
            "claim": "Container-level index tables also store 32-bit file offsets, so even the theoretical 4 GiB is the absolute cap for seekable formats. MOV parses co64 into int64_t but truncates it to uint32_t when building the seek index.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:29-34 `typedef struct _AVIdxEntry {` / ` uint32_t m_Pos    __attribute__(  ( packed )  );` ...\nsrc/SMS_ContainerAVI.c:272-278 `if (   lBase >> 32 == ( lBase & 0xFFFFFFFF ) &&` / `       ( lBase & 0xFFFFFFFF ) < lFileSize      && lFileSize <= 0xFFFFFFFF`\nsrc/SMS_ContainerMOV.c:22-27 `typedef struct MOVIndex {` ... ` uint32_t m_Pos;`\nsrc/SMS_ContainerMOV.c:801 `static void _add_idx_entry ( MOVStream* apMyStm, int afKF, uint32_t aDTS, uint32_t anOffset, uint32_t aSize ) {`\nsrc/SMS_ContainerMOV.c:857 `    _add_idx_entry (  apMyStm, lKeyFrame, ( uint32_t )lCurDTS, ( uint32_t )lCurOffset, lSampleSize  );`\nsrc/SMS_ContainerMOV.c:538 `   for ( i = 0; i < lnEntries; ++i ) lpMyStm -> m_pChunkOffsets[ i ] = _read_long_be( apFileCtx );`  (co64 IS parsed, then truncated at :857/:911)",
            "arithmetic": "",
            "converter_impact": "Emitting co64 in an MP4 buys nothing — the index truncates to 32 bits. Keep every chunk offset < 2^31."
          },
          {
            "claim": "READ BANDWIDTH: on USB, MX4SIO, SMB and MMCE (every fileXio device except udpfs), reads are issued to the IOP in 4,096-byte pieces, and the streaming path is SYNCHRONOUS and SINGLE-BUFFERED — there is no read-ahead. udpfs alone uses 64 KiB pieces.",
            "kind": "HARD",
            "evidence": "src/SMS_FileContext.c:22 `#define MX4SIO_RD_CHUNK 4096`\nsrc/SMS_FileContext.c:16-21 comment: `/* mass: (MX4SIO/BDM) reads are issued to the device in chunks no larger than this. ... 4 KiB (one 8-sector driver batch) is the largest size confirmed stable on hardware -- 8/16/32 KiB single reads stall. */`\nsrc/SMS_FileContext.c:32 `#define UDPFS_RD_CHUNK  ( 64 * 1024 )`\nsrc/SMS_FileContext.c:1631-1632 `int lXio = ( aFileName != NULL && ( strncmp ( aFileName, \"mass\", 4 ) == 0 || strncmp ( aFileName, \"smb\", 3 ) == 0 || strncmp ( aFileName, \"mmce\", 4 ) == 0 ) ) || lUdpfs;` / ` int lRdChunk = lUdpfs ? UDPFS_RD_CHUNK : MX4SIO_RD_CHUNK;`\nsrc/SMS_FileContext.c:1353-1360 (STIO_Fill) `int _tot = 0, _want = apCtx -> m_BufSize, _n;` / `while ( _tot < _want ) { int _chunk = _want - _tot; if ( _chunk > lpPriv -> m_RdChunk ) _chunk = lpPriv -> m_RdChunk; _n = fileXioRead (...); ... }`\nsrc/SMS_FileContext.c:1476-1477 (STIO_Stream, fileXio branch) `   apCtx -> Seek         = STIO_Seek;` / `   apCtx -> Fill         = STIO_Fill;`   -- NOT the async STIO_FillStm\nContrast, non-fileXio (hdd0:/cdfs:/mc0:) path is async double-buffered: src/SMS_FileContext.c:1539-1540 `   fioSetBlockMode ( FIO_NOWAIT );` / `   fioRead ( lpPriv -> m_FD, apCtx -> m_pBuff[ 1 ], apCtx -> m_BufSize );`",
            "arithmetic": "During playback m_BufSize = 48 * 4096 = 196,608 bytes (see next finding). On mass:/smb:/mmce: that is 48 separate fileXioRead RPC round-trips per buffer refill, all blocking the demuxer thread.",
            "converter_impact": "Keep total muxed bitrate low enough that one blocking 192 KiB refill fits in the time the 2 MiB video + 512 KiB audio queues can cover. There is NO measured MB/s figure in the source to convert this into a bitrate number — treat it as SOFT guidance to prefer lower bitrates on USB/MX4SIO/SMB than on hdd0:."
          },
          {
            "claim": "Streaming read buffer is 196,608 bytes (192 KiB) at playback start, and jumps to 1,572,864 bytes (1.5 MiB) after the first seek / fast-forward / rewind.",
            "kind": "HARD",
            "evidence": "src/SMS_FileContext.c:1662 `    retVal -> m_StreamSize = 384;`\nsrc/SMS_FileContext.c:1461 `  apCtx -> m_BufSize = anBlocks * 4096;`\nsrc/SMS_Player.c:1064-1066 ` s_Player.m_pFileCtx -> Stream (` / `  s_Player.m_pFileCtx, s_Player.m_StartPos = s_Player.m_pFileCtx -> m_CurPos, s_Player.m_pFileCtx -> m_StreamSize >> 3` / ` );`\nsrc/SMS_PlayerControl.c:808 `  lpFileCtx -> Stream ( lpFileCtx, lFilePos, lpFileCtx -> m_StreamSize );`\nsrc/SMS_PlayerControl.c:1350 `    lpFileCtx -> Stream ( lpFileCtx, lpFileCtx -> m_CurPos, lpFileCtx -> m_StreamSize );`\n(CDDA differs: src/SMS_FileContext.c:1199 `     retVal -> m_StreamSize = 432;` with 2352-byte sectors)",
            "arithmetic": "Start-of-playback: (384 >> 3) * 4096 = 48 * 4096 = 196,608 bytes. Post-seek: 384 * 4096 = 1,572,864 bytes per buffer, and both m_pBuff[0] and m_pBuff[1] are reallocated to that size (src/SMS_FileContext.c:1468-1469), i.e. ~3 MiB of heap. CDDA: (432 >> 3) * 2352 = 54 * 2352 = 127,008 bytes.",
            "converter_impact": "Informational for the converter's bitrate budget: the player has ~192 KiB of file-level read-ahead before the first seek. Combined with the 2 MiB video packet queue this is the entire buffering budget against device latency."
          },
          {
            "claim": "Compressed-packet prefetch depth is bounded only by the ring capacities: the player pre-fills until a packet queue blocks, i.e. up to 2 MiB of compressed video + 512 KiB of compressed audio ahead of the decoder.",
            "kind": "SOFT",
            "evidence": "src/SMS_Player.c:345-351 ` while ( !lLock ) {` / `  if (   (  lSize = s_Player.m_pCont -> ReadPacket ( s_Player.m_pCont, &lStmIdx )  ) <= 0   ) break;` / `  SMS_RingBufferPost ( lpStms[ lStmIdx ] -> m_pPktBuf );` / ` }`\nsrc/SMS_Player.c:339-340 `   lpRB -> BlockCB         = _block_callback;` / `   lpRB -> m_pBlockCBParam = &lLock;`\nsrc/SMS_RingBuffer.c:80-90 `while ( aSize > apRB -> m_Size ) { if ( apRB -> BlockCB ) apRB -> BlockCB ( apRB ); ... }`",
            "arithmetic": "",
            "converter_impact": "Higher video bitrate directly shrinks the prefetch measured in TIME. Keeping keyframe/packet sizes modest keeps more packets in the same 2 MiB, which is what absorbs the synchronous 192 KiB device reads."
          },
          {
            "claim": "Decoded-frame display queue holds only 3 frames — the video output ring is a 192-byte array of 4-byte pointers rounded to 64-byte slots.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:277 `static unsigned char s_VideoBuffer[ 192 ] __attribute__(   (  aligned( 64 )  )   );`\nsrc/SMS_Player.c:285 `  s_pVideoBuffer  = SMS_RingBufferInit (  s_VideoBuffer, sizeof ( s_VideoBuffer )  );`\nsrc/SMS_MPEG12.c:203 `  SMS_FrameBuffer** lppOutput = ( SMS_FrameBuffer** )SMS_RingBufferAlloc ( apOutput, 4 );`\nsrc/SMS_RingBuffer.c:74 `aSize = ( aSize + 63 ) & 0xFFFFFFC0;`",
            "arithmetic": "Each 4-byte entry is rounded to (4+63)&~63 = 64 bytes. 192 / 64 = 3 slots.",
            "converter_impact": "SOFT-adjacent but HARD-sized: only 3 decoded frames buffer decode-time jitter. A stream with widely varying per-frame decode cost (large B-frame bursts) will drop frames rather than smooth out. Prefer even GOP/frame complexity."
          },
          {
            "claim": "PLAYER-WIDE RESOLUTION CAP: video wider or taller than 1024 is refused outright with STR_RESOLUTION_TOO_BIG before any decoding.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:1676-1681 `        if ( lpVideoCtx -> m_Width  > 1024 ||` / `             lpVideoCtx -> m_Height > 1024` / `        ) {` / `         s_Player.m_pErrorMsg = &STR_RESOLUTION_TOO_BIG;` / `         goto destroy;` / `        }  /* end if */`",
            "arithmetic": "",
            "converter_impact": "Reject/clamp any output with width > 1024 or height > 1024, regardless of codec. This is enforced in the player, not the codec, so it applies to every container."
          },
          {
            "claim": "NO limit on total duration. Duration is int64_t in milliseconds and the on-screen clock formats hours from an int.",
            "kind": "HARD",
            "evidence": "include/SMS.h:183 `#define SMS_TIME_BASE   1000`\ninclude/SMS_Container.h:80 ` int64_t              m_Duration;`\nsrc/SMS_PlayerControl.c:149-153 `void PlayerControl_FormatTime ( char* apBuf, uint64_t aTime ) {` / ` int lS = ( int )( aTime / SMS_TIME_BASE );` / ` int lM = lS / 60;` / ` int lH = lM / 60;`",
            "arithmetic": "int seconds overflows at 2^31 s ~= 68 years. Not a practical limit.",
            "converter_impact": "No duration assertion needed. (MPEG-PS is the exception noted separately: its m_Duration field is aliased by the 8-stream overflow.)"
          },
          {
            "claim": "NO fixed cap on seek-table / index SIZE — AVI idx1 and MOV sample indexes grow by realloc; the only bound is the ~32 MiB EE heap. AVI entries are 12 bytes, MOV entries 16 bytes.",
            "kind": "SOFT",
            "evidence": "src/SMS_ContainerAVI.c:29-34 `typedef struct _AVIdxEntry { uint32_t m_Pos ...; uint32_t m_CumLen ...; uint32_t m_fKey ...; }`  (12 bytes)\nsrc/SMS_ContainerAVI.c:185-192 `  lpIndices = SMS_Realloc (` / `   lpStm -> m_pIdx, ( unsigned int * )( &lpStm -> m_IdxAllocSize ),` / `   ( lpStm -> m_nIdx + 1 ) * sizeof ( _AVIdxEntry )` / `  );` / `  lpIndex = &lpIndices[ lpStm -> m_nIdx++ ];`\nsrc/SMS_ContainerMOV.c:22-27 `typedef struct MOVIndex { uint32_t m_Flags; uint32_t m_DTS; uint32_t m_Pos; uint32_t m_Size; }`  (16 bytes)\nsrc/SMS_ContainerMOV.c:802-806 same SMS_Realloc pattern\nsrc/SMS_EE.c:45-52 `void* SMS_Realloc ( void* apData, unsigned int* apSize, unsigned int aMinSize ) {` / ` if ( aMinSize < *apSize ) return apData;` / ` *apSize = 17 * aMinSize / 16 + 32;` / ` return realloc ( apData, *apSize );`\nEntry count for AVI comes straight from the idx1 chunk: src/SMS_ContainerAVI.c:128 ` int          i, lnIdx = aSize / 16;`",
            "arithmetic": "AVI: 12 bytes x entries, allocated with 17/16 slack -> ~12.75 bytes/entry. A 2-hour 25 fps video track = 180,000 entries ~= 2.3 MB; plus the audio track's chunk entries. MOV: 16 bytes x samples, ~17 bytes/entry with slack -> the same 180,000 samples ~= 3.1 MB. Neither SMS_Realloc nor its callers check for a NULL return.",
            "converter_impact": "Not a hard ceiling, but keep audio chunk granularity coarse: a per-frame-interleaved AVI audio track can double the index. Very long / very high-fps files with per-sample MP4 indexes are the risk case. There is no in-player guard — an allocation failure is an unchecked NULL."
          },
          {
            "claim": "OpenDML AVI: at most 8 RIFF/'movi' chunks are tracked. Content in a 9th RIFF chunk is unreachable by seek.",
            "kind": "HARD",
            "evidence": "src/SMS_ContainerAVI.c:62-64 ` uint32_t m_RiffEndList [ 8 ];` / ` uint32_t m_MoviEndList [ 8 ];` / ` uint32_t m_MoviListList[ 8 ];`\nsrc/SMS_ContainerAVI.c:336 ` lpCont -> m_nChunks           = 1;`\nsrc/SMS_ContainerAVI.c:344 ` for ( i = 1; i < 8; ++i ) {`\nsrc/SMS_ContainerAVI.c:1089-1103 ` for ( i = 0; i < lpCont -> m_nChunks; ++i ) {` ... ` return i != lpCont -> m_nChunks;`",
            "arithmetic": "Index 0 is the base RIFF, indices 1..7 come from the scan loop -> 8 chunks maximum.",
            "converter_impact": "If the converter emits OpenDML AVI, keep it to <= 8 RIFF chunks. At the conventional 1 GiB per RIFF chunk that is 8 GiB nominally, so the 2 GiB file ceiling binds first — but a converter that splits into small RIFF chunks could hit 8 well under 2 GiB."
          },
          {
            "claim": "Decoded-PCM output ring is 44,672 bytes, capping any single decoded audio frame at 44,592 bytes of PCM.",
            "kind": "HARD",
            "evidence": "src/SMS_Player.c:286 `  s_pAudioBuffer  = SMS_RingBufferInit (  UNCACHED_SEG( SMS_AUDIO_BUFFER ), SMS_AUDIO_BUFFER_SIZE  );`\ninclude/SMS_Data.h:54-56 `#define SMS_SYNTH_BUFFER      &g_DataBuffer[ SMS_SMSUTILS_OFFSET ]` / `#define SMS_AUDIO_BUFFER      (   ( unsigned char* )(  ( unsigned int )( SMS_SYNTH_BUFFER + 4352 + 63 ) & ~63  )   )` / `#define SMS_AUDIO_BUFFER_SIZE ( &g_DataBuffer[ SMS_DATA_BUFFER_SIZE ] - SMS_AUDIO_BUFFER )`\ninclude/SMS_Data.h:49 `#define SMS_DATA_BUFFER_SIZE  (  ( SMS_SIO2MAN_OFFSET    + SMS_SIO2MAN_SIZE    + 63 ) & ~63         )`\nDecoders allocate payload+80: src/SMS_AC3.c:2604 ` lpOutBuf    = ( short* )SMS_RingBufferAlloc ( apRB, retVal + 80 );`",
            "arithmetic": "Evaluating the offset chain in include/SMS_Data.h:14-49: SMSUTILS_OFFSET = 217488, SIO2MAN_OFFSET = 260720, so SMS_DATA_BUFFER_SIZE = (260720 + 5817 + 63) & ~63 = 266560. SMS_AUDIO_BUFFER offset = (217488 + 4352 + 63) & ~63 = 221888. SMS_AUDIO_BUFFER_SIZE = 266560 - 221888 = 44,672 bytes. Max decoded frame = 44672 - 80 = 44,592 bytes = 11,148 stereo 16-bit samples.",
            "converter_impact": "No practical constraint for normal codecs (AC3 = 1536 samples x 2ch x 2B = 6144 B; MP3 = 1152 samples = 4608 B). Only relevant if the converter emits exotic large-frame audio."
          }
        ],
        "gaps": [
          "Could not quote the prototypes of fileXioLseek / fioLseek / fileXioRead: ps2sdk is not vendored in this repo (Makefile:12 references $(PS2SDK)) and is not installed on this machine. The 32-bit-signed lseek conclusion is proven from the IOP side instead — iop/SMSUdpfs/udpfs/src/udpfs_ioman.c:233 registers `int offset` in the 32-bit slot and a separate s64 slot at :216 — plus the repo-wide absence of any fileXioLseek64 call. The EE-side prototype itself is inferred, not quoted.",
          "The filesystem drivers that actually service USB, MX4SIO, SMB and MMCE are PREBUILT BINARIES with no source in this tree: irx/bdmfs_fatfs.irx, irx/usbmass_bd.irx, irx/mx4sio_bd.irx, irx/smbman.irx, irx/bdm.irx. Their exact behaviour for a file >= 2 GiB (does lseek(SEEK_END) return a truncated 32-bit size, clamp, or return -1?) is UNCONFIRMED. Only udpfs (iop/SMSUdpfs, in-tree) could be traced end to end; for it the truncation happens to round-trip correctly up to 4 GiB via the unsigned assignment, while seeks are still capped at 2 GiB.",
          "No sustained-bitrate or MB/s figure exists anywhere in the source. The only throughput-relevant constants are MX4SIO_RD_CHUNK (4096) and UDPFS_RD_CHUNK (65536) in src/SMS_FileContext.c, and the SMAP 100 Mbps link (iop/SMSUdpfs/smap/src/smap.c:160). I cannot convert the 4 KiB chunking into a maximum playable bitrate from source alone. MX4SIO_SOLUTION.md:183 states 4 KiB is \"the empirically-confirmed reliable ceiling on hardware\" — that is repository documentation, not code, and I have not treated it as a source-confirmed fact.",
          "Whether a single packet at exactly the ring capacity (2,097,088 B video) actually completes rather than deadlocking was not empirically verified. Reading SMS_RingBufferAlloc/Free (src/SMS_RingBuffer.c:92-104 wrap logic and :120-124 tail restore), it appears to succeed only once the queue is fully drained and both pointers are back at m_pBeg — i.e. it stalls the whole pipeline first. Treat packets near the ceiling as functionally unusable even though they are within the HARD limit.",
          "Codec/decoder-level limits (IPU macroblock constraints, MPEG-4 GMC/quarter-pel, AAC object types, DTS frame sizes) were out of scope for this area and were not audited. The 1024x1024 resolution cap reported above is the player-level gate only; individual decoders may impose lower ones.",
          "The legacy in-tree drivers iop/SMSUMS (USB mass) and iop/SMSSMB (SMB) are present in the source tree but SMS_IOP.c loads the prebuilt usbmass_bd.irx (src/SMS_IOP.c:645) and smbman.irx (src/SMS_IOP.c:591) in BDM builds instead. I used SMSSMB/SMSCDVD only as evidence of the ioman lseek ABI width, not as the live code path.",
          "How the file browser obtains and displays sizes (src/SMS_FileDir.c, src/SMS_DirTree.c) was not fully traced, so I cannot say whether a >2 GiB file is even listed/selectable before STIO_InitFileContext runs."
        ]
      }
    ],
    "probeResults": [
      {
        "area": "Audio parameter combinations: which codec x bitrate x sample-rate x channel cells ffmpeg 8.1.1 actually accepts, versus which the Advanced Settings dropdowns can emit",
        "results": [
          {
            "scenario": "Full 504-cell sweep (6 codecs x 7 bitrates x 4 sample rates x 3 channel counts), encoder isolated from muxer by writing to Matroska, then re-run to the null muxer to capture ffmpeg's negotiated output line",
            "command": "python C:/Users/natha/AppData/Local/Temp/claude/.../scratchpad/grid.py   # each cell: ffmpeg -hide_banner -nostdin -y -f lavfi -i sine=frequency=440:duration=0.5:sample_rate=48000 -c:a <CODEC> -b:a <N>k -ar <SR> -ac <CH> -f matroska out.mka",
            "observed": "cells: 504 / OK=398  FAIL=106. Failures group into exactly five distinct root-cause messages (hex pointers normalised): ac3 21 cells 'Specified sample rate 22050 is not supported by the ac3 encoder'; libmp3lame 28 cells \"Specified channel layout '5.1' is not supported by the libmp3lame encoder\"; mp2 28 cells \"Specified channel layout '5.1' is not supported by the mp2 encoder\"; mp2 6 more cells 'Error while opening encoder - maybe incorrect parameters such as bit_rate, rate, width or height'; libvorbis 23 cells 'encoder setup failed'. aac and flac: 0/84 failures each.",
            "verdict": "WORKS"
          },
          {
            "scenario": "ac3 at 22050 Hz — every bitrate, every channel count",
            "command": "ffmpeg -hide_banner -nostdin -y -f lavfi -i sine=frequency=440:duration=0.5:sample_rate=48000 -c:a ac3 -b:a 192k -ar 22050 -ac 2 -f matroska out.mka",
            "observed": "exit 4294967274 (-22). '[ac3 @ ...] Specified sample rate 22050 is not supported by the ac3 encoder' then '[enc:ac3] Error while opening encoder - maybe incorrect parameters such as bit_rate, rate, width or height.' ... 'Conversion failed!'. All 21/21 cells at 22050 fail (7 bitrates x 3 channel counts); 14 of those are reachable from the dropdowns (mono+stereo). ac3 accepts ONLY 32000/44100/48000 — verified by an -ar sweep over 8000..192000 with no -b:a at all: OK=[32000, 44100, 48000], REJ=[8000, 11025, 12000, 16000, 22050, 24000, 64000, 88200, 96000, 176400, 192000]. Bitrate is irrelevant to this failure. 'AC-3 / Dolby Digital' is offered on the .avi, .mpg, .mp4 and .m4a presets, and '22050 Hz' is offered unconditionally, so this is directly selectable.",
            "verdict": "BROKEN",
            "root_cause": "tools/sms_converter/ui/advanced_settings.py:283 — the sample-rate combo is populated from a flat literal list `for sr in [48000, 44100, 32000, 22050]` with no reference to the selected codec; tools/sms_converter/ffmpeg_utils.py:393-395 then emits `-ar 22050` unconditionally, making it a hard constraint ffmpeg must honour or die."
          },
          {
            "scenario": "mp2 at 22050 Hz above 160 kbps",
            "command": "ffmpeg -hide_banner -nostdin -y -f lavfi -i sine=frequency=440:duration=0.5:sample_rate=48000 -c:a mp2 -b:a 192k -ar 22050 -ac 2 -f matroska out.mka",
            "observed": "exit -22. '[aost#0:0/mp2] [enc:mp2] Error while opening encoder - maybe incorrect parameters such as bit_rate, rate, width or height.' (mp2 gives no specific diagnostic — unlike ac3 it never names the offending parameter). Bisecting -b:a from 8k to 400k in 8k steps: mp2 at 48000/44100/32000 accepts 32k..384k; at 24000/22050/16000 accepts only 8k..160k. So the rule is the MPEG-2 LSF ceiling: below 32000 Hz, mp2 caps at 160 kbps. 192, 256 and 320 kbps are all offered in the bitrate combo, so 6 dropdown cells (3 bitrates x mono/stereo) are dead. mp2's legal rate set is [16000, 22050, 24000, 32000, 44100, 48000] — 22050 itself is fine, it is the bitrate pairing that fails.",
            "verdict": "BROKEN",
            "root_cause": "tools/sms_converter/ui/advanced_settings.py:269 — `for b in [64, 96, 128, 160, 192, 256, 320]` is a fixed list independent of the sample-rate combo at line 283; nothing recomputes the ceiling when the rate drops below 32 kHz."
          },
          {
            "scenario": "libvorbis — bitrate must fall inside a per-channel window that moves with sample rate",
            "command": "python .../phase3.py  # bisects -b:a 8k..500k in 8k steps for each (rate, channels): ffmpeg ... -c:a libvorbis -b:a <N>k -ar <SR> -ac <CH> -f null -",
            "observed": "'[libvorbis @ ...] encoder setup failed', exit -22, for 23/84 cells. libvorbis is the only encoder here with a MINIMUM as well as a maximum. Measured accepted windows: 48000 1ch 32k..240k, 2ch 48k..496k, 6ch 88k..496k; 44100 1ch 32k..240k, 2ch 48k..496k, 6ch 88k..496k; 32000 1ch 32k..184k, 2ch 40k..376k, 6ch 184k..496k; 22050 1ch 16k..88k, 2ch 32k..176k, 6ch 96k..496k; 16000 1ch 16k..96k, 2ch 24k..200k, 6ch 96k..496k. No gaps inside any window. 16 dropdown-reachable cells die, and they are counter-intuitive: MONO fails where stereo succeeds (mono 48000 @ 256k and @ 320k fail while stereo 48000 @ 320k is fine), because the constraint is per-channel. libvorbis's own rate set is unrestricted — an -ar sweep with no -b:a accepted all of 8000..192000; every libvorbis failure in the grid is a bitrate-window violation, not a rate violation.",
            "verdict": "BROKEN",
            "root_cause": "tools/sms_converter/ui/advanced_settings.py:267-300 — codec, bitrate, sample rate and channels are four QComboBoxes wired only to `_on_control_changed`; there is no handler that narrows one combo's contents from another's value. `_restrict_codec_choices` (line 354) is the only cross-filter in the file and it keys on the CONTAINER extension only."
          },
          {
            "scenario": "libmp3lame at 22050 Hz above 160 kbps — accepted, reported as honoured, silently written at 160",
            "command": "ffmpeg -hide_banner -nostdin -y -f lavfi -i sine=frequency=440:duration=10:sample_rate=48000 -c:a libmp3lame -b:a 320k -ar 22050 -ac 2 -f mp3 lame_22050_320.mp3   then   ffprobe -v error -select_streams a:0 -show_entries stream=bit_rate -of json lame_22050_320.mp3",
            "observed": "exit 0, and ffmpeg's own Output line claims '320 kb/s' — but ffprobe reports bit_rate=160000 and the 10-second file's size implies 161 kbps. Same at 192k and 256k, and at 24000 Hz and 16000 Hz. Verified across the matrix: 48000/44100/32000 honour 128/160/192/256/320 exactly; 24000, 22050 and 16000 all pin to 160k for any request above 160. This is the MPEG-2 LSF layer III ceiling. The dangerous part is that ffmpeg exits 0 AND misreports the value in its own progress output, so nothing short of probing the finished file detects it — the converter's own validator does exactly that (validator.py:249 reads `bit_rate` from ffprobe), so it will read back 160 kbps for a job the UI said was 320.",
            "verdict": "DEGRADED",
            "root_cause": "tools/sms_converter/ffmpeg_utils.py:389-391 — `if abitrate: cmd.extend([\"-b:a\", f\"{abitrate}k\"])` passes the combo value straight through with no per-codec ceiling; the pairing is created at ui/advanced_settings.py:269 and :283."
          },
          {
            "scenario": "aac — silent clamp at exactly 6 bits per sample per channel",
            "command": "ffmpeg -hide_banner -nostdin -y -f lavfi -i sine=...:duration=0.3 -c:a aac -b:a 640k -ar <SR> -ac <CH> -f null -   (asking absurdly high to expose the ceiling)",
            "observed": "aac never hard-fails anywhere in the 504-cell grid (0/84). It clamps instead, and the ceiling is exactly 6 * sample_rate * channels bits/s: 48000/1ch -> 288 kb/s (predicted 288, MATCH); 44100/1ch -> 264 (MATCH); 32000/1ch -> 192 (MATCH); 22050/1ch -> 132 (MATCH); 48000/2ch -> 576 (MATCH); 44100/2ch -> 529 (MATCH); 32000/2ch -> 384 (MATCH); 22050/2ch -> 264 (MATCH). 6ch cells hit ffmpeg's own 640k request cap before the formula binds. Nine dropdown-reachable cells are silently reduced: mono 48000@320->288, mono 44100@320->264, mono 32000@{256,320}->192, mono 22050@{160,192,256,320}->132, stereo 22050@320->264. Unlike libmp3lame, ffmpeg reports the clamped value honestly here.",
            "verdict": "DEGRADED",
            "root_cause": "tools/sms_converter/ui/advanced_settings.py:269 and :297-299 — the bitrate list is independent of both the rate combo and the channels combo, so 'Mono (1 ch)' halves the real ceiling with no feedback."
          },
          {
            "scenario": "True maximum channel count per encoder (-ac 1..16 sweep at 48000 Hz / 192k)",
            "command": "for n in 1..16: ffmpeg -hide_banner -nostdin -y -f lavfi -i sine=...:duration=0.3 -c:a <CODEC> -b:a 192k -ar 48000 -ac <n> -f null -",
            "observed": "libmp3lame accepts [1,2]; first rejection at -ac 3: \"Specified channel layout '2.1' is not supported by the libmp3lame encoder\". mp2 accepts [1,2]; first rejection at -ac 3: \"Specified channel layout '2.1' is not supported by the mp2 encoder\". ac3 accepts [1..6]; first rejection at -ac 7: \"Specified channel layout '6.1' is not supported by the ac3 encoder\". libvorbis accepts [1..6]; first rejection at -ac 7: 'encoder setup failed'. aac accepts [1..8]; -ac 9 fails with 'Task finished with error code: -22'. flac accepts [1..8]; same failure at 9. So: mp3/mp2 max 2, ac3/vorbis max 6, aac/flac max 8.",
            "verdict": "WORKS"
          },
          {
            "scenario": "Legal sample-rate set per encoder, measured with NO -b:a so bitrate cannot confound the result",
            "command": "for sr in 8000 11025 12000 16000 22050 24000 32000 44100 48000 64000 88200 96000 176400 192000: ffmpeg ... -c:a <CODEC> -ar $sr -ac 2 -f null -",
            "observed": "libmp3lame OK=[8000,11025,12000,16000,22050,24000,32000,44100,48000] REJ=[64000,88200,96000,176400,192000]. aac OK=[8000..96000] REJ=[176400,192000]. mp2 OK=[16000,22050,24000,32000,44100,48000] REJ=[8000,11025,12000,64000+]. ac3 OK=[32000,44100,48000] only. flac OK=all 14 tested, REJ=none. libvorbis OK=all 14 tested, REJ=none. Important correction to my own first pass: an earlier sweep run at a fixed -b:a 192k wrongly reported mp2 and libvorbis as rejecting 22050 — that was the bitrate constraint, not the rate constraint. Only ac3 genuinely rejects 22050.",
            "verdict": "WORKS"
          },
          {
            "scenario": "Does the container/muxer add any constraint beyond the encoder? 1512 cells across the seven output containers the presets use, restricted to the codecs Advanced Settings actually offers per container",
            "command": "python .../phase4.py  # each cell: ffmpeg ... -vn -c:a <CODEC> -b:a <N>k -ar <SR> -ac <CH> -f <avi|mpeg|mp4|mp3|flac|ogg> out<ext>",
            "observed": "Reachable codec sets computed from the app's own sms_supports(): .avi -> [libmp3lame, mp2, ac3, libvorbis]; .mpg -> [libmp3lame, mp2, ac3]; .mp4/.m4a -> [libmp3lame, aac, ac3, libvorbis]; .mp3 -> [libmp3lame]; .flac -> [flac]; .ogg -> [libvorbis]. Failure counts per container x codec are IDENTICAL to the bare-encoder counts in every case: .avi ac3 21/84, libmp3lame 28/84, libvorbis 23/84, mp2 34/84; .mpg same; .mp4 same; .mp3 libmp3lame 28/84; .flac 0/84; .ogg libvorbis 23/84. Not one failure is attributable to a muxer. (A first run showed .m4a+libmp3lame and .m4a+libvorbis failing 84/84 with 'Could not find tag for codec mp3 ... not currently supported in container' — but that was my error: I used -f ipod, while ffmpeg_utils.py:416 maps \".m4a\" to \"mp4\". Re-run with -f mp4: libmp3lame 28/84, libvorbis 23/84, aac 0/84, matching the encoder exactly.)",
            "verdict": "WORKS"
          },
          {
            "scenario": "Is the 6-channel column reachable from the UI at all, and what does 'Keep source layout' really do with a 5.1 source?",
            "command": "ffmpeg -hide_banner -nostdin -y -f lavfi -i sine=frequency=440:duration=1:sample_rate=48000 -af pan=5.1|c0=c0|c1=c0|c2=c0|c3=c0|c4=c0|c5=c0 -c:a pcm_s16le src51.wav   then   ffmpeg -hide_banner -nostdin -y -i src51.wav -vn -c:a libmp3lame -b:a 192k -ar 48000 -f avi ks.avi",
            "observed": "The Channels combo (advanced_settings.py:297-299) offers only Stereo=2, Mono=1 and Keep-source=None — there is no 6 entry, so `-ac 6` is never emitted and the 84 six-channel failures in the raw grid are NOT directly selectable. The Keep-source path (channels=None -> no -ac at all) behaves completely differently: source verified as '48000,6,5.1', and libmp3lame produced 'mp3,48000,2,192000' at exit 0 — ffmpeg auto-inserted a downmix to stereo rather than failing. Same for mp2 in .avi and .mpg ('mp2,48000,2,192000'). ac3, libvorbis, aac and flac all preserved 6 channels ('ac3,48000,6', 'vorbis,48000,6', 'aac,48000,6', 'flac,48000,6'). So 'Keep source layout' silently does not keep the layout for MP3 and MP2 — it downmixes 5.1 to stereo with no warning.",
            "verdict": "DEGRADED",
            "root_cause": "tools/sms_converter/ffmpeg_utils.py:397-399 — `if channels: cmd.extend([\"-ac\", str(channels)])`; when channels is None nothing is emitted and ffmpeg's automatic layout negotiation silently substitutes a supported layout, which for libmp3lame/mp2 means discarding four channels."
          },
          {
            "scenario": "'Keep source rate' (sample_rate=None -> no -ar) against sources at rates the encoder cannot take",
            "command": "ffmpeg -hide_banner -nostdin -y -i s22050.wav -vn -c:a mp2 -b:a 192k -ac 2 -f mpeg out.mpg   /   ... -c:a libvorbis ... -f ogg out.ogg   /   ... -c:a ac3 ... -f avi out.avi",
            "observed": "Split result. mp2 with a 22050 Hz source at 192k: exit -22, '[enc:mp2] Error while opening encoder - maybe incorrect parameters...' — a hard failure, and 192 kbps is precisely the MPEG-1/MPEG-2 presets' declared abitrate_kbps (config.py:98 and :113), so choosing 'Keep source rate' on an MPEG preset with a 22.05 kHz source kills the job. Same for an 11025 Hz source. libvorbis with a 22050 Hz source at 192k: exit -22, 'encoder setup failed'. ac3 behaves oppositely: with NO -ar, sources at 8000, 22050 and 24000 Hz all encoded at exit 0 and ffmpeg silently resampled every one of them to 32000 Hz ('Output ... Audio: ac3 ..., 32000 Hz, stereo, fltp, 192 kb/s', ffprobe confirms 'ac3,32000,2'). The identical sources with an EXPLICIT -ar of the same value die: 'Specified sample rate 8000 / 22050 / 24000 is not supported by the ac3 encoder', 'Conversion failed!'. This is the general mechanism behind every finding above: an explicit -ar/-ac is a hard constraint ffmpeg will fail on, while omitting it lets ffmpeg renegotiate silently.",
            "verdict": "BROKEN",
            "root_cause": "tools/sms_converter/ffmpeg_utils.py:393-399 — both -ar and -ac are emitted whenever the value is truthy, converting every dropdown selection into a constraint that ffmpeg must satisfy exactly or abort; there is no fallback to ffmpeg's own negotiation and no pre-flight check against the encoder's capabilities."
          },
          {
            "scenario": "flac — full 84-cell sweep including a meaningless -b:a",
            "command": "ffmpeg -hide_banner -nostdin -y -f lavfi -i sine=...:duration=0.5 -c:a flac -b:a <64..320>k -ar <22050..48000> -ac <1|2|6> -f matroska out.mka",
            "observed": "0/84 failures, and 0/84 through the real .flac muxer. flac accepted every sample rate tested from 8000 to 192000 with no rejections, and -ac 1..8. The stray -b:a is accepted and ignored (flac is lossless; ffmpeg echoes the requested number in its Output line but ffprobe reports bit_rate as N/A on the real file). The FLAC preset already declares abitrate_kbps=0 (config.py:185) so -b:a is normally omitted anyway (ffmpeg_utils.py:390), but a user who changes the bitrate combo away from 'Lossless / encoder default' will emit it harmlessly.",
            "verdict": "WORKS"
          },
          {
            "scenario": "aac — full 84-cell sweep, hard-failure check",
            "command": "ffmpeg -hide_banner -nostdin -y -f lavfi -i sine=...:duration=0.5 -c:a aac -b:a <64..320>k -ar <22050..48000> -ac <1|2|6> -f matroska out.mka",
            "observed": "0/84 failures — aac is the only lossy encoder here that never hard-fails on any offered combination, in any container (.mp4 0/84, .m4a 0/84). Its only misbehaviour is the silent 6-bits/sample/channel clamp documented above. Legal rates 8000..96000; max 8 channels.",
            "verdict": "WORKS"
          }
        ],
        "notes": [
          "SCOPE CORRECTION worth carrying forward: the brief asked for channels 1, 2 and 6, but the Channels combo (ui/advanced_settings.py:297-299) only offers Stereo=2, Mono=1 and 'Keep source layout'=None. There is no 6 entry, so `-ac 6` is never emitted. Of the 504 cells requested, 336 are dropdown-reachable. Within those 336: 36 hard failures, 15 silent bitrate substitutions, 285 clean.",
          "THE RULES, stated compactly. Sample-rate sets (measured with no -b:a): ac3 = {32000, 44100, 48000} ONLY; mp2 = {16000, 22050, 24000, 32000, 44100, 48000}; libmp3lame = {8000, 11025, 12000, 16000, 22050, 24000, 32000, 44100, 48000}; aac = 8000..96000; flac and libvorbis = unrestricted across 8000..192000. Max channels: libmp3lame 2, mp2 2, ac3 6, libvorbis 6, aac 8, flac 8. Bitrate ceilings: mp2 caps at 160 kbps below 32 kHz (384 kbps at and above it); libmp3lame caps at 160 kbps below 32 kHz but does so SILENTLY; aac clamps at exactly 6*rate*channels bits/s; ac3 has no practical ceiling in this range; libvorbis has a per-channel WINDOW with a floor as well as a ceiling.",
          "THE FOUR HEADLINE DEFECTS, in severity order. (1) ac3 + 22050 Hz is 14 selectable combinations that all die, with the clearest error message of any of them. (2) libvorbis has a per-channel bitrate window, so 16 combinations die and the pattern is inverted from intuition — MONO fails where stereo succeeds. (3) mp2 + 22050 Hz + {192, 256, 320} is 6 combinations that die with a generic message naming no parameter. (4) libmp3lame + 22050 Hz + {192, 256, 320} does not die, it lies: ffmpeg exits 0, prints '320 kb/s' in its own Output line, and writes 160.",
          "THE SINGLE MECHANISM BEHIND ALL OF IT: ffmpeg treats an explicit -ar/-ac as a hard constraint and aborts if the encoder cannot meet it, but auto-negotiates silently when the flag is absent. Proven side by side on identical input: `-i s8000.wav -c:a ac3 ... ` (no -ar) succeeded and resampled to 32000 Hz, while `-i s8000.wav -c:a ac3 -ar 8000 ...` failed with 'Specified sample rate 8000 is not supported by the ac3 encoder'. ffmpeg_utils.py:393-399 always emits both flags when the value is truthy, so every dropdown pick becomes a hard constraint.",
          "'Keep source' is not a safe escape hatch, it just changes the failure mode from loud to quiet. 'Keep source layout' + a 5.1 source + MP3 or MP2 silently downmixes to stereo (verified: source '48000,6,5.1' -> output 'mp3,48000,2'), which contradicts the label. 'Keep source rate' + a 22.05 kHz source + mp2 at the MPEG presets' own default of 192 kbps is a hard failure. Meanwhile 'Keep source rate' + ac3 silently resamples 8000/22050/24000 all up to 32000 Hz.",
          "The container layer is exonerated. Across 1512 cells over .avi/.mpg/.mp4/.m4a/.mp3/.flac/.ogg, every single failure was already an encoder failure — no muxer added a constraint. Note that I initially reported .m4a+libmp3lame and .m4a+libvorbis as failing 84/84 with 'Could not find tag for codec mp3 in stream #0'; that was my own test error (I used -f ipod), because ffmpeg_utils.py:416 maps '.m4a' to the 'mp4' muxer, which accepts both. Re-running with the app's actual muxer brought those in line with the encoder. Flagging it because 'ipod' is what ffmpeg would pick from the extension alone if the explicit -f were ever dropped.",
          "One asymmetry worth noting for any fix: aac and flac are the only two codecs that never hard-fail on any offered combination. If the goal is to make Advanced Settings incapable of producing a dead command, constraining bitrate and sample rate off the selected codec (rather than validating after the fact) would eliminate all 36 hard failures; the 15 silent substitutions need a separate warning path since ffmpeg exits 0 on those and, for libmp3lame specifically, misreports the result.",
          "Raw data and reproduction scripts are under C:/Users/natha/AppData/Local/Temp/claude/C--Users-natha-Github-Simple-Media-System-Simple-Media-System--claude-worktrees-sync-status-check-fb8014/bd14acea-cbf8-4209-8e53-ec19927777fa/scratchpad — grid.py (504-cell encoder sweep), phase2.py (negotiated-bitrate + channel/rate sweeps), phase3.py (window bisection), phase4.py (container sweep + true-bitrate check), phase5.py (keep-source paths), final.py (consolidated table); results in grid.json, phase2_grid.json, phase4.json. Nothing was written into the repo; the repo was read-only throughout.",
          "Environment: ffmpeg 8.1.1-full_build-www.gyan.dev (gcc 15.2.0, MSYS2), ffprobe 8.1.1, Windows 11. Exit code -22 (EINVAL) surfaces through the shell as 4294967274."
        ]
      },
      {
        "area": "sms_converter: audio-source -> video-target and video-source -> audio-target (ffmpeg 8.1.1-full_build-www.gyan.dev, libxvid present)",
        "results": [
          {
            "scenario": "Q1 -- VIDEO source + AUDIO preset (video-with-audio.mp4 -> MP3 Audio preset). Does '-map 0:v:0? -map 0:a:0? ... -vn' break the muxer in ffmpeg 8.1.1?",
            "command": "ffmpeg -y -i <SCRATCH>/in/video-with-audio.mp4 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -vn -c:a libmp3lame -b:a 192k -ar 48000 -ac 2 -f mp3 -progress pipe:1 -nostats <SCRATCH>/out/video-with-audio__mp3__AUD.mp3.partial",
            "observed": "exit=0. ffmpeg's own stream mapping proves -vn wins over the explicit -map:\n  Stream #0:0[0x1](und): Video: h264 ... 320x240\n  Stream #0:1[0x2](und): Audio: aac (LC) ... 44100 Hz, mono\n  Stream mapping:\n    Stream #0:1 -> #0:0 (aac (native) -> mp3 (libmp3lame))\n  Output #0, mp3, to '...': Stream #0:0: Audio: mp3, 48000 Hz, stereo, 192 kb/s\nOnly ONE mapping line -- the video stream selected by '-map 0:v:0?' is silently discarded, no warning, no error. Output: 121,581 bytes, format=mp3, 1 audio stream (mp3), duration 5.015521. App validator returns PASS.",
            "verdict": "WORKS"
          },
          {
            "scenario": "Q1 (breadth) -- VIDEO source + every other audio preset: AAC .m4a, FLAC .flac, Ogg .ogg",
            "command": "ffmpeg -y -i <SCRATCH>/in/video-with-audio.mp4 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -vn -c:a aac -b:a 192k -ar 48000 -ac 2 -f mp4 -movflags +faststart -progress pipe:1 -nostats <SCRATCH>/out/video-with-audio__m4a__AUD.m4a.partial\nffmpeg -y -i <SCRATCH>/in/video-with-audio.mp4 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -vn -c:a flac -f flac -progress pipe:1 -nostats <SCRATCH>/out/video-with-audio__flac__AUD.flac.partial\nffmpeg -y -i <SCRATCH>/in/video-with-audio.mp4 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -vn -c:a libvorbis -b:a 192k -ar 48000 -ac 2 -f ogg -progress pipe:1 -nostats <SCRATCH>/out/video-with-audio__ogg__AUD.ogg.partial",
            "observed": "All exit=0.\n  .m4a  -> 122,459 bytes, format=mov,mp4,m4a, 1 stream: aac, dur 5.015 -- validator PASS\n  .flac -> 256,311 bytes, format=flac, 1 stream: flac, dur 5.0155 -- validator PASS\n  .ogg  ->  26,958 bytes, format=ogg,  1 stream: vorbis, dur 5.0155 -- validator PASS\nNo video stream in any output. The FLAC preset correctly emits no -b:a / -ar / -ac (sample_rate=None, channels=None, abitrate=0 all flow through get_settings_dict as intended).",
            "verdict": "WORKS"
          },
          {
            "scenario": "Q1 (edge) -- SILENT video source + audio preset (video-silent.mp4 -> MP3 / AAC .m4a presets)",
            "command": "(no ffmpeg command is ever built -- ConversionWorker.run refuses first)",
            "observed": "worker.py:162 `if not self.settings.get(\"vcodec\") and not self._has_audio():` fires. ffmpeg is NEVER launched. failed_signal carries: \"This file has no audio track, so an audio-only preset has nothing to convert. Choose a video preset instead.\" Correct and deliberate -- this is the ONLY cross-type guard in the app, and it only covers the audio-target direction.",
            "verdict": "WORKS"
          },
          {
            "scenario": "Q2 -- AUDIO source + VIDEO preset (audio-only.mp3 -> Xvid-Compatible AVI preset). has_real_video_stream()==False so build_ffmpeg_cmd takes the -vn branch.",
            "command": "ffmpeg -y -i <SCRATCH>/in/audio-only.mp3 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -vn -c:a libmp3lame -b:a 128k -ar 48000 -ac 2 -f avi -progress pipe:1 -nostats <SCRATCH>/out/audio-only__avi__VID.avi.partial",
            "observed": "exit=0. File IS produced: 91,356 bytes. ffprobe: format_name=avi, duration=5.040000, streams = [{codec_type: audio, codec_name: mp3, nb_frames: 210}]. ZERO video streams. Note the command has NO -c:v, NO -b:v, NO -vf, NO -vtag at all -- the entire video block is skipped. The app's own validator returns PASS with no reasons and no warnings, so the worker promotes .partial to `audio-only_SMS.avi` and the UI reports success.\nThis is NOT what a user asking for a video file wants: they get an AVI/RIFF container holding only an MP3 track. It will show up in the SMS browser under video, and SMS's AVI reader will find no video FourCC. Nothing anywhere -- UI, worker, validator -- tells the user their \"video\" conversion produced no video.\nThe size estimate is also wrong: main_window._estimate_bytes gates the video bitrate on settings[\"vcodec\"] (the PRESET) rather than on whether the SOURCE has video, so it counts 1500 kbps of video that is never written. Measured: 5s clip estimated 1,032,762 bytes vs 91,356 actual = 11.3x overstatement; a 4-minute song is advertised as 47.3 MiB.",
            "root_cause": "ffmpeg_utils.py:302 `if vcodec and has_video_stream and vcodec != \"none\":` with the fallthrough at ffmpeg_utils.py:382-383 `else: cmd.append(\"-vn\")`. When has_video_stream is False (worker.py:266 passes self._has_video()), a VIDEO target silently degrades to an audio-only file with a video extension. Secondary: main_window.py:415 `vbitrate = settings.get(\"vbitrate_kbps\", 1500) if settings.get(\"vcodec\") else 0`. Tertiary: validator.py:117-278 has no check that a video-target container actually contains a video stream.",
            "verdict": "DEGRADED"
          },
          {
            "scenario": "Q2 (breadth) -- AUDIO source + the other video presets: MPEG-4 MP4, MPEG-2 .mpg, MPEG-1 .mpg",
            "command": "ffmpeg -y -i <SCRATCH>/in/audio-only.mp3 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -vn -c:a aac -b:a 128k -ar 48000 -ac 2 -f mp4 -movflags +faststart -progress pipe:1 -nostats <SCRATCH>/out/audio-only__mp4__VID.mp4.partial\nffmpeg -y -i <SCRATCH>/in/audio-only.mp3 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -vn -c:a mp2 -b:a 192k -ar 48000 -ac 2 -f mpeg -progress pipe:1 -nostats <SCRATCH>/out/audio-only__mpg__VID.mpg.partial",
            "observed": "All exit=0, all produce audio-only files with video extensions:\n  .mp4 -> 82,044 bytes, format=mov,mp4,m4a, 1 stream aac, dur 5.000 -- validator PASS\n  .mpg -> 122,880 bytes, format=mpeg, 1 stream mp2, dur 5.016 -- validator PASS (both MPEG-1 and MPEG-2 presets build the identical command here, since the whole video block is skipped)\nSame degradation across every video target. Identical results for audio-with-art.mp3 (91,356 / 82,044 / 122,880 -- byte-for-byte the same as the no-art source).",
            "verdict": "DEGRADED"
          },
          {
            "scenario": "Q3 -- Is attached_pic cover art correctly excluded? (audio-with-art.mp3, disposition.attached_pic=1 on stream #1 png, against all 8 presets)",
            "command": "ffmpeg -y -i <SCRATCH>/in/audio-with-art.mp3 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -vn -c:a aac -b:a 128k -ar 48000 -ac 2 -f mp4 -movflags +faststart -progress pipe:1 -nostats <SCRATCH>/out/audio-with-art__mp4__VID.mp4.partial\n(plus the avi / mpg / mp3 / m4a / flac / ogg equivalents)",
            "observed": "Cover art is excluded in every case. has_real_video_stream() returns False for audio-with-art.mp3 (the only video stream has disposition.attached_pic=1), so -vn is emitted and no video reaches the output. Outputs are BYTE-IDENTICAL to the no-cover-art source: avi 91,356 == 91,356; mp4 82,044 == 82,044; mpg 122,880 == 122,880; flac 264,154 == 264,154; ogg 26,737 == 26,737. No output has an attached_pic stream. All validator PASS.",
            "verdict": "WORKS"
          },
          {
            "scenario": "Q3 (control) -- prove the has_real_video_stream guard is load-bearing: force has_video_stream=True on the cover-art mp3, i.e. the pre-guard behaviour",
            "command": "ffmpeg -y -i <SCRATCH>/in/audio-with-art.mp3 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -c:v mpeg4 -b:v 1500k -r 30/1 -bf 0 -g 50 -vf scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=fps=30/1,format=yuv420p -c:a aac -b:a 128k -ar 48000 -ac 2 -f mp4 -movflags +faststart -progress pipe:1 -nostats <SCRATCH>/out/audio-with-art__mp4__VID__CTRL.mp4.partial",
            "observed": "MP4: exit=4294967274 (i.e. -22), zero-byte output. Verbatim:\n  [vf#0:0] No filtered frames for output stream, trying to initialize anyway.\n  [mp4] Could not find tag for codec mpeg4 in stream #0, codec not currently supported in container\n  [out#0/mp4] Could not write header (incorrect codec parameters ?): Invalid argument\n  [out#0/mp4] Nothing was written into output file, because at least one of its streams received no packets.\n  Conversion failed!\nAVI (same control, -c:v libxvid -vtag XVID -f avi): exit=0 but the video track is a GHOST --\n  Stream #0:0: Video: mpeg4 (XVID / 0x44495658), yuv420p, 320x240, 30 fps (attached pic)\n  [out#0/avi] video:0KiB audio:79KiB ... frame=    0\n  ffprobe -show_packets on v:0 -> 'video packets: 0 total bytes: 0'\n  ffmpeg -i ctrl.avi -f null -  ->  frame=    0\nSo pre-guard, cover art gave either a hard -22 failure (mp4) or an AVI advertising a 5.04s 320x240 video stream that decodes to zero frames (avi). The guard is doing real work; the exclusion documented at ffmpeg_utils.py:131-159 is accurate.",
            "verdict": "WORKS"
          },
          {
            "scenario": "Q4 recipe A -- generate a black video track from lavfi + the audio, with -shortest (works for ANY audio source, art or not)",
            "command": "ffmpeg -y -f lavfi -i \"color=c=black:s=640x480:r=30\" -i <SCRATCH>/in/audio-only.mp3 -map 0:v:0 -map 1:a:0 -sn -dn -map_metadata -1 -c:v libxvid -vtag XVID -b:v 1500k -bf 0 -g 50 -vf format=yuv420p -c:a libmp3lame -b:a 128k -ar 48000 -ac 2 -shortest -f avi <SCRATCH>/out/recipeA.avi",
            "observed": "exit=0, 134,090 bytes, encoded at 107x realtime. frame=150. ffprobe: format=avi dur=5.040000; v:0 mpeg4 tag=XVID 640x480 nb_frames=151 dur=5.033333; a:0 mp3 nb_frames=210 dur=5.040000. Full re-decode (`ffmpeg -i out/recipeA.avi -f null -`) yields frame=150 with no errors. App validator (validator.validate_converted_file) -> PASS, vcodec='MPEG-4 part 2', res=640x480, fps=30/1, streams=2, no warnings. -shortest is what stops the infinite lavfi source; without it the encode never ends.",
            "verdict": "WORKS"
          },
          {
            "scenario": "Q4 recipe B -- use the embedded cover art as the video track, single input, looped and re-timestamped (the 'music video' a user actually wants from an album-art MP3)",
            "command": "ffmpeg -y -i <SCRATCH>/in/audio-with-art.mp3 -filter_complex \"[0:v]scale=640:480:force_original_aspect_ratio=decrease,pad=640:480:(ow-iw)/2:(oh-ih)/2:black,loop=loop=-1:size=1:start=0,setpts=N/30/TB,format=yuv420p[v]\" -map \"[v]\" -map 0:a:0 -sn -dn -map_metadata -1 -c:v libxvid -vtag XVID -r 30 -b:v 1500k -bf 0 -g 50 -c:a libmp3lame -b:a 128k -ar 48000 -ac 2 -shortest -f avi <SCRATCH>/out/recipeB2.avi",
            "observed": "exit=0, 133 KiB, 0.227s wall (111x realtime). frame=150. ffprobe: v:0 mpeg4 tag=XVID 640x480 nb_frames=151; a:0 mp3 nb_frames=210; format dur=5.040000. Re-decode -> frame=150 clean. App validator -> PASS, streams=2, no warnings.\nIMPORTANT: the naive form WITHOUT setpts (loop -> scale -> fps=fps=30) also produces a correct file but runs at 0.117x realtime -- 42.69s of CPU for 5s of audio, because the attached_pic frame carries a 90k tbr and the fps filter has to walk it. `setpts=N/30/TB` after `loop` regenerates timestamps and is ~370x faster for identical output. Also note the filter must be `-filter_complex` with an explicit `-map \"[v]\"`; plain `-vf` on the attached_pic stream emits 'No filtered frames for output stream' and writes zero video packets.",
            "verdict": "WORKS"
          },
          {
            "scenario": "Q4 recipe C -- same black-source approach into the MP4 target (mpeg4, NO vtag, +faststart) to confirm it does not hit the 'Could not find tag for codec mpeg4' wall",
            "command": "ffmpeg -y -f lavfi -i \"color=c=black:s=640x480:r=30\" -i <SCRATCH>/in/audio-only.mp3 -map 0:v:0 -map 1:a:0 -sn -dn -map_metadata -1 -c:v mpeg4 -b:v 1500k -bf 0 -g 50 -vf format=yuv420p -c:a aac -b:a 128k -ar 48000 -ac 2 -shortest -f mp4 -movflags +faststart <SCRATCH>/out/recipeC.mp4",
            "observed": "exit=0, 127 KiB, 46.2x realtime, frame=150. ffprobe: v:0 mpeg4 tag=mp4v 640x480 nb_frames=150; a:0 aac tag=mp4a nb_frames=236; format dur=5.000000. Re-decode -> frame=150 clean. App validator -> PASS, streams=2, no warnings. Confirms the MP4 path is fine as long as -vtag XVID is absent (which build_ffmpeg_cmd already enforces at ffmpeg_utils.py:320-322).",
            "verdict": "WORKS"
          },
          {
            "scenario": "Control -- VIDEO source + VIDEO preset, all four video presets (the baseline that must keep working)",
            "command": "ffmpeg -y -i <SCRATCH>/in/video-with-audio.mp4 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -c:v libxvid -vtag XVID -b:v 1500k -r 30/1 -bf 0 -g 50 -vf scale=320:240:force_original_aspect_ratio=decrease,pad=320:240:(ow-iw)/2:(oh-ih)/2:black,fps=fps=30/1,format=yuv420p -c:a libmp3lame -b:a 128k -ar 48000 -ac 2 -f avi -progress pipe:1 -nostats <SCRATCH>/out/video-with-audio__avi__VID.avi.partial",
            "observed": "All exit=0 with both streams present.\n  avi -> 909,802 B: mpeg4 320x240 151 frames + mp3 -- validator PASS\n  mp4 -> 1,110,808 B: mpeg4 320x240 150 frames + aac -- validator PASS\n  mpg -> 1,290,240 B: mpeg2video 320x240 + mp2 -- validator PASS\n  video-silent.mp4 -> avi 819,856 B (video only, -map 0:a:0? matches nothing, no failure) and mp4 1,025,957 B -- both validator PASS\nresolve_video_encoder correctly picks libxvid for the AVI/XVID preset (this build has --enable-libxvid) and plain mpeg4 for the .mp4 presets, and -vtag XVID appears only on .avi.",
            "verdict": "WORKS"
          }
        ],
        "notes": [
          "BOTTOM LINE: only ONE of the two reported directions is actually broken. video-source -> audio-target works correctly in ffmpeg 8.1.1 across all four audio presets; audio-source -> video-target silently produces an audio-only file wearing a video extension and reports success.",
          "Q1 settled definitively: -vn DOES override an explicit -map 0:v:0? in ffmpeg 8.1.1. Proof is ffmpeg's own 'Stream mapping:' block, which lists only 'Stream #0:1 -> #0:0 (aac -> mp3)' for an input that has both streams. The -map selects candidates; -vn then filters them out before the muxer ever sees a video stream. No warning is printed and nothing is left dangling. The concern that the explicit map would 'override -vn and break the muxer' is not what happens.",
          "Q2 settled: the file IS produced (exit 0, non-zero size, promoted to its final name) and is NOT what the user wants. Concrete example: audio-only.mp3 + 'Xvid-Compatible AVI [RECOMMENDED]' -> audio-only_SMS.avi, 91,356 bytes, format_name=avi, exactly one stream (mp3 audio), zero video streams. Validator PASS, no warning. The generated command contains no -c:v, -b:v, -vf, -vtag or -g whatsoever -- the whole video block at ffmpeg_utils.py:302-380 is skipped.",
          "The asymmetry is the real defect: worker.py:162-168 has an explicit pre-flight guard for the OTHER direction ('This file has no audio track, so an audio-only preset has nothing to convert. Choose a video preset instead.'), which correctly stops video-silent.mp4 + MP3 preset before ffmpeg launches. There is no mirror-image guard for 'video preset, source has no real video'. Grep across the whole package confirms no such string exists.",
          "Third-order consequence: main_window.py:415 gates the video bitrate on settings['vcodec'] (a property of the PRESET) rather than on whether the SOURCE has video, so the queue advertises a size ~11.3x too large -- 47.3 MiB predicted for a 4-minute MP3 under the Xvid AVI preset.",
          "validator.py never checks that a video-target container came out containing a video stream. Adding that check would be the cheapest place to catch this, but it must be a WARN not a FAIL: validator FAIL deletes the output (worker.py:235 _cleanup_partial), and the audio is at least salvageable.",
          "The has_real_video_stream() guard (ffmpeg_utils.py:131-159) is correct and load-bearing -- the control runs reproduce exactly the failure its docstring describes: MP4 exits -22 with 'Could not find tag for codec mpeg4 in stream #0' and zero bytes, and AVI writes a video stream header with video:0KiB / 0 packets / frame=0. Do not remove it. The fix for Q2 belongs above it (build a real video source), not by reverting it.",
          "Recommended minimal fix shape for audio->video, all verified above: when settings has a vcodec but has_real_video_stream() is False, either (a) refuse with a message mirroring worker.py:162, or (b) synthesise a video track. For (b) the exact working forms are recipe A (`-f lavfi -i color=c=black:s=WxH:r=FPS` as input 0, real file as input 1, `-map 0:v:0 -map 1:a:0`, `-shortest`) and recipe B (single input, `-filter_complex \"[0:v]scale,pad,loop=loop=-1:size=1:start=0,setpts=N/30/TB,format=yuv420p[v]\" -map \"[v]\" -map 0:a:0 -shortest`). Both need `-shortest`; recipe B needs `-filter_complex` + explicit `-map \"[v]\"` (plain -vf on an attached_pic stream yields zero frames) and needs the `setpts=N/30/TB` after `loop` or it runs 370x slower.",
          "Test inputs and all 30+ outputs are under C:\\Users\\natha\\AppData\\Local\\Temp\\claude\\C--Users-natha-Github-Simple-Media-System-Simple-Media-System--claude-worktrees-sync-status-check-fb8014\\bd14acea-cbf8-4209-8e53-ec19927777fa\\scratchpad (in/, out/). The two driver scripts that import the converter's real build_ffmpeg_cmd / has_real_video_stream / validate_converted_file and replicate load_preset + get_settings_dict without Qt are scratchpad\\drive.py and scratchpad\\drive2.py; raw per-run data is scratchpad\\results.json. <SCRATCH> in the commands above stands for that scratchpad path. No file in the repo worktree was modified.",
          "Environment: ffmpeg 8.1.1-full_build-www.gyan.dev (gyan.dev, --enable-libxvid present, so resolve_video_encoder resolves mpeg4+XVID to libxvid), Python 3.14.4, Windows 11."
        ]
      },
      {
        "area": "Advanced Settings panel — every way it lets the user exceed / contradict what SMS (and ffmpeg) can do",
        "results": [
          {
            "scenario": "CONTROL INVENTORY (read, not run). Video: Codec combo (5 entries -> mpeg4 x2, mpeg2video, mpeg1video, msmpeg4v3, filtered per container); Width QSpinBox 2..1024 step 2; Height QSpinBox 2..1024 step 2; Preserve-aspect checkbox; Scaling mode combo (4); Allow-upscale checkbox; Frame rate combo (23.976/24/25/29.97/30/50/59.94/60); Video bitrate QSpinBox 100..10000 step 100; Deinterlace checkbox; Keyframe interval (GOP) QSpinBox 1..600; B-frames QSpinBox 0..4; Multi-pass checkbox + Passes QSpinBox locked 2..2; QPel checkbox; GMC checkbox. Audio: Codec combo (6 -> libmp3lame/aac/mp2/ac3/flac/libvorbis, filtered per container); Bitrate combo 64/96/128/160/192/256/320 + 'Lossless / encoder default'(0); Sample rate combo 48000/44100/32000/22050 + 'Keep source rate'(None); Channels combo 2/1/None; Normalize (loudnorm) checkbox. Only the two codec combos are constrained by anything (_restrict_codec_choices, called ONLY from load_preset). Every other control is fully independent of every other control and of the chosen codec.",
            "command": "Read C:/.../tools/sms_converter/ui/advanced_settings.py lines 82-311",
            "observed": "17 controls. Cross-control coupling exists in exactly one place: _restrict_codec_choices(preset.ext) filters the two codec combos against config.SMS_CONTAINER_CODECS. Nothing couples bitrate/sample-rate/channels to the audio codec, and nothing couples B-frames/QPel/GMC/GOP/fps/dimensions to the video codec.",
            "verdict": "DEGRADED",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/ui/advanced_settings.py:354 (_restrict_codec_choices is the only cross-control constraint, and it only covers codec x container)"
          },
          {
            "scenario": "User ticks the 'QPel (Quarter-Pixel)' checkbox and converts anything with video. This is the single worst hole: it does not produce a bad file, it makes ffmpeg refuse to start, on EVERY codec and EVERY container, because -qpel is not an ffmpeg option at all (qpel is a value of -flags, verified: `ffmpeg -h full | grep qpel` shows only 'qpel  E..V....... use 1/4-pel motion compensation' nested under flags).",
            "command": "ffmpeg -y -i src.mp4 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -c:v libxvid -vtag XVID -b:v 1500k -r 30/1 -bf 0 -g 50 -qpel 1 -vf scale=640:480:force_original_aspect_ratio=decrease,pad=640:480:(ow-iw)/2:(oh-ih)/2:black,fps=fps=30/1,format=yuv420p -c:a libmp3lame -b:a 128k -ar 48000 -ac 2 -f avi -progress pipe:1 -nostats out.avi.partial   (command generated verbatim by build_ffmpeg_cmd)",
            "observed": "EXIT 2880417800 (0xABAB...  = -1). stderr:\n  Unrecognized option 'qpel'.\n  Error splitting the argument list: Option not found\nNo file produced. The failure is in ffmpeg's ARGUMENT PARSER, before any codec is opened, so it is unconditional: reproduced identically in two-pass mode (pass 1 exit 2880417800, same message). The validator never runs; the user sees a raw ffmpeg parse error. The checkbox is enabled and clickable on every preset that has a video group.",
            "verdict": "BROKEN",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/ffmpeg_utils.py:370-371 — `if settings.get(\"qpel\", False): cmd.extend([\"-qpel\", \"1\"])`. Should be `-flags +qpel`."
          },
          {
            "scenario": "User picks the Xvid AVI preset, changes Codec to 'Microsoft MPEG-4 v3' (offered, because SMS_CONTAINER_CODECS['.avi']['video'] contains msmpeg4v3), and raises B-Frames above 0. B-frames 0..4 is offered unconditionally; msmpeg4v3 cannot encode them.",
            "command": "ffmpeg -y -i src.mp4 -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -c:v msmpeg4v3 -b:v 1500k -r 30/1 -bf 4 -g 50 -vf scale=640:480:force_original_aspect_ratio=decrease,pad=640:480:(ow-iw)/2:(oh-ih)/2:black,fps=fps=30/1,format=yuv420p -c:a libmp3lame -b:a 128k -ar 48000 -ac 2 -f avi -progress pipe:1 -nostats out.avi.partial",
            "observed": "EXIT 4294967274 (= -22, EINVAL). stderr:\n  [msmpeg4 @ ...] B-frames not supported by codec\n  [vost#0:0/msmpeg4 ...] Error while opening encoder - maybe incorrect parameters such as bit_rate, rate, width or height.\n  [out#0/avi @ ...] Nothing was written into output file, because at least one of its streams received no packets.\n  Conversion failed!\nZero-byte output, no file. Same failure at bframes=2 and in two-pass mode. B-frames 1..4 on mpeg4/libxvid/mpeg1video/mpeg2video all encode fine (verified bf=4 on mpeg1video -> exit 0, validator PASS), so the panel is right for 4 of 5 codecs and fatally wrong for the 5th.",
            "verdict": "BROKEN",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/ui/advanced_settings.py:181-182 — spin_bframes.setRange(0, 4) with no dependency on combo_vcodec; emitted unconditionally at ffmpeg_utils.py:352-353."
          },
          {
            "scenario": "Exhaustive audio matrix. Every (container, audio codec offered by _restrict_codec_choices, bitrate combo entry, sample-rate combo entry, channels combo entry) the panel can produce, driven through the app's own build_ffmpeg_cmd.",
            "command": "python C:/.../scratchpad/sweep_fast.py   (7 containers x offered codecs x [64,96,128,160,192,256,320,0] x [48000,44100,32000,22050,None] x [2,1,None]; each combo built by build_ffmpeg_cmd and executed)",
            "observed": "TOTAL COMBOS: 2160   HARD FFMPEG FAILURES: 246  (11.4%)\nThree distinct causes:\n(a) AC-3 @ 22050 Hz — 96 combos across .avi/.mpg/.mp4/.m4a. Every bitrate, every channel count. '[ac3 @ ...] Specified sample rate 22050 is not supported by the ac3 encoder / Supported sample rates: 48000 ...'\n(b) MP2 @ 22050 Hz at 192/256/320 kbps — 18 combos across .avi/.mpg. '[mp2 @ ...] bitrate 192 is not allowed in mp2' (MPEG-2 LSF Layer II caps at 160 kbps). 160k @ 22050 succeeds.\n(c) libvorbis with a bitrate too high for the (rate, channels) pair — 132 combos across .ogg/.avi/.mp4/.m4a. e.g. mono 320k @48000: '[libvorbis @ ...] encoder setup failed'. All of 256k and 320k mono fail at every rate; 192k fails at 32000/22050 mono; stereo 320k @22050 fails.\nZero failures for libmp3lame (.mp3, .avi, .mpg, .mp4, .m4a) and zero for flac (.flac) — those two are safe across all 40 combos each.\nIn every one of the 246 cases the job dies before any output exists, so the validator never speaks.",
            "verdict": "BROKEN",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/ui/advanced_settings.py:267-300 — combo_abitrate, combo_sample and combo_channels are populated with fixed lists and never re-filtered when combo_acodec changes."
          },
          {
            "scenario": "MP3 at the panel's top bitrate with the panel's lowest sample rate — the one audio combination that does NOT fail, but silently ignores the user.",
            "command": "ffmpeg -y -v error -f lavfi -i \"sine=d=1\" -c:a libmp3lame -b:a 320k -ar 22050 -ac 2 -f mp3 t.mp3 && ffprobe -v error -show_entries stream=sample_rate,bit_rate -of csv t.mp3",
            "observed": "exit 0; ffprobe: stream,22050,160000. The user asked for 320 kbps and got 160 kbps (MPEG-2 LSF Layer III ceiling). No ffmpeg warning, validator PASS, and the validator's own report line prints 'Audio bitrate: 160 kbps' — so the discrepancy is visible only if the user compares it against what they set.",
            "verdict": "DEGRADED",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/ui/advanced_settings.py:269-271 — bitrate list is codec- and rate-independent."
          },
          {
            "scenario": "User ticks 'GMC (Global Motion Comp)'. The tooltip implies it does something and recommends OFF for PS2 compatibility. It is only wired for one of the five codecs.",
            "command": "ffmpeg -y -i src.mp4 ... -c:v mpeg4 ... -gmc 1 ... -f mp4 -movflags +faststart ... out.mp4.partial   (and the same with -c:v mpeg2video -f mpeg)",
            "observed": "With the Xvid path (resolve_video_encoder swaps in libxvid): exit 0, GMC actually applied — libxvid is the only encoder in this build with a `-gmc` AVOption (`ffmpeg -h encoder=libxvid` lists `-gmc <int> ... use GMC`; `ffmpeg -h encoder=mpeg4` does not).\nWith native mpeg4 (.mp4 presets) and with mpeg2video: exit 0 but ffmpeg prints\n  [out#0/mp4 @ ...] Codec AVOption gmc (use GMC) has not been used for any stream. The most likely reason is either wrong type ... or that it is a private option of some decoder which was not actually used for any stream.\nThe file is produced, validator PASS, and the setting was silently discarded. So the checkbox is honest on 1 codec and a no-op on the other 4 — and the app never says which.",
            "verdict": "DEGRADED",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/ffmpeg_utils.py:373-374 — `-gmc 1` emitted for any vcodec; only libxvid consumes it."
          },
          {
            "scenario": "User picks the Xvid AVI preset and changes Audio Codec to 'Ogg Vorbis'. The panel offers it because config.SMS_CONTAINER_CODECS['.avi']['audio'] contains 'vorbis'. The resulting AVI carries an ffmpeg-specific wFormatTag that SMS's table does not list.",
            "command": "python -c \"from harness import run; run('vorbis_in_avi','.avi', acodec='libvorbis')\"  then  ffprobe -v error -show_entries stream=codec_type,codec_name,codec_tag,codec_tag_string -of csv=p=0 out_vorbis_in_avi.avi.partial",
            "observed": "ffmpeg exit 0. Validator: PASS, no warnings at all.\nffprobe: vorbis,audio,oV[0][0],0x566f\nSMS reads a 16-bit wFormatTag (src/SMS_Container.c:290 `File_GetUShort`) and looks it up in src/SMS_Codec.c:58-70, whose only Vorbis entry is `{ SMS_CodecID_OGGV, 0x0000674F }`. 0x566F is not in that table (the 16-bit-reachable entries are 0x0050, 0x0055, 0x2000, 0x2001, 0x674F, 0x0160, 0x0161, 0x0001, 0x00FF), so SMS_CodecGetID returns SMS_CodecID_NULL. By contrast AC-3 in AVI writes 0x2000 and MP3 writes 0x0055 — both present in the table — and msmpeg4v3 writes FourCC 'MP43', which IS in the AVI video table, so those three are genuinely fine.",
            "verdict": "BROKEN",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/config.py:268 — the '.avi' audio set lists 'vorbis' by codec NAME, but SMS matches by wFormatTag and ffmpeg's AVI muxer writes 0x566F, not the 0x674F the SMS table holds."
          },
          {
            "scenario": "User picks the MPEG-4 MP4 or AAC (.m4a) preset and changes Audio Codec to 'AC-3 / Dolby Digital'. Offered because SMS_CONTAINER_CODECS['.mp4'/'.m4a']['audio'] contains 'ac3'.",
            "command": "ffmpeg -y -v error -f lavfi -i \"sine=d=1\" -c:a ac3 -b:a 192k -ar 48000 -ac 2 -f mp4 t_ac3.m4a  then  python -c \"d=open('t_ac3.m4a','rb').read(); [print(t, t in d) for t in [b'ac-3',b'dac3',b'esds',b'mp4a']]\"",
            "observed": "exit 0. Box scan: b'ac-3' True, b'dac3' True, b'esds' False, b'mp4a' False.\nffmpeg writes AC-3 into MP4 as a bare 'ac-3' sample entry with a dac3 config box and NO esds. SMS's MOV reader identifies codecs exclusively through _mov_read_esds -> _mp4_get_id (src/SMS_ContainerMOV.c:359-408), keyed on the MPEG-4 objectTypeIndication, and its table maps AC3 to 226. Its atom dispatch table (src/SMS_ContainerMOV.c:701-722) has handlers for esds/stsd/wave/... and no 'ac-3' or 'dac3' entry, so there is nothing to read the codec id from. Validator: PASS, silent.\nFor contrast I checked the object-type ids ffmpeg actually writes for the other MP4 combos the panel offers, and they DO line up: MPEG-1 video -> 0x6A (106 = SMS_CodecID_MPEG1), MPEG-2 video -> 0x61 (97 = SMS_CodecID_MPEG2), MPEG-4 -> 0x20 (32), Vorbis -> 0xDD (221 = SMS_CodecID_OGGV), AAC -> 0x40 (64).",
            "verdict": "BROKEN",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/config.py:280,284 — 'ac3' listed for .mp4/.m4a on the strength of the esds object-type table, but ffmpeg does not emit an esds for AC-3 in MP4."
          },
          {
            "scenario": "User picks the FLAC preset (described as 'Keeps every bit of the original', sample_rate=None = 'Keep source rate') and ticks 'Normalize audio (Loudnorm)'. loudnorm's single-pass mode outputs 192 kHz, and with no -ar in the command there is nothing to bring it back down.",
            "command": "python -c \"...build_ffmpeg_cmd(... acodec='flac', abitrate_kbps=0, sample_rate=None, channels=None, normalize_audio=True)...\"  ->  ffmpeg -y -i src_flac.flac -map 0:v:0? -map 0:a:0? -sn -dn -map_metadata -1 -vn -c:a flac -filter:a loudnorm -f flac -progress pipe:1 -nostats o_flac_loudnorm.flac.partial",
            "observed": "exit 0. Source: 44100 Hz, 1 ch, 0.042 MiB. Output: 192000 Hz, 1 ch, 0.564 MiB — a 13.4x size increase and a resampled (no longer lossless) file, from a preset whose whole stated purpose is bit-exactness. Validator: PASS, zero warnings; it has no sample-rate check of any kind. Without normalize the same command gives 44100 Hz / 0.042 MiB, correctly preserved.\nThe panel's own Sample Rate tooltip says '48000 Hz ... is the hardware recommended baseline for PS2 audio output', and this path produces 192000 with the rate control left on 'Keep source rate'.",
            "verdict": "BROKEN",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/ffmpeg_utils.py:393-405 — `-ar` is skipped when sample_rate is None, but `-filter:a loudnorm` is still appended, so loudnorm's 192 kHz output rate becomes the file's rate."
          },
          {
            "scenario": "Width/height spin range 2..1024 against anything resembling a legal or sane frame size. The only rounding the panel does is odd->even (advanced_settings.py:496-499 and 544-547).",
            "command": "python -c \"from harness import run; run('dim_2x2_xvid','.avi',width=2,height=2,scaling_mode='Stretch',allow_upscale=True); run('dim_2x2_mpeg1','.mpg',width=2,height=2,...); run('4x1024_xvid','.avi',width=4,height=1024,...); run('dim_1024x1024_xvid','.avi',width=1024,height=1024,...); run('642x482_mpeg2','.mpg',width=642,height=482,...)\"",
            "observed": "ALL FIVE encode successfully and ALL FIVE are kept by the validator:\n  2x2 libxvid/.avi     -> exit 0, validator PASS (no warning whatsoever)\n  2x2 mpeg1video/.mpg  -> exit 0, validator PASS\n  4x1024 libxvid/.avi  -> exit 0, validator WARN (resolution only; ffmpeg note 'Invalid pixel aspect ratio 384/1, limit is 255/255 reducing')\n  1024x1024 libxvid    -> exit 0, validator WARN (resolution)\n  1024x1024 mpeg1video -> exit 0, validator WARN (resolution)\n  642x482 mpeg2video   -> exit 0, validator WARN (resolution)\nThere is no minimum-dimension check, no aspect-ratio sanity check, and no multiple-of-16 check anywhere — 642x482 MPEG-2 is accepted with only the generic 'above standard 640x480' note. The validator's hard resolution FAIL is `w > MAX_WIDTH or h > MAX_HEIGHT` i.e. >1024, which the spin boxes cannot reach: 1024x1024 lands exactly on the boundary and only WARNs.",
            "verdict": "DEGRADED",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/validator.py:210-216 — the FAIL threshold (>1024) is strictly outside the spin range (2..1024), so only the soft WARN branch is ever reachable from the panel."
          },
          {
            "scenario": "The FPS list applied to MPEG-1 vs MPEG-2 vs MPEG-4 — are 50 / 59.94 / 60 legal for each? Ran all eight combo entries against mpeg1video and mpeg2video in .mpg, plus 60 against libxvid in .avi.",
            "command": "python -c \"from harness import run; [run('fps'+f,'.mpg',fps=f,vcodec='mpeg1video',vtag=None,acodec='mp2',abitrate_kbps=192) for f in ['23.976','25','50','59.94','60']]\"  (and the mpeg2video / libxvid equivalents)",
            "observed": "All eight rates encode cleanly on both mpeg1video and mpeg2video — they are exactly the eight MPEG frame_rate_code values, so this is legal by spec and ffmpeg confirms it:\n  mpeg1 23.976 -> 24000/1001 PASS | 25 -> 25/1 PASS | 50 -> 50/1 WARN | 59.94 -> 60000/1001 WARN | 60 -> 60/1 WARN\n  mpeg2 23.976 -> PASS | 50 -> WARN | 59.94 -> WARN | 60 -> WARN\n  libxvid 60 -> WARN\nThe only complaint at 50/59.94/60 is 'High frame rate (60.00 fps) may impact smooth playback.' — a WARN, file kept.\nIMPORTANT: the validator's MPEG-1/2 whitelist (validator.py:229) is [23.976, 24.0, 25.0, 29.97, 30.0, 50.0, 59.94, 60.0], which is byte-for-byte the key set of MPEG_ALLOWED_FPS_MAP (config.py:46-55) that populates the combo. The 'Invalid frame rate for MPEG-1/2' FAIL therefore cannot fire on anything the panel can produce. This is not a hole — it is dead code.",
            "verdict": "WORKS",
            "root_cause": ""
          },
          {
            "scenario": "Video bitrate range 100..10000 kbps, plus GOP 1..600 extremes, plus the packet-size ceilings.",
            "command": "python -c \"from harness import run; run('vbr_100_xvid','.avi',vbitrate_kbps=100); run('vbr_10000_xvid','.avi',vbitrate_kbps=10000); run('vbr_10000_gop1_1024_mpeg2','.mpg',vbitrate_kbps=10000,gop=1,width=1024,height=1024,scaling_mode='Stretch',allow_upscale=True,fps='23.976',vcodec='mpeg2video',...); run('gop1_xvid','.avi',gop=1); run('gop600_xvid','.avi',gop=600)\"",
            "observed": "100 kbps -> PASS. 10000 kbps libxvid -> WARN 'High video bitrate (4360 kbps)' (libxvid's own rate control caps it well under the request). GOP 1 -> PASS, GOP 600 -> PASS, both silent.\nThe deliberate worst case the panel can construct (1024x1024, 10000 kbps, GOP 1, 23.976 fps, MPEG-2) -> exit 0, validator WARN x2 (resolution, and 'High video bitrate (10286 kbps)'), file KEPT.\nLargest video packet observed across every test in this session: 81,689 bytes, against MAX_VIDEO_PACKET_SIZE = 2,097,088 — 25x under. Largest audio packet observed: 576 bytes, against MAX_AUDIO_PACKET_SIZE = 524,224 — 900x under. The two packet-size FAILs are unreachable from the panel: 10000 kbps / 23.976 fps is ~52 KB per frame even before rate control.",
            "verdict": "DEGRADED",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/validator.py:239-241 — >3000 kbps is a WARN and there is no upper FAIL, so the top 7000 kbps of the spin range produces kept files."
          },
          {
            "scenario": "Can the validator's container-aware codec WARN (validator.py:191 and :255) ever fire on something the panel produced? Enumerated every (preset x offered vcodec x offered acodec) triple the panel can reach, using the panel's own filtering logic.",
            "command": "python -c \"from config import PRESETS, VIDEO_CODECS_MAP, AUDIO_CODECS_MAP, sms_supports; ...enumerate _restrict_codec_choices output per preset and re-test with sms_supports...\"  plus a control: ffmpeg -y -i src.mp4 -t 2 -c:v mpeg2video -b:v 1000k -c:a aac probe_avi.avi",
            "observed": "offered (preset x vcodec x acodec) combos: 63   that trip the container WARN: 0\nThe branch is live — the hand-built control file (MPEG-2 + AAC in an AVI, which the panel will NOT offer) correctly produces:\n  WARN\n    W: MPEG2VIDEO video inside .avi is not in SMS's codec table for that container and probably will not open on the PS2.\n    W: AAC audio inside .avi is not in SMS's codec table for that container and probably will not play on the PS2.\nSo _restrict_codec_choices genuinely closes the codec-x-container hole for all 63 reachable combinations. The catch is that the panel and the validator consult the SAME table (config.SMS_CONTAINER_CODECS), so the validator is structurally incapable of catching an error IN that table — which is exactly what the Vorbis-in-AVI and AC-3-in-MP4 findings above are.",
            "verdict": "WORKS",
            "root_cause": ""
          },
          {
            "scenario": "Which validator FAILs are reachable from Advanced Settings at all? Walked all ten FAIL branches against the panel's ranges.",
            "command": "Read C:/.../tools/sms_converter/validator.py:125-277, cross-checked against every empirical result above",
            "observed": "1. 'Output file does not exist' — n/a.\n2. file_size >= 3.99 GiB — the ONLY FAIL a user can plausibly reach, and it is driven by source duration, not by a setting alone (10000 kbps needs ~57 min of source). Not produced here.\n3. 'ffprobe failed' — n/a.\n4. streams > 8 — unreachable; get_settings_dict always sets limit_streams=True, so build_ffmpeg_cmd emits `-map 0:v:0? -map 0:a:0? -sn -dn` and the output has at most 2 streams (every file in this session: 2).\n5. h264/hevc/av1/vp9/vp8 — unreachable; VIDEO_CODECS_MAP contains none of them.\n6. w>1024 or h>1024 — unreachable; spin max IS 1024.\n7. Invalid MPEG-1/2 frame rate — unreachable; whitelist == combo list (verified all 8).\n8. Unsupported audio codec — unreachable; all six AUDIO_CODECS_MAP outputs (mp3/mp2/aac/ac3/vorbis/flac as ffprobe names them) match validator.py:251.\n9/10. Packet-size ceilings — unreachable (25x and 900x margin measured).\nNet: across ~40 deliberately abusive configurations run through the real build_ffmpeg_cmd, the validator returned PASS or WARN every single time and never once deleted a file. Every WARN keeps the output (worker.py:222 `if val_result.status in (\"PASS\", \"WARN\")` -> promote), so nothing the panel can set is ever rejected.",
            "verdict": "DEGRADED",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/validator.py:117-278 — every FAIL threshold sits outside the reachable range of the corresponding Advanced Settings control."
          },
          {
            "scenario": "Scaling Mode combo: 'Fit inside dimensions' is described as 'Scale down without adding black bars', but its filter chain contains a pad=.",
            "command": "python -c \"from harness import run; run('fit_inside_640x480','.avi',scaling_mode='Fit inside dimensions'); run('letterbox_640x480','.avi',scaling_mode='Letterbox to exact dimensions')\"",
            "observed": "Fit inside  -> -vf scale='min(640,iw)':'min(480,ih)':force_original_aspect_ratio=decrease,pad=640:480:(ow-iw)/2:(oh-ih)/2:black,scale=trunc(iw/2)*2:trunc(ih/2)*2,fps=...  -> 640x480, vpkt=17787\nLetterbox   -> -vf scale=640:480:force_original_aspect_ratio=decrease,pad=640:480:(ow-iw)/2:(oh-ih)/2:black,fps=...                                    -> 640x480, vpkt=17787\nByte-identical output size and identical max packet — the two modes are the same thing for a downscale, and 'Fit inside' does add black bars despite its tooltip. The trailing scale=trunc(iw/2)*2 is also a no-op because pad has already fixed the size to 640x480.",
            "verdict": "DEGRADED",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/ffmpeg_utils.py:246-249 — the 'Fit inside dimensions' branch pads to the exact target, which is what the 'Letterbox' branch is for."
          },
          {
            "scenario": "Dimension spin boxes vs the 'Allow upscaling' checkbox — the panel shows the orange over-resolution warning based on the SPIN BOX values, but build_scale_filter silently overrides those values when upscaling is off.",
            "command": "python -c \"from harness import run; run('upscale_off_1024','.avi', width=1024, height=1024, allow_upscale=False, scaling_mode='Stretch')\"   (source is 720x480)",
            "observed": "Generated filter: -vf scale=720:480,fps=fps=30/1,format=yuv420p — the requested 1024x1024 was clamped to the source size. Output is 720x480, validator WARN (720x480 above standard).\nMeanwhile _check_resolution_warning (advanced_settings.py:551-557) tests the spin values (1024 > 640) and shows 'Resolution exceeds standard 640x480...' for a file that is 720x480. The number in the box and the number in the file disagree with no indication which one won.",
            "verdict": "DEGRADED",
            "root_cause": "C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter/ffmpeg_utils.py:238-242 — the upscale clamp happens in the filter builder; the UI warning at advanced_settings.py:551 never sees it."
          },
          {
            "scenario": "Multi-pass encoding checkbox across every video codec the panel offers (the Passes spin is hard-locked to 2..2, so there is no 3-pass hole).",
            "command": "python C:/.../scratchpad/twopass.py  — for each codec, build_ffmpeg_cmd with pass_num=1 then pass_num=2 and the shared passlogfile, exactly as worker.py:178-183 does",
            "observed": "xvid/.avi      pass1 exit 0, pass2 exit 0 -> PASS, 1501 kbps for a 1500 kbps target\nmpeg4/.mp4     pass1 exit 0, pass2 exit 0 -> PASS, 1477 kbps\nmpeg2/.mpg     pass1 exit 0, pass2 exit 0 -> PASS, 1708 kbps\nmpeg1/.mpg     pass1 exit 0, pass2 exit 0 -> PASS, 1705 kbps\nmsmpeg4v3/.avi pass1 exit 0, pass2 exit 0 -> PASS, 1472 kbps\nTwo-pass itself is sound on all five codecs and the pass-count cap is correctly enforced by the widget. It does NOT rescue the QPel or the msmpeg4v3+B-frames failures — both die on pass 1 with the same errors as single-pass.",
            "verdict": "WORKS",
            "root_cause": ""
          }
        ],
        "notes": [
          "HEADLINE: three settings the panel presents as ordinary options kill the conversion outright, with no output and only a raw ffmpeg message shown to the user. (1) QPel — unconditional, every codec, every container, because -qpel is not an ffmpeg option (it is a value of -flags). (2) B-frames >0 with Microsoft MPEG-4 v3. (3) 246 of the 2160 reachable audio (codec, bitrate, sample-rate, channels) combinations — 11.4%. None of these are validator problems: the validator never runs because no file is produced.",
          "SECOND HEADLINE: across ~40 deliberately abusive configurations driven through the real build_ffmpeg_cmd, validate_converted_file returned PASS or WARN every single time and never once deleted a file. Every reachable FAIL threshold sits strictly outside the reachable range of its control: resolution FAIL needs >1024 but the spin caps AT 1024; the MPEG-1/2 frame-rate whitelist is byte-identical to the combo's own list; the stream-count FAIL is impossible because limit_streams is hardcoded True; the 2 MiB / 512 KiB packet ceilings have 25x and 900x margin at the panel's worst case. The only FAIL a user can plausibly hit is the 3.99 GiB size check, and that is a function of source duration rather than of any setting.",
          "The codec x container filtering (_restrict_codec_choices) is genuinely effective — 0 of the 63 reachable (preset x vcodec x acodec) triples trip the validator's container WARN, and I confirmed the WARN branch itself is live by hand-building an MPEG-2+AAC AVI, which it correctly flags twice. But the panel and the validator read the SAME table (config.SMS_CONTAINER_CODECS), so the validator cannot catch an error in that table. Two such errors are real: Vorbis-in-AVI (ffmpeg writes wFormatTag 0x566F; SMS's 16-bit table at src/SMS_Codec.c:62 only has 0x674F) and AC-3-in-MP4/M4A (ffmpeg writes an 'ac-3'+dac3 sample entry with NO esds; SMS's MOV reader identifies codecs only through _mov_read_esds, src/SMS_ContainerMOV.c:359-408, and has no 'ac-3'/'dac3' atom handler). Both produce PASS with zero warnings. By contrast I verified the object-type ids for the other MP4 combos DO line up (MPEG-1 -> 0x6A/106, MPEG-2 -> 0x61/97, Vorbis -> 0xDD/221, AAC -> 0x40/64, all matching s_MP4ObjType), and AC-3/MP3 in AVI write 0x2000/0x0055 which ARE in the SMS table, so those combinations are fine.",
          "Silent-lie class (file produced, validator silent, user got something other than what they set): MP3 320 kbps @ 22050 -> 160 kbps; GMC is a no-op on 4 of 5 codecs (only libxvid has the AVOption — ffmpeg even prints 'Codec AVOption gmc ... has not been used for any stream' and exits 0); FLAC + Normalize -> 192 kHz and 13.4x the source size, from a preset whose description is 'Keeps every bit of the original'; 'Fit inside dimensions' produces byte-identical output to 'Letterbox' despite a tooltip promising no black bars; and the resolution spin boxes are silently overridden by the upscale clamp while the orange warning still quotes the spin value.",
          "Aside, outside the requested area but noticed while checking the tables: config.py:258-259 states the AVI audio table has no AAC entry, but src/SMS_Codec.c:65-66 does list two AAC tags (0x6134706D and 0x000000FF) and a FLAC tag (0x43614C66). 0x00FF is 16-bit and therefore reachable by SMS_Container.c:290's File_GetUShort; the two 32-bit values can never match a 16-bit read. Not a hole in the panel — the panel is more conservative than SMS here — but the comment overstates the case.",
          "MAX_WIDTH / MAX_HEIGHT = 1024 (config.py:20-21) is the one limit in that file with no citation to an SMS source file, unlike SMS_CONTAINER_CODECS which cites exact line ranges. WARNING_RESOLUTION_MSG asserts that anything up to it 'satisfies SMS format rules'; I could not find a resolution ceiling in the SMS sources to corroborate 1024.",
          "No test coverage: grep across test_converter.py and test_end_to_end.py for qpel / gmc / bframes / msmpeg4 returns exactly one hit (test_converter.py:237, a sms_supports assertion). None of the three hard-failure classes above have a test.",
          "Reproduction harness left at C:/Users/natha/AppData/Local/Temp/claude/C--Users-natha-Github-Simple-Media-System-Simple-Media-System--claude-worktrees-sync-status-check-fb8014/bd14acea-cbf8-4209-8e53-ec19927777fa/scratchpad/ — harness.py (single-case runner through build_ffmpeg_cmd + validate_converted_file), sweep_fast.py (the 2160-combo audio matrix), twopass.py. Nothing was written into the repo; the repo was read-only throughout."
        ]
      }
    ]
  },
  "workflowProgress": [
    {
      "type": "workflow_phase",
      "index": 1,
      "title": "SMS source"
    },
    {
      "type": "workflow_phase",
      "index": 2,
      "title": "ffmpeg probe"
    },
    {
      "type": "workflow_phase",
      "index": 3,
      "title": "Synthesize"
    },
    {
      "type": "workflow_agent",
      "index": 1,
      "label": "sms:avi",
      "phaseIndex": 1,
      "phaseTitle": "SMS source",
      "agentId": "ab8bb3c0a497102ce",
      "model": "claude-opus-5",
      "state": "done",
      "startedAt": 1785366342333,
      "queuedAt": 1785366340002,
      "attempt": 1,
      "lastToolName": "StructuredOutput",
      "lastToolSummary": "AVI container path (src/SMS_ContainerAVI.c + src/SMS_Codec.…",
      "promptPreview": "You are auditing the PlayStation 2 media player \"SMS\" at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014.\n\nABSOLUTE RULE: READ ONLY. Do NOT edit, create, or delete ANY file anywhere. The PS2 source\n(src/, include/, iop/, irx/) is working perfectly and is off-limits for modification. You are\nextracting facts from it, nothing more.\n\nYour job i…",
      "lastProgressAt": 1785367041716,
      "tokens": 166836,
      "toolCalls": 35,
      "durationMs": 699383,
      "resultPreview": "{\"area\":\"AVI container path (src/SMS_ContainerAVI.c + src/SMS_Codec.c + the SMS_Container.c / SMS_Player.c code they hand off to)\",\"findings\":[{\"claim\":\"s_CodecVideoTags accepts EXACTLY 22 entries (21 real + NULL terminator), all mapping to only 3 codec IDs: MPEG4 (11 tags), MSMPEG4V3 (9 tags), DXSB (1 tag). Full set -> SMS_CodecID_MPEG4: 'DIVX','DX50','XVID','MP4S','M4S2',0x00000004,'DIV1','BLZ0'…"
    },
    {
      "type": "workflow_agent",
      "index": 2,
      "label": "sms:mp4",
      "phaseIndex": 1,
      "phaseTitle": "SMS source",
      "agentId": "a1f0a38714dbd983b",
      "model": "claude-opus-5",
      "state": "done",
      "startedAt": 1785366342258,
      "queuedAt": 1785366340003,
      "attempt": 1,
      "lastToolName": "StructuredOutput",
      "lastToolSummary": "MP4 / MOV / M4A container path (src/SMS_ContainerMOV.c + sr…",
      "promptPreview": "You are auditing the PlayStation 2 media player \"SMS\" at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014.\n\nABSOLUTE RULE: READ ONLY. Do NOT edit, create, or delete ANY file anywhere. The PS2 source\n(src/, include/, iop/, irx/) is working perfectly and is off-limits for modification. You are\nextracting facts from it, nothing more.\n\nYour job i…",
      "lastProgressAt": 1785367193499,
      "tokens": 183481,
      "toolCalls": 45,
      "durationMs": 851240,
      "resultPreview": "{\"area\":\"MP4 / MOV / M4A container path (src/SMS_ContainerMOV.c + src/SMS_Codec.c and the pieces they feed)\",\"findings\":[{\"claim\":\"The file MUST begin with a box whose type field (bytes 4..7) is literally 'ftyp' AND whose bytes 8..11 (major_brand) are non-zero. Nothing else is probed. A classic QuickTime .mov that starts with 'moov'/'mdat'/'wide'/'free' is rejected outright by BOTH SMS_GetContaine…"
    },
    {
      "type": "workflow_agent",
      "index": 3,
      "label": "sms:mpegps",
      "phaseIndex": 1,
      "phaseTitle": "SMS source",
      "agentId": "a347e981ab2a90a57",
      "model": "claude-opus-5",
      "state": "done",
      "startedAt": 1785366343245,
      "queuedAt": 1785366340003,
      "attempt": 1,
      "lastToolName": "StructuredOutput",
      "lastToolSummary": "MPEG Program Stream container path (.mpg / .mpeg) — src/SMS…",
      "promptPreview": "You are auditing the PlayStation 2 media player \"SMS\" at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014.\n\nABSOLUTE RULE: READ ONLY. Do NOT edit, create, or delete ANY file anywhere. The PS2 source\n(src/, include/, iop/, irx/) is working perfectly and is off-limits for modification. You are\nextracting facts from it, nothing more.\n\nYour job i…",
      "lastProgressAt": 1785367146433,
      "tokens": 193370,
      "toolCalls": 52,
      "durationMs": 803187,
      "resultPreview": "{\"area\":\"MPEG Program Stream container path (.mpg / .mpeg) — src/SMS_ContainerMPEG_PS.c and everything it directly reaches\",\"findings\":[{\"claim\":\"The demuxer accepts EXACTLY three top-level PES stream-id groups. Everything else (0x1B9 program_end, 0x1F0-0x1FF, 0x1BE padding, 0x1BF private_2, 0x1BC PSM, 0x1BB sys-hdr, 0x1BA pack) is either consumed as metadata or silently resynced past. Accepted: 0…"
    },
    {
      "type": "workflow_agent",
      "index": 4,
      "label": "sms:video-decode",
      "phaseIndex": 1,
      "phaseTitle": "SMS source",
      "agentId": "a2033b14abd6180cb",
      "model": "claude-opus-5",
      "state": "done",
      "startedAt": 1785366342369,
      "queuedAt": 1785366340003,
      "attempt": 1,
      "lastToolName": "StructuredOutput",
      "lastToolSummary": "Video decoder ceilings (MPEG-1/2 via IPU + libmpeg; MPEG-4 …",
      "promptPreview": "You are auditing the PlayStation 2 media player \"SMS\" at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014.\n\nABSOLUTE RULE: READ ONLY. Do NOT edit, create, or delete ANY file anywhere. The PS2 source\n(src/, include/, iop/, irx/) is working perfectly and is off-limits for modification. You are\nextracting facts from it, nothing more.\n\nYour job i…",
      "lastProgressAt": 1785367451414,
      "tokens": 395249,
      "toolCalls": 89,
      "durationMs": 1109044,
      "resultPreview": "{\"area\":\"Video decoder ceilings (MPEG-1/2 via IPU + libmpeg; MPEG-4 Part 2 / MSMPEG4v3 software) — SMS at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014\",\"findings\":[{\"claim\":\"GLOBAL HARD CEILING: any video stream whose container-reported width OR height exceeds 1024 is refused before decoding starts, with the error string STR_RESOLUTION_TO…"
    },
    {
      "type": "workflow_agent",
      "index": 5,
      "label": "sms:audio-decode",
      "phaseIndex": 1,
      "phaseTitle": "SMS source",
      "agentId": "a7bdcc6f50dad801c",
      "model": "claude-opus-5",
      "state": "done",
      "startedAt": 1785366341982,
      "queuedAt": 1785366340003,
      "attempt": 1,
      "lastToolName": "StructuredOutput",
      "lastToolSummary": "Audio decoder ceilings (channels / sample rate / bitrate / …",
      "promptPreview": "You are auditing the PlayStation 2 media player \"SMS\" at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014.\n\nABSOLUTE RULE: READ ONLY. Do NOT edit, create, or delete ANY file anywhere. The PS2 source\n(src/, include/, iop/, irx/) is working perfectly and is off-limits for modification. You are\nextracting facts from it, nothing more.\n\nYour job i…",
      "lastProgressAt": 1785367074557,
      "tokens": 299989,
      "toolCalls": 68,
      "durationMs": 732574,
      "resultPreview": "{\"area\":\"Audio decoder ceilings (channels / sample rate / bitrate / SPU output path) — SMS PS2 media player\",\"findings\":[{\"claim\":\"The AC3 decoder can emit ONLY 1 or 2 output channels. The float-to-int converter is selected from a 2-entry function-pointer table indexed by (channels-1); there is no 3+ channel entry and no 3+ channel writer anywhere in the file. 5.1 AC3 CANNOT be passed through as 5…"
    },
    {
      "type": "workflow_agent",
      "index": 6,
      "label": "sms:player-limits",
      "phaseIndex": 1,
      "phaseTitle": "SMS source",
      "agentId": "ab828b360d3d3e7a5",
      "model": "claude-opus-5",
      "state": "done",
      "startedAt": 1785366342486,
      "queuedAt": 1785366340004,
      "attempt": 1,
      "lastToolName": "StructuredOutput",
      "lastToolSummary": "SMS (PS2) player-wide and I/O limits — packet buffers, stre…",
      "promptPreview": "You are auditing the PlayStation 2 media player \"SMS\" at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014.\n\nABSOLUTE RULE: READ ONLY. Do NOT edit, create, or delete ANY file anywhere. The PS2 source\n(src/, include/, iop/, irx/) is working perfectly and is off-limits for modification. You are\nextracting facts from it, nothing more.\n\nYour job i…",
      "lastProgressAt": 1785367297902,
      "tokens": 253082,
      "toolCalls": 78,
      "durationMs": 955416,
      "resultPreview": "{\"area\":\"SMS (PS2) player-wide and I/O limits — packet buffers, stream table, file-offset width, read path/bandwidth, index & duration\",\"findings\":[{\"claim\":\"MAXIMUM VIDEO PACKET PAYLOAD = 2,097,088 bytes (2 MiB - 64). The downstream tool's \\\"2 MiB video\\\" is the RING BUFFER capacity, not the packet ceiling — it is off by the 64-byte packet header. A packet of exactly 2 MiB (2,097,152) FAILS to al…"
    },
    {
      "type": "workflow_agent",
      "index": 7,
      "label": "probe:encoder-grid",
      "phaseIndex": 2,
      "phaseTitle": "ffmpeg probe",
      "agentId": "a38c5ea93b73fec68",
      "model": "claude-opus-5",
      "state": "done",
      "startedAt": 1785367455378,
      "queuedAt": 1785367453257,
      "attempt": 1,
      "lastToolName": "StructuredOutput",
      "lastToolSummary": "Audio parameter combinations: which codec x bitrate x sampl…",
      "promptPreview": "You are testing the SMS Media Converter at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter (Python + PySide, wraps ffmpeg).\n\nffmpeg 8.1.1 and ffprobe are on PATH; python is on PATH. You MAY run ffmpeg/ffprobe/python\nfreely. Write every temp file under C:/Users/natha/AppData/Local/Temp/claude/C--Users-natha-Github-Simple-M…",
      "lastProgressAt": 1785368485724,
      "tokens": 145641,
      "toolCalls": 27,
      "durationMs": 1030346,
      "resultPreview": "{\"area\":\"Audio parameter combinations: which codec x bitrate x sample-rate x channel cells ffmpeg 8.1.1 actually accepts, versus which the Advanced Settings dropdowns can emit\",\"results\":[{\"scenario\":\"Full 504-cell sweep (6 codecs x 7 bitrates x 4 sample rates x 3 channel counts), encoder isolated from muxer by writing to Matroska, then re-run to the null muxer to capture ffmpeg's negotiated outpu…"
    },
    {
      "type": "workflow_agent",
      "index": 8,
      "label": "probe:cross-convert",
      "phaseIndex": 2,
      "phaseTitle": "ffmpeg probe",
      "agentId": "a7f25f7452073631a",
      "model": "claude-opus-5",
      "state": "done",
      "startedAt": 1785367455070,
      "queuedAt": 1785367453257,
      "attempt": 1,
      "lastToolName": "StructuredOutput",
      "lastToolSummary": "sms_converter: audio-source -> video-target and video-sourc…",
      "promptPreview": "You are testing the SMS Media Converter at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter (Python + PySide, wraps ffmpeg).\n\nffmpeg 8.1.1 and ffprobe are on PATH; python is on PATH. You MAY run ffmpeg/ffprobe/python\nfreely. Write every temp file under C:/Users/natha/AppData/Local/Temp/claude/C--Users-natha-Github-Simple-M…",
      "lastProgressAt": 1785367966093,
      "tokens": 154858,
      "toolCalls": 28,
      "durationMs": 511023,
      "resultPreview": "{\"area\":\"sms_converter: audio-source -> video-target and video-source -> audio-target (ffmpeg 8.1.1-full_build-www.gyan.dev, libxvid present)\",\"results\":[{\"scenario\":\"Q1 -- VIDEO source + AUDIO preset (video-with-audio.mp4 -> MP3 Audio preset). Does '-map 0:v:0? -map 0:a:0? ... -vn' break the muxer in ffmpeg 8.1.1?\",\"command\":\"ffmpeg -y -i <SCRATCH>/in/video-with-audio.mp4 -map 0:v:0? -map 0:a:0? …"
    },
    {
      "type": "workflow_agent",
      "index": 9,
      "label": "probe:ceiling-holes",
      "phaseIndex": 2,
      "phaseTitle": "ffmpeg probe",
      "agentId": "a8e30265db403a27c",
      "model": "claude-opus-5",
      "state": "done",
      "startedAt": 1785367455092,
      "queuedAt": 1785367453257,
      "attempt": 1,
      "lastToolName": "StructuredOutput",
      "lastToolSummary": "Advanced Settings panel — every way it lets the user exceed…",
      "promptPreview": "You are testing the SMS Media Converter at C:/Users/natha/Github/Simple-Media-System/Simple-Media-System/.claude/worktrees/sync-status-check-fb8014/tools/sms_converter (Python + PySide, wraps ffmpeg).\n\nffmpeg 8.1.1 and ffprobe are on PATH; python is on PATH. You MAY run ffmpeg/ffprobe/python\nfreely. Write every temp file under C:/Users/natha/AppData/Local/Temp/claude/C--Users-natha-Github-Simple-M…",
      "lastProgressAt": 1785369157153,
      "tokens": 184167,
      "toolCalls": 50,
      "durationMs": 1702061,
      "resultPreview": "{\"area\":\"Advanced Settings panel — every way it lets the user exceed / contradict what SMS (and ffmpeg) can do\",\"results\":[{\"scenario\":\"CONTROL INVENTORY (read, not run). Video: Codec combo (5 entries -> mpeg4 x2, mpeg2video, mpeg1video, msmpeg4v3, filtered per container); Width QSpinBox 2..1024 step 2; Height QSpinBox 2..1024 step 2; Preserve-aspect checkbox; Scaling mode combo (4); Allow-upscale…"
    },
    {
      "type": "workflow_agent",
      "index": 10,
      "label": "synthesize",
      "phaseIndex": 3,
      "phaseTitle": "Synthesize",
      "agentId": "a6b4edd6264e50d2c",
      "model": "claude-opus-5",
      "state": "done",
      "startedAt": 1785369162384,
      "queuedAt": 1785369158914,
      "attempt": 1,
      "lastToolName": "Bash",
      "lastToolSummary": "cd \"C:/Users/natha/AppData/Local/Temp/claude/C--Users-natha…",
      "promptPreview": "You are writing the authoritative capability specification that a converter rewrite will be\nbuilt on. Below are findings from six read-only audits of the PS2 SMS source, and three\nempirical ffmpeg probes.\n\nSMS SOURCE FINDINGS:\n[\n {\n  \"area\": \"AVI container path (src/SMS_ContainerAVI.c + src/SMS_Codec.c + the SMS_Container.c / SMS_Player.c code they hand off to)\",\n  \"findings\": [\n   {\n    \"claim\": …",
      "lastProgressAt": 1785369932221,
      "tokens": 247933,
      "toolCalls": 25,
      "durationMs": 769836,
      "resultPreview": "# SMS (PS2) OUTPUT CAPABILITY SPECIFICATION v1\n\n**Status of evidence.** Every claim below is tagged:\n\n- **[SRC]** — read directly out of the SMS C sources in this worktree.\n- **[SRC‑V]** — I re‑read the cited source myself in this session and confirmed it (not just inherited from an audit).\n- **[EMP]** — measured with ffmpeg 8.1.1‑full_build‑www.gyan.dev / ffprobe on this machine.\n- **[EMP‑V]** — …"
    }
  ],
  "totalTokens": 2224606,
  "totalToolCalls": 497
}