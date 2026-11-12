# hunt-bloat-deadcode

Read-only audit of what this fork added on top of SMS 2.9rev4 in src/ and include/. No repository file was touched; every trial ran on `git archive HEAD` copies under (investigator scratch, not kept)/hunt-bloat-deadcode/ (scripts patch.py, mk.sh, reach.py, chkdata.py; build trees b/p0..b/dgall). Nothing was run on a PS2 or emulator; no hardware acceptance is claimed. This label has no reporter-retest list.

Method: ee-gcc 3.2.3 in the ps2dev docker image, no -ffunction-sections, so unused statics and unreferenced globals are really linked (confirmed with ee-nm). Dead candidates came from a symbol-level reachability scan over all objects (MIPS REL addends decoded, gp0 from .reginfo) and each one was confirmed by grep over src/, include/, iop/, tools/ including assembly. Each proposed removal was then built in isolation and combined, in three variants.

Control build reproduces the baseline exactly: bin/SMS.elf 1,810,056, packed 947,332, ee-size text 719,456 / data 1,085,334 / bss 554,036.

Measured savings (BDM=1 release, `make pack`):
- F1 dead IRX blobs in g_DataBuffer: ELF -37,376, packed -26,432, RAM -37,312
- F2 dead legacy IPU icon data + IPU_UnpackImage: ELF -20,480, packed -17,456
- F3 small dead-code batch: ELF -3,200, packed -1,088
- F4 SMS_ExitCrumb call sites/strings in release: ELF -896, packed -1,008
- All four: ELF 1,810,056 -> 1,748,232 (-61,824, -3.4%), packed 947,332 -> 902,868 (-44,464, -4.7%), text+data+bss 2,358,826 -> 2,296,986 (-61,840 bytes of RAM)
- BDM=0: 1,623,024 -> 1,581,168 (-41,856)
- SMS_DIAG=1: 1,812,856 -> 1,751,928 (-60,928)
All ten builds exit 0; compiler warnings drop from 18 to 10 in the combined build. Host test suite on the combined tree: PASS=44 FAIL=0 (same as baseline 44/44).

Ruled out (checked, not bloat or not provably removable):
- Static .bss growth: the large symbols (s_Samples 196,608, s_Bitmap 16,384, thread stacks) are upstream or in use; fork additions are only s_PFStack 4 KB, s_ds34Stack 4 KB and libfileXio internals.
- Embedded blobs: splash_jpg 61,728, jellyfish_jpg 44,240, main_bg_mini_jpg, g_RGBA_Blob 27,764, g_IconBallRGBA, g_IconLoadingRGBA and every *_irx blob are referenced.
- The xz decoder is already minimal (XZ_SINGLE, CRC32 only, no BCJ, about 11 KB).
- SMS_DIAG instrumentation in SMS_RingBuffer.c, SMS_FileContext.c, SMS_Container.c and SMS_Player.c (PLAY_CRUMB, PROBE_CRUMB, DIAG_CRUMB, SMS_RingBufferName) does compile to nothing in release; only SMS_ExitCrumb leaks (F4).
- Fork changes in SMS_Player.c, SMS_VideoBuffer.c, SMS_SPU.c are functional hardening, not provably removable.
- SMS_SIO2MAN blob (5,817 bytes) is dead under BDM but lies inside the region reused as the audio ring buffer; removing it would shrink the ring, so it is not proposed.
- Seen but not proposed because removal would change behaviour: g_SaveDiag "TEMP diag" strings in src/SMS_Config.c (line 69, 809-1006); roughly 2 s nop busy-wait loops repeated in the USB/MX4SIO/iLink/ATA/MMCE starters in src/SMS_IOP.c; _spu_wait PollSema polling; three copies of a 4-entry lBit table and duplicated _is_sub helpers (a few dozen bytes).
- Dead upstream leftovers (not fork-added, about 1.5 KB total, left alone): CDDA_GetPicture, CDDA_GetDiskPicture, CDDA_SetMediaMode, CDDA_Pause, RC_Shutdown, RCX_Shutdown, CDVD_QueryDVDV, PlayerControl_SwitchSubs, GS_XYZF, SMS_iTimerReset, DSP_FFT, g_pMPEG12Str, g_pSpaceStr, g_BSlashStr, DTS tables lossy_quant/lossless_quant/dts_bit_rates/dts_sample_rates/dts_bits_per_sample.

One out-of-scope observation is included as F6 from code reading only (not reproduced): the in-place network switch resets the IOP but only restarts the network stack, and its comment about dev9 reload is false.

## #1 Dead IRX modules still embedded in g_DataBuffer (SMSSMB always; UMS and USBD under BDM)
- issue: 0
- severity: medium
- category: cleanup
- files: ['src/SMS_Data.c', 'include/SMS_Data.h', 'src/SMS_IOP.c']
- reproduced: True
- confidence: 0.9
- risk: Low. Pure data layout change; every live module moves but is addressed only through the macro chain, and the byte-identity check covers it. The audio ring size and alignment are unchanged by construction (multiple of 64). Not run on a console.

### evidence
g_DataBuffer is a 266,560-byte .data blob of concatenated IRX images. Three of them have no consumer in the release build. (1) SMSSMB.IRX (block starting at the `/* SMSSMB.IRX */` marker, src/SMS_Data.c:12412, 19,641 bytes, 19,648 padded): SMS_SMB_OFFSET/SMS_SMB_SIZE are referenced nowhere outside the macro chain in include/SMS_Data.h; upstream loaded it at its SMS_IOP.c:252, the fork loads smbman_irx under BDM and sets `i = -1; /* SMB unavailable in non-BDM builds */` otherwise (src/SMS_IOP.c:590-596). (2) UMS.IRX (src/SMS_Data.c:5480, 8,504 -> 8,512) and USBD.IRX (src/SMS_Data.c:6015, 9,140 -> 9,152): SMS_USB_MASS_* and SMS_USBD_* are used only at src/SMS_IOP.c:673 and :687, inside the `#else` (non-BDM) arm of SMS_IOPStartUSB; BDM uses usbd_irx/usbmass_bd_irx. Total removable under BDM: 37,312 bytes = 583 x 64, so SMS_AUDIO_BUFFER keeps its 64-byte alignment and its size (44,672). Trial build: ELF 1,810,056 -> 1,772,680 (-37,376), packed 947,332 -> 920,900 (-26,432), .data -37,312. chkdata.py confirmed every remaining module is byte-identical at its new offset, all offsets are 16-aligned, buffer 266,560 -> 229,248 (BDM) / 246,912 (BDM=0, SMSSMB only).

### fix
In src/SMS_Data.c delete the SMSSMB.IRX block and wrap the UMS.IRX and USBD.IRX blocks in `#ifndef BDM ... #endif`. In include/SMS_Data.h remove SMS_SMB_SIZE/SMS_SMB_OFFSET, chain SMS_SMSUTILS_OFFSET from `SMS_VU0_DATA_OFFSET + SMS_VU0_DATA_SIZE`, and under `#ifdef BDM` define `SMS_USB_MASS_SIZE 0` and `SMS_USBD_SIZE 0` so the offset chain collapses. Do not touch SMS_SIO2MAN (it sits inside the audio ring-buffer region).

### test
Build BDM=1, BDM=0 and SMS_DIAG=1 (all built clean in the trial). Re-run the byte-identity check: extract each module at its new SMS_*_OFFSET and compare with the old image; assert SMS_AUDIO_BUFFER is still 64-aligned and 44,672 bytes. Run the host test suite (44/44 on the combined trial tree). On hardware: boot, play one file with audio, and on a non-BDM build mount USB.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "Could not refute it. The dead data is real and the proposed removal held up in my own independent trial, but severity is low, not medium: it saves about 37 KB of a 32 MB machine and 26 KB of packed download, and fixes no behaviour. Nothing was run on a console.\n\n**Is the data dead? (read against current tree, c275476)**\n- `git grep` over all tracked files finds `SMS_SMB_OFFSET` / `SMS_SMB_SIZE` only in the macro chain at `include/SMS_Data.h:24,42,43`. No C or assembly consumer exists.\n- `SMS_IOPStartNet` (`src/SMS_IOP.c:582-596`) loads `smbman_irx` under `#ifdef BDM` and otherwise sets `i = -1; /* SMB unavailable in non-BDM builds */`. So the SMSSMB image is unreachable in both configurations.\n- `SMS_USBD_*` and `SMS_USB_MASS_*` are used only at `src/SMS_IOP.c:673` and `:687`, both inside the `#else` (non-BDM) arm of `SMS_IOPStartUSB`. The BDM arm uses `usbd_irx` / `usbmass_bd_irx`.\n- `Makefile:3` has `BDM ?= 1`, and `-DBDM` is added to the global `EE_CFLAGS` (`Makefile:54`), so `SMS_Data.c` and every includer of `SMS_Data.h` see the same setting.\n- All other `g_DataBuffer` uses go through the `SMS_*_OFFSET` macros (`SMS_IOP.c:179-184, 471, 503, 575, 579, 600`; `SMS_CDVD.c:77`; `SMS_MPEG4.c:520-521`; `SMS_Player.c:294`; `SMS_ContainerMPEG_PS.c:919`). There are no hard-coded numeric offsets.\n- Upstream 2.9rev4 did use these blobs (`SMS_IOP.c:252` for SMB, `:289` and `:303` for USB). This is leftover from the fork's move to BDM/smbman, not a behavioural regression.\n\n**Host reproduction (scratch `verify-hunt-bloat-deadcode`)**\nI wrote my own patch script and applied it to a private export. I then compiled the real `src/SMS_Data.c` with gcc in four configurations (original and patched, each with and without `-DBDM`) and dumped every module at its `SMS_*_OFFSET`.\n\n| Config | Buffer size before | Buffer size after | `SMS_AUDIO_BUFFER_SIZE` |\n|---|---|---|---|\n| BDM | 266,560 | 229,248 | 44,672 (unchanged) |\n| non-BDM | 266,560 | 246,912 | 44,672 (unchanged) |\n\n- Every surviving module is byte-identical (`cmp`) at its new offset: 14 modules under BDM, 16 under non-BDM including USBD and USB_MASS.\n- All offsets remain 16-aligned and the buffer stays 64-aligned.\n- The removed sizes are exact multiples of 64 (37,312 = 583\u00d764; 19,648 = 307\u00d764), which is why the audio ring keeps its size and alignment.\n- The embedded UMS/USBD images start with the XZ magic `fd 37 7a 58`, so the non-BDM `SifExecDecompModuleBuffer` path still works on them.\n\n**PS2 builds of the patched tree (pinned toolchain)**\n- `all`, `BDM=0 all` and `SMS_DIAG=1 all` all returned rc=0.\n- The `SMS_Data.c` trial produced no new diagnostics: the warning/error lines in the BDM=1, BDM=0 and SMS_DIAG=1 logs match the same-config baselines line for line (only docker-compose's unset-variable notices differ); the baseline is not warning-free (`build/base.log` has 248 such lines).\n\n| Build | ELF before | ELF after | `.data` change |\n|---|---|---|---|\n| BDM=1 | 1,810,056 | 1,772,680 | 0xbe5cc \u2192 0xb540c (\u221237,312) |\n| BDM=0 | 1,623,024 | 1,603,440 | 0xbe218 \u2192 0xb9558 (\u221219,648) |\n\n- Packed ELF (BDM=1): 947,332 \u2192 920,900. This matches the reporter's numbers.\n- `.text` size is identical before and after in both configurations.\n- `.sdata` / `.bss` move by 0x9200 (BDM) and 0x4C80 (non-BDM), both multiples of 64, so every later object keeps its mod-64 alignment.\n- Host suite on the patched tree: PASS=44 FAIL=0.\n\n**Adversarial checks on the fix**\n- The region that used to sit just before the synth buffer was the dead SMSSMB tail; it is now the VU0 IDCT table. I looked for underflow: `MP123_CoreInit` does `memset(g_pSynthBuffer, 0, 4352)` from the base, and the AC3 workarea indexes upward from the base. I found no negative indexing.\n- The relationship between SMSUTILS, the synth buffer and the audio ring is unchanged by the patch.\n\n**Applying it**\n- Delete from the `/* SMSSMB.IRX */` marker up to, but not including, `/* SMSUTILS.IRX */`. That takes the 0xDB padding line with it.\n- Put `#ifndef BDM` immediately before `/* UMS.IRX */` and `#endif` immediately before `/* SMSTCPIP.IRX */`, so both padding lines are inside the guard.\n- In the header, remove `SMS_SMB_SIZE` and `SMS_SMB_OFFSET`, chain `SMS_SMSUTILS_OFFSET` from `SMS_VU0_DATA_OFFSET + SMS_VU0_DATA_SIZE`, and under `#ifdef BDM` define `SMS_USB_MASS_SIZE` and `SMS_USBD_SIZE` as 0.\n\n**Residual risk**\nEvery global after `g_DataBuffer` and the heap start move down by about 37 KB. A latent wild-pointer bug could behave differently on hardware; that cannot be ruled out from the host.\n\nScratch files are in (investigator scratch, not kept)/verify-hunt-bloat-deadcode (`patch.py`, `dump.c`, `run.sh`, `tree/`, `orig/`); build logs are (investigator scratch, not kept)/build/vhb-*.log.",
 "corrected_fix": ""
}

## #2 Legacy IPU icon arrays, six icon tables and IPU_UnpackImage are dead after the RGBA icon blob replaced them
- issue: 0
- severity: medium
- category: cleanup
- files: ['src/SMS_GUIcons.c', 'src/SMS_IPU_1.c', 'include/SMS_IPU.h', 'src/SMS_PlayerBallSim.c']
- reproduced: True
- confidence: 0.93
- risk: Low. Link-time proof that nothing references the removed symbols; icon rendering path (RGBA blob) is untouched. Not run on a console.

### evidence
GUI_LoadIcons (src/SMS_GUIcons.c:1388-1447) now only decompresses g_RGBA_Blob via lzma2_uncompress. Lines 36-1368 of the same file still define g_IconBall[316], 30 s_Icon* arrays and the tables s_pBrowserIcons, s_SizeBrowserIcons, s_pMiscIcons, s_SizeMiscIcons, s_pBrowserDevIcons, s_SizeBrowserDevIcons. gcc already warns that the six tables are unused; the arrays are referenced only by those tables; g_IconBall has one further mention, an unused `extern` at src/SMS_PlayerBallSim.c:26. 34 symbols, 19,462 bytes plus padding. IPU_UnpackImage (src/SMS_IPU_1.c:220-324, 744 bytes, declared at include/SMS_IPU.h:140) had upstream GUI_LoadIcons as its only caller and has none in the fork (grep over src, include, iop, tools; tools/test_skin_loading.py reads SMS_IPU_1.c but extracts other functions by name). Trial build: ELF 1,810,056 -> 1,789,576 (-20,480), packed 947,332 -> 929,876 (-17,456); text -752, data -19,712. The comment at src/SMS_GUIcons.c:25-28 saying the IPU path stays for the player ball/desktop is no longer true.

### fix
Delete src/SMS_GUIcons.c lines 36-1368 (the arrays and six tables) and fix the comment at lines 25-28; delete the unused extern at src/SMS_PlayerBallSim.c:26; delete IPU_UnpackImage from src/SMS_IPU_1.c:220-324 and its prototype at include/SMS_IPU.h:140.

### test
Build BDM=1, BDM=0, SMS_DIAG=1 (link succeeds in all three in the trial, which proves no remaining reference). Host tests 44/44, including tools/test_skin_loading.py. On hardware: browser, device and misc icons and the player ball still draw.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": true,
 "reproduced": true,
 "reasoning": "I could not refute this: the dead code is real and the proposed deletion is sound. Verified by reading current code and by my own trial builds and host test run; nothing was run on a console.\n\n**What I read (HEAD c275476)**\n- `src/SMS_GUIcons.c` `GUI_LoadIcons` (lines 1388-1447) only does `memalign` + `lzma2_uncompress(g_RGBA_Blob...)` + `memcpy` slicing + `_icon_darken`. It never touches the legacy arrays or tables.\n- Lines 36-1368 define `g_IconBall[316]`, 27 static `s_Icon*` arrays and the six static tables. The tables are the only users of the arrays, and nothing uses the tables.\n- `src/SMS_PlayerBallSim.c:26` is a bare `extern g_IconBall`; the ball is uploaded from `g_IconBallRGBA` at line 63.\n- `IPU_UnpackImage` (`src/SMS_IPU_1.c:220-324`) appears only at its definition and its prototype (`include/SMS_IPU.h:140`).\n- `git grep` over the whole tracked tree found only comments, `tools/bake_icons_rgba.py` (uses the names as PNG filenames in `theme/icons/`, does not parse the C file) and one doc mention in `MX4SIO_SOLUTION.md:256`.\n- `tools/test_skin_loading.py` extracts `_destroy`, `IPU_InitLoadImage` and `IPU_ImageInfo` from `SMS_IPU_1.c` by name, not `IPU_UnpackImage`.\n- `_darken_image` stays live through `IPU_LoadImage` (`SMS_IPU_1.c:180`), which `SMS_GUIDesktop.c:276` still calls.\n\n**Upstream 2.9rev4**\nUpstream `GUI_LoadIcons` calls `IPU_UnpackImage` four times (`SMS_GUIcons.c:1372-1391`) and `SMS_PlayerBallSim.c:57` calls `IPU_LoadImage(g_IconBall...)`. The code is live upstream and became dead only in the fork (commits ea7cc9f and 8ddc89b). It is leftover bloat, not a functional regression.\n\n**What I ran**\nFour private `git archive HEAD` trees under `(investigator scratch, not kept)/verify-hunt-bloat-deadcode/icons-ipu/`, built with ps2dev/ps2dev:v1.0. The patch is `(investigator scratch, not kept)/verify-hunt-bloat-deadcode/apply.py`.\n\n| Build | ELF | Packed | text | data |\n|---|---|---|---|---|\n| Baseline (`make pack`) | 1,810,056 | 947,332 | 719,456 | 1,085,334 |\n| Patched, BDM=1 | 1,789,576 | 929,876 | 718,704 | 1,065,622 |\n| Patched, BDM=0 | 1,602,544 | 765,588 | links, rc=0 | |\n| Patched, SMS_DIAG=1 | 1,792,376 | 930,868 | links, rc=0 | |\n\n- The BDM=1 delta matches the reporter exactly: ELF -20,480, packed -17,456, text -752, data -19,712.\n- The baseline log shows the six \"defined but not used\" warnings at lines 1336-1365; they are gone in the patched builds.\n- The baseline map has `IPU_UnpackImage` at 0x15d890 and `g_IconBall` at 0x21aa30; the patched map has neither.\n- `.sdata` shrinks from 0x96de to 0x8d1e (-2,496 bytes). `s_IconShare`, `s_IconSMB` and `s_IconPicture` have no `section(\".data\")` attribute, so they sat in the GP window. That is extra headroom for the `-G` limit the SMS_DIAG build already struggles with.\n- Host tests on the patched tree via `ht.sh`: PASS=44 FAIL=0.\n\n**Corrections to the report (none change the verdict)**\n- There are 27 `s_Icon*` arrays, not 30. The total of 34 symbols and 19,462 bytes is correct.\n- The comment at `SMS_GUIcons.c:25-28` is only half stale. The IPU still loads the desktop skin through `IPU_LoadImage`; only the \"player ball\" part is false. Reword it rather than claim the IPU is unused.\n- The deletion should cover lines 36-1369, taking the trailing blank line too.\n- Stale comments remain in `tools/bake_icons_rgba.py` (lines 14-15 and 23-24, which say `g_IconBall` keeps its IPU blob) and `SMS_PlayerBallSim.c:57-59`. Optional one-line touch-ups.\n\n**Hardware risk**\nLow. No executed path changes; the only effect is that `.data`/`.sdata`/`.bss` addresses shift down by about 20 KB.",
 "corrected_fix": ""
}

## #3 Small dead-code batch: orphaned asm round-rect, unused DS3/DS4 wrappers, dead pad helpers and BDM-unused statics
- issue: 0
- severity: low
- category: cleanup
- files: ['src/SMS_GS_0.S', 'include/SMS_GS.h', 'src/SMS_GS_1.c', 'src/libds34usb.c', 'src/libds34bt.c', 'include/libds34usb.h', 'include/libds34bt.h', 'src/SMS_PAD.c', 'src/SMS_GUI.c', 'src/SMS_GUIMenuSMS.c', 'src/SMS_Config.c', 'src/lzma2.c', 'src/SMS_IOP.c', 'src/SMS_GUIDesktop.c']
- reproduced: True
- confidence: 0.88
- risk: Low. Every removal is proven by a successful link in three configurations. The only conditional items are the two `#ifndef BDM` guards, which were exercised by the BDM=0 build. Not run on a console.

### evidence
Each item has no caller/reader (grep over src, include, iop, tools incl. .S and inline asm; reachability scan; isolated build). (a) src/SMS_GS_0.S: GS_RRV and GS_RenderRoundRect_asm (lines 827-1075, .globl at 60-61) plus the s_SinCos table (37-42, used only at line 845 inside GS_RRV); the fork's C GS_RenderRoundRect at src/SMS_GS_1.c:614 replaced it, and _gs_rrv is reached only from the dead asm (line 1037). Prototype at include/SMS_GS.h:1274. 936 bytes text + 24 sdata. (b) Unused wrappers ds34usb_deinit/_reinit_ports/_set_rumble/_set_led/_reset and ds34bt_deinit/_reinit_ports/_init_charging/_set_rumble/_set_led/_reset/_get_version/_get_features, about 1.3 KB. (c) src/SMS_PAD.c BDM section: PadDeinitPads (reachable only through the never-used macro PAD_Quit()), ReadPadStatus, ReadCombinedPadStatus, ReadCombinedPadStatus_raw, old_pad[]. (d) src/SMS_GUI.c:90-91 s_PadBuf0/g_PadBuf1 (2 x 256 bytes, 64-aligned): unused under BDM because include/SMS_PAD.h:32 defines PAD_OpenPort(a,b,c) as a comment; still used with BDM=0. (e) src/SMS_GUIMenuSMS.c:1452 _autonet_handler: unused under BDM (its table entry at line 258 is non-BDM only). (f) src/SMS_Config.c:61 s_pSMSCfg unused in every build; src/lzma2.c encode() unused (88 bytes). (g) Unused locals producing warnings only: src/SMS_IOP.c:1419-1420 (i, lFD, lBuff[64]), src/SMS_GUIDesktop.c:590 (i). Trial build: ELF 1,810,056 -> 1,806,856 (-3,200), packed -1,088; text -2,608, data -512.

### fix
Delete (a)-(c) and (f) with their prototypes; fix the stale comment at src/SMS_GS_1.c:608-609. Wrap s_PadBuf0/g_PadBuf1 in `#ifndef BDM`. Wrap _autonet_handler in `#ifndef BDM` including its forward declaration at src/SMS_GUIMenuSMS.c:74 (the trial wrapped only the definition and got a new "declared static but never defined" warning). Remove the unused locals in (g).

### test
Build BDM=1, BDM=0, SMS_DIAG=1; all three link in the trial. Warning count should fall (18 -> 10 in the combined trial). Host tests 44/44. On hardware: rounded rectangles in menus/dialogs still draw, DS3/DS4 over USB and BT still read input, and a BDM=0 build still opens both pad ports.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

## #4 SMS_ExitCrumb call sites and their string literals are compiled into release although the function body is empty
- issue: 0
- severity: low
- category: cleanup
- files: ['src/SMS_IOP.c', 'include/SMS_IOP.h', 'src/SMS_GUIMenuSMS.c', 'src/SMS_Player.c']
- reproduced: True
- confidence: 0.92
- risk: Very low. All crumb arguments are integer constants and string literals with no side effects, so dropping the calls cannot change release behaviour. Not run on a console.

### evidence
SMS_ExitCrumb (src/SMS_IOP.c:271-299) has an empty body unless SMS_DIAG is defined, but it is a real external function, so every call still evaluates its arguments and is emitted. `#define CRUMB( n, s ) do { if ( afExit ) SMS_ExitCrumb ( ( n ), ( s ) ); } while ( 0 )` at src/SMS_IOP.c:303 is used about 21 times in SMS_IOPReset, and there are three direct calls at src/SMS_GUIMenuSMS.c:1028, 1031, 1068. The comments at src/SMS_IOP.c:305-309 and src/SMS_Player.c:68 say the diagnostics cost nothing when off; that is false for these sites. Trial build with the calls compiled out: ELF 1,810,056 -> 1,809,160 (-896), packed 947,332 -> 946,324 (-1,008); text+rodata -624, data -320. The SMS_DIAG=1 build is unchanged in behaviour (function and calls kept).

### fix
In include/SMS_IOP.h replace the prototype at line 73 with:
#ifdef SMS_DIAG
void SMS_ExitCrumb ( int, const char* );
#else
#define SMS_ExitCrumb( n, s ) ( ( void )0 )
#endif
and wrap the definition in src/SMS_IOP.c:271-299 in `#ifdef SMS_DIAG ... #endif`.

### test
Build release and confirm with ee-nm that SMS_ExitCrumb is absent and the crumb strings are gone from the ELF; build SMS_DIAG=1 and confirm the symbol and strings are present. Host tests 44/44.

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

## #5 Comments that misdescribe the code after the icon, round-rect and diagnostics rewrites
- issue: 0
- severity: low
- category: docs
- files: ['src/SMS_GUIcons.c', 'src/SMS_GS_1.c', 'src/SMS_IOP.c', 'src/SMS_Player.c', 'tools/bake_icons_rgba.py']
- reproduced: True
- confidence: 0.85
- risk: None for the binary.

### evidence
src/SMS_GUIcons.c:25-28 says the IPU icon data stays for the player ball/desktop, but GUI_LoadIcons no longer reads any of it (see the icon finding). src/SMS_GS_1.c:608-609 describes the asm round-rect as still relevant although GS_RenderRoundRect_asm has no caller. src/SMS_IOP.c:305-309 and src/SMS_Player.c:68 state that diagnostics cost zero bytes when SMS_DIAG is off, which the measured 896-byte difference contradicts for SMS_ExitCrumb. tools/bake_icons_rgba.py lines 14 and 23 refer to the IPU arrays in SMS_GUIcons.c as a live source.

### fix
Correct or delete each comment in the same commits as the three cleanups above so the text matches the code; after the SMS_ExitCrumb macro fix the zero-cost statement becomes true and can stay.

### test
Review only; no binary change (ELF size identical).

### VERDICT

Not independently checked (below the verification cut-off, or its checker did not finish).

## #6 In-place network switch resets the IOP but restarts only the network stack; comment about dev9 reload is false (code reading only)
- issue: 0
- severity: medium
- category: bug
- files: ['src/SMS_IOP.c', 'src/SMS_GUIMenuSMS.c', 'src/SMS_Player.c', 'src/SMS_ContainerMPEG_PS.c']
- reproduced: False
- confidence: 0.4
- risk: Unknown until confirmed; any fix touches IOP reset sequencing and needs hardware verification.

### evidence
Not reproduced; no console or emulator was available and this is outside the bloat scope. SMS_IOPNetSwitch (src/SMS_IOP.c:1764-1819, called from src/SMS_GUIMenuSMS.c:1430) calls SMS_IOPReset(SMS_IOPRESET_SWITCH) and then only restarts the network stack. The comment at line 1805 says each starter re-loads dev9 itself as needed, but the starters only issue SMS_IOCtl(g_pDEV9X, DEV9CTLINIT); the PS2DEV9 module load `_load_module(1,1)` appears only at line 1447 in SMS_IOPInit, AUDSRV `_load_module(0,1)` only at 1637 and POWEROFF only at 1646. Separately, the SMSUTILS/AUDSRV/PS2DEV9/POWEROFF/CDVD images sit in the part of g_DataBuffer that is overwritten as SMS_SYNTH_BUFFER/SMS_AUDIO_BUFFER once playback has run (src/SMS_Player.c:294, src/SMS_ContainerMPEG_PS.c:919), while SMS_IOPReset reloads SMSUTILS from that region (src/SMS_IOP.c:471). If SMS_IOPRESET_SWITCH really unloads those modules, audio, dev9 and power-off would be gone after a switch, and a switch after any playback would reload from overwritten bytes.

### fix
Do not change code on this evidence alone. First confirm what SMS_IOPRESET_SWITCH actually tears down (read SMS_IOPReset for that mode); if it is a full IOP reboot, either reload dev9/audsrv/poweroff in SMS_IOPNetSwitch the way SMS_IOPInit does, or keep those images outside the region reused for audio. At minimum correct the comment at line 1805.

### test
On hardware or an emulator: boot, play a file with audio, use the network-stack switch from the menu, then check audio playback, HDD/network (dev9) access and power-off.

### VERDICT diagnosis
{
 "real": true,
 "fix_sound": false,
 "reproduced": false,
 "reasoning": "I tried to refute this and could not: the defect is real, and broader than reported. It was not run on a console or emulator; the evidence is code reading plus host inspection of the embedded IRX binaries.\n\n**What the code does (current tree, )**\n- `SMS_IOPRESET_SWITCH` is a full IOP reboot. `SMS_IOPReset` (src/SMS_IOP.c:316-514) runs `while( !SifIopReset( afExit ? \"\" : s_pUDNL, 0 ) ){}` at line 404 for any non-zero mode. It then reloads only SMSUTILS, iomanx, filexio, bdm, bdmfs_fatfs, sio2man, mcman, mcserv and padman.\n- `SMS_IOPNetSwitch` (src/SMS_IOP.c:1764-1819) then clears the flags and calls only `SMS_IOPStartNet` or `SMS_IOPStartUDPFS`.\n- The comment at line 1805 (\"Each starter re-loads dev9 itself as needed\") is false. The starters only issue `SMS_IOCtl ( g_pDEV9X, DEV9CTLINIT, NULL )` (lines 529, 936). `SMS_IOCtl` is `fioDopen` + `fioIoctl` (1843-1857), which fails when no dev9 device is registered.\n- The PS2DEV9 module is loaded in exactly one place: `_load_module ( 1, 1 )` at line 1447 in `SMS_IOPInit`. AUDSRV is loaded only at 1637, POWEROFF only at 1646, LIBSD only at 1604.\n\n**Host evidence that the new stack cannot load**\n- I compiled src/SMS_Data.c with include/SMS_Data.h on WSL gcc and scanned each embedded IRX for import/export tables (magic 0x41e00000 / 0x41c00000).\n- PS2SMAP imports `dev9`. The only exporter of `dev9` is the PS2DEV9 image.\n- A Python scan of irx/*.irx shows `udpfs_smap.irx` also imports `dev9`, and no file in irx/ exports it.\n- So after the reboot neither SMAP can resolve its imports. The UDPFS path will stop at `GUI_Error ( lE2 )` (line 947); the HOST/SMB path gets no NIC driver.\n\n**Host evidence for the buffer overlap**\n- The same harness printed: SYNTH_BUFFER at offsets 217488..221840, AUDIO_BUFFER at 221888..266560.\n- SMSUTILS occupies 217488..221221, AUDSRV 221232..230221, PS2DEV9 230224..238373, POWEROFF 238384..240613, CDVD 240624..260717.\n- So after playback with audio (src/SMS_Player.c:294) the AUDSRV, PS2DEV9, POWEROFF and CDVD images are PCM data. SMSUTILS is overwritten only after MPEG-PS playback (src/SMS_ContainerMPEG_PS.c:919; other containers use SMS_AUD_SPR), yet `SMS_IOPReset` reloads it from there at line 471.\n\n**Additional breakage the finding missed (code reading)**\n- `SIF_BindRPC` (src/SMS_SIF.c:19) returns immediately when `apData->server` is non-NULL. After a mid-session reboot every EE RPC client keeps a stale IOP server pointer and never rebinds: `s_SMSUClt` (SMS_IOP.c:509), the SPU audio/volume clients (SMS_SPU.c:45, 300), CDVD (SMS_CDVD.c:74), CDDA (SMS_CDDA.c:65) and MC (SMS_MC.c:67).\n- The first UI sound (e.g. `GUI_Error` -> `SPU_PlaySound`, SMS_GUIDesktop.c:869) would send an RPC to a dead server address on the rebooted IOP.\n- In BDM builds `PAD_Init` is `PadInitPads` (include/SMS_PAD.h:29), called once at cold start. Nothing re-opens the pad ports after the switch, so the new padman never updates `padArea` and the native controller would freeze.\n\n**Trigger**\n- BDM build (the default, Makefile:3), a network adapter, a live stack (`s_NetOwner != 0`), then picking a different network mode in the Device menu and answering Yes (src/SMS_GUIMenuSMS.c:1426-1430).\n- That is the feature's only use, and it is documented to users in docs/devices.html:115 and docs/network.html:78-80.\n\n**Provenance**\n- Fork-only: commits 9228114 and 0471035. The latter's message says \"NOT HARDWARE TESTED. This is the highest-risk change in the project\".\n- 2.9rev4 has no `SMS_IOPNetSwitch`. It has the same g_DataBuffer reuse (include/SMS_Data.h:54-56), harmless there because those modules load once at boot. Its exec exit also reloads SMSUTILS from that region \u2014 an upstream quirk, not a fork regression.\n\n**Why the proposed fix is not sound**\n- It is not concrete: two alternatives plus \"at minimum fix the comment\".\n- Reloading dev9/audsrv/poweroff \"the way SMS_IOPInit does\" would load PCM garbage after any playback, because those images sit inside AUDIO_BUFFER.\n- It does not address LIBSD, the stale RPC clients, the pad, CDVD, rmman or ds34.\n- Correcting the comment removes nothing.\n- Finishing the feature properly means re-running most of boot init and keeping about 45 KB of pristine IRX copies, which cannot be verified without hardware and is the over-engineering the owner asked to avoid.",
 "corrected_fix": "Remove the unfinished in-place switch and return to the behaviour before commit 0471035: the mode is saved and applies on the next launch.\n\n1. **src/SMS_GUIMenuSMS.c, `_switch_netmode`**: delete lines 1419-1435 \u2014 the comment starting \"If a DIFFERENT stack is already running\" and the whole `if ( SMS_IOPNetLiveMode () != SMS_NETMODE_OFF && SMS_IOPNetLiveMode () != lMode ) { if ( GUI_Question ( STR_NET_SWITCH_ASK.m_pStr ) ) { SMS_IOPNetSwitch ( lMode ); GUI_Redraw ( GUIRedrawMethod_Redraw ); } }` block. The function then ends right after `apMenu -> Redraw ( apMenu );`.\n2. **src/SMS_IOP.c**: delete lines 1732-1820 \u2014 the `#ifdef BDM` block holding the \"IN-PLACE NETWORK STACK SWITCH\" comment and `SMS_IOPNetSwitch`, through `#endif /* BDM */`. Also delete the now-unused `SMS_IOPNetLiveMode` and its comment (lines 860-882).\n3. **include/SMS_IOP.h**: delete lines 46-49 (the comment and `int SMS_IOPNetSwitch ( unsigned int );`) and line 87 (the `SMS_IOPNetLiveMode` prototype).\n4. **Leave the `SMS_IOPReset` body untouched.** With only callers 0 and 1 left, `afExit != SMS_IOPRESET_EXEC` is the same as `!afExit`, so boot and exec-exit code stay byte-identical. Dropping `#define SMS_IOPRESET_SWITCH 2` and its comment lines (include/SMS_IOP.h:36-37, 40) is optional.\n5. **Leave locale string 310** (`STR_NET_SWITCH_ASK` / `s_pNLNetSwitch`) in place, unused. Removing it means editing all 20 language packs under the exact-count rule.\n6. **Docs**: in docs/devices.html:115 and docs/network.html:78-80, replace the \"restart the network in place\" sentences with \"a changed network mode takes effect the next time SMS is launched\".\n\n**Trial result** (private copy at (investigator scratch, not kept)/verify-hunt-bloat-deadcode/netswitch/tree, steps 1-3 applied):\n- The pinned ps2dev:v1.0 build produces SMS.elf for both BDM=1 and BDM=0, with no new warnings or errors. BDM=1 is 1,809,544 bytes against a 1,810,056 baseline.\n- `tools/check_devmenu.py` prints CONSISTENT and `tools/test_display_limits.py` passes; I did not run the other host tests.\n\n**Risk on PS2**: minimal. No boot, exit or reset code changes, and it restores the mode-change behaviour that shipped before 0471035. No host regression test is worthwhile, since this is IOP sequencing.\n\nIf the owner wants an in-place switch later, it needs hardware and a real re-init: pristine IRX copies outside the audio buffer, zeroed RPC clients, and pad/MC/CDVD/SPU re-init."
}

