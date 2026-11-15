# Simple-Media-System Audit (Gemini)

- Repository: `NathanNeurotic/Simple-Media-System`
- Audited commit: `e65ced39` (`master`; tracked files: `1,012`, tracked bytes: `38,282,639 B`, `2` remote branches, `13` remote tags, local tag `rolling`)
- Goal: smaller ELF/RAM footprint, fewer crashes and hardware hazards, simpler code, cleaner repository.

---

## Host Verification Results

Observed on the clean `Simple-Media-System` checkout (`master@e65ced39`):
- `python -B tools/check_devmenu.py`: exit `0` (`BDM=1` and `BDM=0` cross-checked across `62` writes).
- `python -B -m unittest discover -s tools/sms_converter -p test_*.py`: exit `0` (`64` tests PASS, `ResourceWarning` on unclosed file at `tools/sms_converter/test_converter.py:521`).
- `python -B docs-site/build.py --check`: exit `1` (`docs/assets/ui/capture-manifest.json` Windows CRLF mismatch).
- `tools/test_*.py` batch (`59` scripts): `32` PASS, `24` FAIL (including `test_mpeg_ps_probe.py` exit `1` at `e65ced39` and `test_boot_config.py` BOM/CRLF/spaces failure on Windows), `2` TIMEOUT (`test_browser_artwork_lifecycle.py`, `test_rolling_release.py`), `1` SKIPPED (`test_converter_publication.py`).

---

## Cross-Repo Notes

Shared multi-repo duplicates and common IOP/tooling bugs are tracked **once** in `Gemini-SUMMARY.md` instead of repeating them here:
- `CROSS-003` & `CROSS-014`: Forked `smbman` IOP drivers and shared `smbman-sms` safety/concurrency/leak/dead-code bugs (including `NTLM_Password_Hash` unbounded UTF-16LE write and `O(N^2)` `strlen` in `iop/smbman-sms/src/auth.c:14,52-56`, already clamped at `> 255` in `Launchstation`/`RiptOPL`).
- `CROSS-004`: Shared `ds34bt`/`ds34usb` trees and the 4-repo `SifCallRpc` stale-buffer bug in `libds34bt.c`/`libds34usb.c`.
- `CROSS-005` & `CROSS-016`: Forked `SMSUdpfs` IOP tree (`udpfs_core`, `udpfs_ioman`, `udprdma`, `ministack`, `udpfs_smap`) and shared UDPFS/SMAP protocol/ISR hazards across repos.
- `CROSS-007`: Duplicated `.psu` / SAS packager (`tools/package_sas.py`).
- `CROSS-008`: Embedded child ELF loader overlap (`utils/Loader/loader.c`).
- `CROSS-011`: Byte-identical `AGENTS.md` and `CLAUDE.md` policy copies (`1,269 B` each).
- `CROSS-013`: Shared `SMSTCPIP` lwIP stack bugs (`tcp_slowtmr` keepalive UAF in `iop/SMSTCPIP/src/tcp.c:215,580-630`, `err_to_errno`/`lwip_accept`/`sockopt`, `tcp_input`, `udp_send`, `icmp_*`, `api_lib.c`/`api_msg.c`, `etharp_query`, `ps2ip.c`, dead `loopif.{c,h}` and `tcpip_callback`). SMS-only `SMSTCPIP` and `SMSUdpfs` bugs stay locally numbered below.

---

## Findings (Sorted by Benefit)

### SMS-001
- Title: `1.15 MB` stack array in `_find_signature`, three `CDDA` NULL dereferences, two leaks, and dead `CDDA_Get*Picture` exports
- Category: 4 (Size and memory), 6 (Bugs and security), 1 (Dead code)
- Location(s): `Simple-Media-System/src/SMS_FileContext.c:320-360,766-798,824-918,1008-1018,1091-1095,1140-1145,1228-1239,1976-1978`, `Simple-Media-System/include/SMS_FileContext.h:135-136`, `Simple-Media-System/src/SMS_FileDir.c:420`
- Problem: `_find_signature` (`322`) declares `u8 lBuf[ 2352 * 512 ]` (`1,204,224` B, ~`1.15 MB`) on the EE thread stack while scanning up to `256` CD sectors. Additionally, `CDDA_DirectoryList` (`1091-1095`) checks `apCtx != NULL` and then dereferences `((CDDAFileContext*)apCtx->m_pData)->m_pNodes` even when `CDDA_LoadDirectoryList` (`830,836,883`) leaves `apCtx->m_pData == NULL`; `CDDA_InitFileContext` (`1228-1239`) dereferences unchecked `retVal`/`retVal->m_pPath`; `CDDA_InitFileContextInternal` (`766-798`), `CDDA_LoadDirectoryList` (`834-836`), `CDDA_InitContext` (`1008-1009`), `CDDA_GetFileList` (`1140-1142`), and `_WIN32` `STIO_InitFileContext` (`1976-1978`) leak or dereference unchecked `malloc` results; and `CDDA_GetPicture` (`887-905`) / `CDDA_GetDiskPicture` (`907-918`) have `0` external callers.
- Fix: Stream `_find_signature` through a small multi-sector buffer (or short-lived heap buffer freed before return), guard `apCtx->m_pData` and `malloc`/`retVal` returns with proper cleanup (`retVal->m_pData`, `lpCtx`), and delete dead `CDDA_GetPicture` and `CDDA_GetDiskPicture`.
- Justification: Removing the `1.15 MB` stack frame prevents EE stack overflow into heap/BSS, and checking OOM/NULL paths prevents immediate crashes/leaks.
- Benefit: Removes `1,204,224` B of EE stack pressure (`INFERRED`), fixes `3` NULL-deref paths and `2` leaks, and deletes `2` dead exported functions (~`32` lines, `INFERRED`).
- Effort: M. Risk: Low. Confidence: High (READ for paths/sizes; INFERRED for stack overflow runtime behavior)

### SMS-002
- Title: `180 KB` unused `.bss` buffer (`s_Samples`) and full-size zeroing in AC3 decoder
- Category: 4 (Size and memory)
- Location(s): `Simple-Media-System/src/SMS_AC3.c:323,360,2305-2481`
- Problem: `static sample_t s_Samples[ 16 * 256 * 12 ]` reserves `196,608` B (`192` KB) in `.bss` and `AC3_Init` (`360`) zeroes the entire buffer. In `_ac3_decode_frame` (`2305-2481`), with max `lnFChans == 5` (`s_nFChansTbl[7] == 5`) plus optional `AC3_LFE` offset (`256`), maximum accessed element index is `256 + 1536 + 1024 + 255 = 3071` (`12,288` B), leaving `46,080 * 4 = 184,320` B (`180` KB) permanently unused.
- Fix: Shrink `s_Samples` from `16 * 256 * 12` to `256 * 12` elements (`3,072` `sample_t`, `12` KB).
- Benefit: Saves `184,320` B (`180` KB) of static EE `.bss` RAM and eliminates `184,320` B of `memset` traffic on `AC3_Init`.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-003
- Title: `162.0 KiB` permanent static WMA `.bss` context (`s_Ctx`) plus OOB index, infinite loop, and unchecked `malloc`/`MDCT` bugs
- Category: 4 (Size and memory), 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_WMA.c:1125-1188,1206,1352-1354,1419,1539-1541,1645-1660,1926-1931,2047-2098`
- Problem: `static WMADecodeContext s_Ctx;` (`1188`) permanently pins `165,892` B (`162.0 KiB`) in EE `.bss` even when WMA is not active. In addition, `wma_decode_block:1927` reads `exponents[(( -1 << bsize )) >> esize]` (`exponents[-1]` OOB read); `wma_decode_superframe` (`2047-2098`) spins infinitely if `block_align == 0`, reads past packet bounds if `lBufSize < lFrmSize` or `(pos >> 3) > lFrmSize`, and unconditionally returns `0`; `wma_init:1352-1354` can set `exponent_bands[0][i] = 0`, causing `decode_exp_vlc` (`1645-1660`) `while (--n)` to wrap to `0xFFFFFFFF`; and `init_coef_vlc:1206`, `wma_init:1419`, and `wma_decode_init:1539-1541` leave `malloc`/`DSP_MDCTInit` failures unchecked.
- Fix: Allocate `WMADecodeContext` dynamically on WMA init and free on destroy (or share a codec scratch arena), replace `exponents[-1]` with a non-negative scale index, validate `block_align > 0 && lBufSize >= lFrmSize` and `pos >> 3 <= lFrmSize`, guard zero `n` in `decode_exp_vlc`, and check `malloc`/`DSP_MDCTInit` returns.
- Justification: Dynamic lifetime allocation reclaims `~162 KiB` outside WMA playback, and bounds/OOM checks prevent infinite loops and heap corruption on malformed WMA streams.
- Benefit: Reclaims `165,892` B (`162.0 KiB`) permanent EE `.bss` outside WMA playback and fixes 4 concrete decode crashes/OOB reads (`INFERRED`).
- Effort: M. Risk: Med. Confidence: High (READ)

### SMS-004
- Title: `memmove` backward byte-tail loop (`small_byte_rmemcpy`) increments forward and corrupts memory
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_Utils.S:63-104`, `Simple-Media-System/src/SMS_GUISMBrowser.c:359,367,376,386,393`
- Problem: When `dst > src` with overlapping regions, `memmove` enters `small_rmemcpy` (`75`) and `small_byte_rmemcpy` (`91`) with `$a0 = dst + rem`, `$a1 = src + rem`, and `$a2 = rem`. Lines `97-99` execute `addiu $a0, $a0, 1`, `addiu $a1, $a1, 1`, and `sb $t0, -2($a0)`, stepping forward instead of backward so bytes `0..rem-2` remain uncopied and subsequent iterations overwrite memory past `dst + rem` (including `< 8`-byte right shifts near the end of `s_KbBuf` in `SMS_GUISMBrowser.c:367`).
- Fix: Decrement `$a0` and `$a1` by `-1` per iteration in `small_byte_rmemcpy` (`97-99`) and store `$t0` at `0($a0)` instead of `-2($a0)`.
- Benefit: Eliminates heap/BSS corruption on overlapping right-shift `memmove` calls with zero added instructions.
- Effort: S. Risk: Low. Confidence: High (READ for instruction trace; INFERRED for runtime symptom)

### SMS-005
- Title: AAC/SBR OOB array, Huffman, and PCE writes, `sbrReset` half-clear, stale `is_used`, `qsort` bug, and `pcm_synN` aliasing
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_AAC.c:3631-3632,3734-3735,3928,4024-4046,4870,4883-4907,5128,5144-5156,5303-5305,5841-5905,5931,6036,6040,6098,6121,6204,6218,6313-6320,6337,6357-6374,6464-6467,6531-6534,6570-6572,6739-6778,7548-7561,7611-7661,7807-7815,7832,7856-7865,7917,7932-7960`, `Simple-Media-System/src/SMS_Utils.S:556-611`
- Problem: `program_config_element` (`5841-5905`) writes past `cpe_channel`/`sce_channel` up to `93` channels before checking `MAX_CHANNELS`, and callers (`5931,7917`) ignore its error (`decode_cpe:7807-7815` also commits `internal_channel`/`fr_channels` on error); `m_Element` retains stale `is_used = 1` across elements (`7832`); `sbrReset` (`3631-3632`) zeros `Xsbr` using `sizeof(float)` instead of `sizeof(qmf_t)`; SBR table builders write `vDk1[-1]`, divide by `0`, write index `64` past size-64 arrays, sort `nrLim` instead of `nrLim + 1` in `qsort` (`5303-5305`), scan `f_master` unbounded, and overflow `Q_div`/`patchNoSubbands`; core AAC paths read `_aac_iq_table` (`8192`) with 16-bit indices, read Huffman tables before bounds checks, use unbounded shift `1 << i`, index `[sf_index]` unchecked, overrun `sect_start`/`exclude_mask`, underflow `uint16_t count` in `fill_element`, and pass unaligned/3ch-4ch `internal_channel` to `pcm_synN` (`SMS_Utils.S:556-611`).
- Fix: Enforce bounds across PCE, SBR frequency/limiter/patch/gain tables, `vquant` (`< 8192`), 2-step Huffman tables, `huffman_getescape`, `sf_index < 12`, `section_data`, `excluded_channels`, `fill_element`, and `AudioSpecificConfig2`; zero `noise_used`/`is_used` on element decode; fix `sizeof(qmf_t)` in `sbrReset` and `nrLim + 1` in `qsort`; validate 5ch layout and `% 4 == 0` sample counts before `pcm_synN`.
- Justification: Prevents OOB memory corruption, UB shifts/loops, stale cross-frame failures, and 3ch/4ch surround aliasing.
- Benefit: Resolves 18+ AAC/SBR OOB, corruption, and audio-mixing defects (`INFERRED` runtime crash/audio impact).
- Effort: L. Risk: Med. Confidence: High (READ)

### SMS-006
- Title: Heap buffer overflow (`ogg_sync_buffer(4096)`) and Vorbis header/table bounds bugs in `SMS_OGG.c`
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_OGG.c:64,90,2565-2588,3102-3118,3419-3454,3526-3541,3871,3882,3982-3983,4921-4923,5150-5157,5194-5203,5228-5246,5492-5493,5515-5518`
- Problem: `_decode_start` (`5150-5152`), `_decode_hdr` (`5194-5203`), and `_decode_pcm` (`5515-5518`) request `4096` bytes from `ogg_sync_buffer` and immediately `memcpy` `lnBytes = lpPck->m_Size` bytes into `m_pBuf`, overflowing the heap buffer whenever a packet exceeds `4096` bytes (`ogg_sync_buffer:2577` also dereferences unchecked `realloc`, and `5156` rejects valid small initial packets `< 4096`). Further, `_decode_hdr:5228-5230` ignores `vorbis_synthesis_headerin` errors; `_vorbis_unpack_info` permits `> 2` channels and blocksizes `< 64` or `> 8192` (`vwin[8]` OOB at `4921-4923`, `SMS_RingBufferAlloc` unchecked/overflowed at `5492-5493`); `floor1_unpack` can write `248` items into `postlist[65]`; `res0_unpack` can write `512` items into `booklist[256]`; and `vorbis_book_init_decode` uses unbounded stack `alloca`.
- Fix: Pass `lnBytes` to `ogg_sync_buffer` and check `NULL` on `ogg_sync_buffer`/`realloc`/`SMS_RingBufferAlloc`, drop the `m_Size < 4096` check, propagate `vorbis_synthesis_headerin` errors, reject `channels > 2` and blocksizes outside `64..8192`, cap `floor1` (`<= 63`) and `res0` (`<= 256`), and replace unbounded `alloca` with checked heap scratch freed on return.
- Justification: Prevents heap/stack corruption, OOB table writes, and OOM crashes on malformed or large-page Ogg Vorbis streams.
- Benefit: Eliminates 4 concrete OOB/overflow corruption bugs and unchecked OOM/error paths in Ogg Vorbis playback (`INFERRED`).
- Effort: M. Risk: Low. Confidence: High (READ)

### SMS-007
- Title: SMS-only `SMSTCPIP` `ip_reass` byte-swap overrun (`61 KB` into `1540 B` pbuf) and `u16_t` bitmap underflow
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/iop/SMSTCPIP/src/ip_frag.c:192,198-224`, `Simple-Media-System/iop/SMSTCPIP/src/pbuf.c:176-187`
- Problem: `ip_reass` byte-swaps `ip_reasslen` before passing it to `pbuf_alloc`/`mips_memcpy` (turning a normal `1520`-byte datagram length into `61,445` bytes and overflowing `ip_reassbuf[5780]` plus the `1540`-byte `PBUF_POOL` buffer). At `ip_frag.c:192`, `ip_reasslen / 64 - 1` also underflows in `u16_t` whenever `ip_reasslen < 64`, indexing past `ip_reassbitmap[90]`.
- Fix: Port the fix already present in `RiptOPL/modules/network/SMSTCPIP/ip_frag.c:207-249` (allocating/copying host-order `ip_reasslen` and guarding `< 64` bitmap bounds) and rebuild the embedded `SMS_PS2IP` blob in `src/SMS_Data.c`.
- Justification: Fixes a deterministic IOP buffer overflow on fragmented IP packets using proven code already verified in sibling `RiptOPL`.
- Benefit: Prevents IOP memory corruption and crash during fragmented IP traffic (`READ` mismatch; `INFERRED` crash avoidance).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-008
- Title: Uninitialized semaphore struct and unchecked heap/ring-buffer/SPR allocations in core playback and demuxers
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_IPU.c:702-708`, `Simple-Media-System/src/SMS_DXSB.c:274,313-315`, `Simple-Media-System/src/SMS_MPEG12.c:236-238`, `Simple-Media-System/src/SMS_PCM.c:26,64,80`, `Simple-Media-System/src/SMS_MPEGInit.c:48-60`, `Simple-Media-System/src/SMS_MPEG.c:42-50`, `Simple-Media-System/src/SMS_OGG.c:2707-2749`, `Simple-Media-System/src/SMS_AAC.c:3670-3744`, `Simple-Media-System/src/SMS_GUIMenuSMS.c:425`, `Simple-Media-System/src/SMS_ContainerJPG.c:64`, `Simple-Media-System/src/SMS_ContainerFLAC.c:123,126,138`, `Simple-Media-System/src/SMS_ContainerAAC.c:197,200,212`, `Simple-Media-System/src/SMS_ContainerAC3.c:109,112`, `Simple-Media-System/src/SMS_ContainerOGG.c:105,108`, `Simple-Media-System/src/SMS_ContainerMP3.c:121,124`, `Simple-Media-System/src/SMS_ContainerAVI.c:1164`, `Simple-Media-System/src/SMS_ContainerASF.c:157`
- Problem: `IPU_InitContext` (`SMS_IPU.c:702-708`) only assigns `lSema.init_count = 1` on a stack `ee_sema_t`, passing uninitialized `max_count`/`attr`/`option` fields to `CreateSema`. Furthermore, `SMS_MPEG12.c`, `SMS_DXSB.c`, and `SMS_PCM.c` dereference `SMS_RingBufferAlloc` unchecked (`SMS_DXSB.c:274` also `mips_memset`s `SMS_SPR_FREE` without clamping to remaining SPR capacity), and numerous codec/container/UI allocations dereference `malloc`/`calloc`/`realloc` unchecked.
- Fix: Zero-initialize `lSema` (`memset(&lSema, 0, sizeof(lSema))`) with explicit `max_count`, add `NULL` checks after allocation/ring-buffer calls in these core paths, and clamp DXSB writes to SPR capacity.
- Justification: Prevents undefined kernel semaphore behavior and EE TLB crashes under 32 MB RAM pressure.
- Benefit: Eliminates uninitialized kernel syscall arguments and unchecked-OOM dereferences across core playback paths.
- Effort: M. Risk: Med. Confidence: High (READ)

### SMS-009
- Title: Unchecked allocations and `_NeAACDecOpen`/`sbrDecodeInit`/`SMS_RingBufferAlloc` dereferences across all 5 audio codecs
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_Codec.c:92-123`, `Simple-Media-System/src/SMS_RingBuffer.c:90,124`, `Simple-Media-System/src/SMS_AAC.c:6703,6718,6725,6733,7418-7425,7444,7448,7461,7464,7466,7468,7651-7656,7729-7736,8238,8290-8301`, `Simple-Media-System/src/SMS_AC3.c:339,2604,2623`, `Simple-Media-System/src/SMS_DTS.c:9975,11585,11647`, `Simple-Media-System/src/SMS_FLAC.c:391,399,529,546,587`, `Simple-Media-System/src/SMS_MP123.c:4753,4940,5232,6408`
- Problem: Outside `SMS-008`, `SMS_Codec.c:92-123` assumes `SMS_Codec_*_Open` leaves `apCtx->m_pCodec == NULL` on failure, yet all five codec open functions dereference `calloc(1, sizeof(SMS_Codec))` immediately without checking `NULL` (`SMS_AAC.c:8293,8300` also dereferences `_NeAACDecOpen()` unchecked). Every audio codec also dereferences `SMS_RingBufferAlloc` (returns `NULL` when `aSize > m_Capacity` in `SMS_RingBuffer.c:90,124`) unchecked, and `SMS_FLAC.c`/`SMS_AAC.c` leave 15+ `malloc`/`realloc`/`sbrDecodeInit` pointers unguarded.
- Fix: Check every `calloc`, `malloc`, `realloc`, `_NeAACDecOpen`, `sbrDecodeInit`, and `SMS_RingBufferAlloc` return at these 30+ codec call sites, freeing partial allocations and leaving `apCtx->m_pCodec` NULL on failure.
- Justification: Prevents immediate NULL-pointer crashes under low EE memory or oversized frame/ring-buffer requests.
- Benefit: Eliminates unchecked NULL dereferences at 30+ allocation sites across `AAC`, `AC3`, `DTS`, `FLAC`, and `MP123` (`INFERRED`).
- Effort: M. Risk: Low. Confidence: High (READ)

### SMS-010
- Title: FLAC decoder OOB channel writes/calls, negative-index predictor read, UAF/hang on `MaxFrameSz == 0`, and shift UB
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_FLAC.c:40,107,155-157,176-184,277,322,370-372,399,438-443,490-512,569-577`
- Problem: `s_Ctx` sizes `m_pDec[2]` and `_synth_indep[2]` for 2 channels while `_decode_frame` (`322`) permits up to `8` channels, causing OOB writes to `m_nAlloc`/`m_nAllocFrm` (`370-372`) and OOB function-pointer calls (`441`). `decode_subframe_fixed` (`176-184`) reads `m_pDec[0][-4..-1]` before switching on `pred_order`; `m_MaxFrameSz == 0` causes `realloc(..., 0)` UAF plus an infinite `lSize -= 0` loop (`399,490-512,569-577`); `get_ur_golomb_jpegls:107` ignores `limit`; and wasted bits reducing `m_CurBPS <= 0` cause `32 - 0` shift UB in `decode_residuals:156-157`.
- Fix: Reject `m_nChannels > 2` in `_decode_frame`, read `decoded[pred_order - k]` inside `pred_order` cases, enforce non-zero `MaxFrameSz`/`lISize > 0`, apply `limit` in `get_ur_golomb_jpegls`, and guard `m_CurBPS > 0` / `tmp >= 1`.
- Justification: Prevents struct corruption, OOB function-pointer calls, UAF, infinite decode hangs, and shift UB.
- Benefit: Fixes 5 critical safety, crash, and hang bugs in `SMS_FLAC.c` (`INFERRED`).
- Effort: M. Risk: Low. Confidence: High (READ)

### SMS-011
- Title: EE/VU0 `lq`/`sqc2` alignment hazards, mutated read-only asm operands without `"memory"`, and missing `jr $ra` delay `nop`s
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_AC3.c:325,2492,2531,2636-2661`, `Simple-Media-System/include/SMS_AC3.h:82-130`, `Simple-Media-System/src/SMS_DTS.c:11666-11691`, `Simple-Media-System/src/SMS_AAC.c:6823-6826,7194-7253,7365,7670-7671`
- Problem: `quant_to_spec` (`SMS_AAC.c:6823-6826`) executes `sqc2 $vf1, 0(%0)` without `"=m"`/`"memory"` into unaligned stack arrays `spec_coef`/`spec_coef1`/`spec_coef2` (`R5900` `sqc2` masks the low 4 address bits, overwriting adjacent stack bytes); `s_AC3Ctx` (`SMS_AC3.c:325`) omits `aligned(16)` while `_ac3_spdif` executes `lq` (`2640-2641`); `_ac3_spdif` and `_dts_spdif` mutate read-only `"r"` inputs without `+r` or `"memory"`; and `_ac3_float_to_int_1` (`2531`) plus `_overlap_0` (`SMS_AAC.c:7251`) end with `jr $ra` inside `.set noreorder` without a `nop`, executing the next function's first instruction in the delay slot.
- Fix: Add `aligned(16)` on `s_AC3Ctx` (or `m_InBuf`), `spec_coef`, `spec_coef1`, and `spec_coef2`; use `+r` and `"memory"` clobbers in S/PDIF inline asm; insert `nop` after `jr $ra` at `SMS_AC3.c:2531` and `SMS_AAC.c:7251`.
- Justification: Prevents R5900 128-bit address masking corruption, compiler misoptimizations, and wrong delay-slot instructions.
- Benefit: Eliminates 5 hardware/ABI corruption hazards across AC3, DTS, and AAC (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-012
- Title: `_pcm_syn1`/`_pcm_syn2` EE MMI bugs (`jr $ra` missing `nop`, stale `mtsab` on channel 1, +16 B OOB `lq` read)
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_OGG.c:5257-5365`
- Problem: Inside `.set noreorder`/`.set nomacro`/`.set noat`, `_pcm_syn1` ends (`5324-5325`) with `jr $ra` directly before `_pcm_syn2` with no `nop`, running `_pcm_syn2`'s first instruction in the delay slot; `_pcm_syn2` sets `mtsab $a1, 0` (`5335`) once for channel 0 and runs `qfsrv` on channel 1 (`$a3`, `5354-5362`) using `$a1`'s stale shift offset whenever channel alignments mod 16 differ; and both routines look ahead with `lq $t4, 64($a1)`/`64($a3)`, reading up to `16` bytes past the end of the 64-byte slice.
- Fix: Insert `nop` after `_pcm_syn1`'s `jr $ra`, issue `mtsab $a3, 0` before channel 1's `qfsrv` (and reload `mtsab $a1, 0` for the next loop step), and avoid speculative `64($a1)`/`64($a3)` lookahead on the final iteration or aligned pointers.
- Justification: Fixes real right-channel stereo misalignment and removes OOB quadword reads in EE MMI Vorbis synthesis.
- Benefit: Fixes Vorbis right-channel audio corruption when channel buffer alignments differ (`INFERRED`) and eliminates OOB `lq` reads.
- Effort: S. Risk: Med. Confidence: High (READ)

### SMS-013
- Title: `SMS_GS_1.c`/`SMS_EE.c` display-list alignment mismatch (`56 B` OOB read), `realloc` leak, DMA race, and dangling `m_pLastTag`
- Category: 6 (Bugs and security), 2 (Duplication)
- Location(s): `Simple-Media-System/src/SMS_GS_1.c:75,186-195,205-215,248-263,312-318,335-341`, `Simple-Media-System/src/SMS_EE.c:55-89`
- Problem: `GSContext_CallList` and `GSContext_CallList2` duplicate `m_nAlloc` growth without `DMA_Wait(DMAC_VIF1)` and request unaligned `+514` qwords (`4112` B) while `GSContext_NewPacket` aligns sizes via `(... + 63) & ~63`; when `NewPacket` grows a list previously grown by `CallList*`, `SMS_ReallocWithAlign` (`SMS_EE.c:71-74`) rounds `lnOldSize` up by up to `56` bytes past the allocation and `memcpy`s out of bounds. `SMS_ReallocWithAlign:68` also leaks `apData` on OOM and runs a redundant `realloc` (`71-76`), while `GSContext_Flush` dereferences `m_pLastTag[aCtx]` which is never reset in `Init`/`Flush` or updated when `aDWC == 0` (`GS_DisplayOffsets:75` also divides by `lW` unchecked).
- Fix: Unify display-list growth into one helper with `DMA_Wait(DMAC_VIF1)` and 64-byte alignment, preserve `apData` around `realloc` and drop the redundant second `realloc` in `SMS_ReallocWithAlign`, clear `m_pLastTag[aCtx]` in `Init`/`Flush` and guard `lpTag != NULL`, and guard `lW > 0`.
- Justification: Prevents heap OOB reads, OOM leaks, VIF1 DMA corruption, and stale `m_pLastTag` dereferences while removing duplicated growth blocks.
- Benefit: Fixes display-list memory safety and DMA hazards and deduplicates growth code (~`15` lines reduced, `INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ for mismatch/math; INFERRED for heap/DMA consequences)

### SMS-014
- Title: Large decoder frame arrays (`12,288 B` AAC CPE and `9,216 B` MP3) pinned on EE thread stacks
- Category: 4 (Size and memory)
- Location(s): `Simple-Media-System/src/SMS_AAC.c:7670-7671,7752-7754,7822-7832`, `Simple-Media-System/src/SMS_MP123.c:6429-6430`
- Problem: `channel_pair_element` (`7752-7754`) keeps `spec_data1[1024]` and `spec_data2[1024]` (`4,096` B) on stack while calling `reconstruct_channel_pair` (`7670-7671`), which allocates `spec_coef1[1024]` and `spec_coef2[1024]` (`8,192` B) for `12,288` B of concurrent stack pressure. In `SMS_MP123.c:6429-6430`, `_mp3_decode_frame` puts `sl_HybridIn[2][32][18]` and `sl_HybridOut[2][18][32]` (`9,216` B total) on the stack.
- Fix: Move these per-frame temporary spectral/hybrid buffers into the decoder context (or shared active codec scratch buffer) or overlap non-concurrent lifetimes.
- Justification: Avoids stack overflow on small EE thread stacks without per-frame heap churn.
- Benefit: Cuts peak EE stack frame depth by `12,288` B (AAC CPE) and `9,216` B (MP3) (`INFERRED` stack safety).
- Effort: M. Risk: Med. Confidence: High (READ)

### SMS-015
- Title: DTS core OOB channel/subband/scalefactor/VQ indexing and `5,120 B` stack allocation
- Category: 6 (Bugs and security), 4 (Size and memory)
- Location(s): `Simple-Media-System/src/SMS_DTS.c:26,189,1127,10756-10780,11168,11329-11440`
- Problem: `_dts_subframe_header` (`10756`) computes `prim_channels = bitstream_get(state, 3) + 1` (`1..8`) without clamping to `DTS_PRIM_CHANNELS_MAX` (`5`), writing past size-5 arrays (`subband_activity`..`bitalloc_huffman`). `_dts_subsubframe` also indexes `bitalloc_12[5]` up to `[7]` (`11340-11346`), indexes `scale_factor_quant6`/`scale_factor_quant7` with unchecked `scale_sum` (`11375-11393`) and an 8-bit `0..255` VQ value (`11440`), indexes `subband_activity[5]` with `joint_intensity[j] - 1` up to `6` (`11405-11408`), and allocates `5,120` B (`subband_samples[5][8][32]`) on the stack (`11168`).
- Fix: Validate `prim_channels <= DTS_PRIM_CHANNELS_MAX`, `bitalloc_huffman[j] < 5`, `source_channel < DTS_PRIM_CHANNELS_MAX`, and scalefactor indices `< 64` / `< 128`, and move `subband_samples` into `DTSContext` or codec scratch.
- Justification: Prevents OOB table/struct corruption on malformed DTS bitstreams and reduces `_dts_subsubframe` stack pressure.
- Benefit: Eliminates 5 OOB indexing bugs and `5,120` B of stack allocation (`INFERRED`).
- Effort: M. Risk: Low. Confidence: High (READ)

### SMS-016
- Title: SMS-only `SMSUdpfs/smap` `smap_fifo_read` drops trailing byte on odd-length UDPFS reads
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/iop/SMSUdpfs/smap/src/xfer.c:144-149`
- Problem: `smap_fifo_read` exits its read loop on `i + 2 <= bytes`, silently dropping the final byte of every odd-length UDPFS/SMAP RX transfer (`Launchstation` and `RiptOPL` already carry the `u32 tail = SMAP_REG32(SMAP_R_RXFIFO_DATA)` fix at `modules/network/udpfs_smap/src/xfer.c:144-156`).
- Fix: Port the trailing-byte `tail` extraction fix from `RiptOPL`/`Launchstation`'s `xfer.c` and rebuild `irx/udpfs_smap.irx.xz` via `tools/build_udpfs.sh`.
- Justification: Minimal 3-line fix resolves silent odd-length packet truncation in `SMSUdpfs`.
- Benefit: Fixes odd-length UDPFS/SMAP RX packet corruption (`READ` discrepancy; `INFERRED` transfer bug).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-017
- Title: SMS-only `SMSTCPIP` socket bounds/`NULL` deref, `--aTimeout` wait corruption, and missing `MEMP_SYS_TIMEOUT` pool entry
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/iop/SMSTCPIP/src/sockets.c:104-115,235-254`, `Simple-Media-System/iop/SMSTCPIP/src/ps2ip.c:413-414,454-457`, `Simple-Media-System/iop/SMSTCPIP/src/memp.c:53-77`, `Simple-Media-System/iop/SMSTCPIP/include/lwip/memp.h:53`
- Problem: In SMS's `SMSTCPIP` copy, `get_socket(int aSD)` omits `aSD < 0 || aSD >= NUM_SOCKETS` (`15`), and `lwip_close` (`250`) calls `sock_set_errno(sock, EBADF)` unconditionally when `sock == NULL` (`RiptOPL/modules/network/SMSTCPIP/sockets.c:118-133,263-286` already fixes both). Meanwhile `ps2ip.c:454-457` returns `--aTimeout` instead of elapsed milliseconds from `sys_arch_sem_wait`, corrupting `sys_arch_mbox_fetch` spin waits on large timeouts, and `memp_sizes`/`memp_num` (`memp.c:53-77`) define `10` entries while `MEMP_MAX == 11` (`MEMP_SYS_TIMEOUT` left zeroed).
- Fix: Port the `get_socket`/`lwip_close` bounds and `NULL` checks from `RiptOPL/modules/network/SMSTCPIP/sockets.c`, return elapsed ms (not `--aTimeout`) from `sys_arch_sem_wait`, and match `memp_sizes`/`memp_num` length to `MEMP_MAX` (`11`), then rebuild `SMS_PS2IP`.
- Justification: Prevents IOP NULL dereferences on invalid sockets, corrupted mbox timeout spinning, and zeroed `MEMP_SYS_TIMEOUT` pool sizes.
- Benefit: Fixes 3 IOP lwIP crash/hang bugs unique to SMS's `SMSTCPIP` copy (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-018
- Title: Dead embedded IRXs (`SMSSMB.IRX`, `!BDM` `UMS.IRX`/`USBD.IRX` in `BDM=1`), unbuilt IOP trees, and unused lwIP headers
- Category: 1 (Dead code), 4 (Size and memory), 7 (Build/CI/repo clutter)
- Location(s): `Simple-Media-System/src/SMS_Data.c:5480-6589,12412-13642`, `Simple-Media-System/include/SMS_Data.h:17-18,24,35-36,42-43`, `Simple-Media-System/src/SMS_IOP.c:584-605,636-693`, `Simple-Media-System/Makefile:3,50-59`, `Simple-Media-System/iop/SMSSMB/`, `Simple-Media-System/iop/SMSUMS/`, `Simple-Media-System/iop/USBD/`, `Simple-Media-System/iop/SMSCDVD/`, `Simple-Media-System/iop/SMSMAP/`, `Simple-Media-System/iop/SMSPWROFF/`, `Simple-Media-System/iop/SMSUdpfs/ministack/src/udptty.c`, `Simple-Media-System/iop/SMSUdpfs/ministack/include/udptty.h`, `Simple-Media-System/iop/SMSTCPIP/include/lwip/dhcp.h`, `Simple-Media-System/iop/SMSTCPIP/include/lwip/snmp.h`, `Simple-Media-System/iop/SMSTCPIP/include/lwip/raw.h`
- Problem: `SMSSMB.IRX` (`SMS_SMB_SIZE = 19,641` B) in `g_DataBuffer` and `iop/SMSSMB/` (`4` files, `112,522` B) have zero callers (`BDM=1` loads `smbman_irx` and `#else` sets `i = -1`). `UMS.IRX` (`8,504` B) and `USBD.IRX` (`9,140` B) are only referenced inside `#else` (`!BDM`) of `SMS_IOPStartUSB` yet compile into default `BDM=1` ELFs. Meanwhile `iop/SMSUMS/` (`11` files, `75,430` B), `iop/USBD/` (`19` files, `95,652` B), `iop/SMSCDVD/` (`6` files, `65,248` B), `iop/SMSMAP/` (`7` files, `44,761` B), and `iop/SMSPWROFF/` (`5` files, `3,377` B) (`396,990` B total) are never built by top-level `Makefile`, alongside dead `ministack` `udptty.{c,h}` (`2,105` B) and unused lwIP headers `dhcp.h`/`snmp.h`/`raw.h` (`16,298` B).
- Fix: Delete `SMSSMB.IRX` data/`SMS_SMB_*` macros and chain `SMS_SMSUTILS_OFFSET` directly; wrap `UMS.IRX`/`USBD.IRX` data in `#ifndef BDM` (preserving 16-byte module and 64-byte audio alignment); delete `iop/SMSSMB/`, `udptty.{c,h}`, and `dhcp.h`/`snmp.h`/`raw.h` immediately, and remove `SMSUMS`/`USBD`/`SMSCDVD`/`SMSMAP`/`SMSPWROFF` if legacy `!BDM` builds are retired.
- Benefit: Removes `19,641` B (all builds) / `37,285` B (`BDM=1`) from ELF `.data`, ~`1,750` lines in `SMS_Data.c`, and `130,925` B (`415,393` B if `!BDM` retired) of dead IOP source.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-019
- Title: Dead `377.6 KB` vendored IOP image header `include/s_iop_image.h`
- Category: 1 (Dead code), 4 (Size and memory)
- Location(s): `Simple-Media-System/include/s_iop_image.h:1-3820`, `Simple-Media-System/src/SMS_IOP.c:228,437`
- Problem: `include/s_iop_image.h` (`377,604` B, `3,820` lines) defines `static unsigned char s_iop_image[]`, but its only references (`#include "s_iop_image.h"` and `SifIopReset(s_iop_image, 0)` in `src/SMS_IOP.c:228,437`) are commented out.
- Fix: Delete `include/s_iop_image.h` and remove the two commented lines in `src/SMS_IOP.c`.
- Benefit: Removes `377,604` B and `3,820` lines of dead binary array clutter and prevents accidental `377.6 KB` `.data` inclusion.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-020
- Title: Legacy IPU icon arrays, tables, and `IPU_UnpackImage` left behind after XZ RGBA migration
- Category: 1 (Dead code), 4 (Size and memory)
- Location(s): `Simple-Media-System/src/SMS_GUIcons.c:25-1368`, `Simple-Media-System/src/SMS_PlayerBallSim.c:26-27,57-63`, `Simple-Media-System/src/SMS_IPU_1.c:220-324`, `Simple-Media-System/include/SMS_IPU.h:140`
- Problem: `GUI_LoadIcons` and `SMS_PlayerBallSim_Init` now decode `g_RGBA_Blob` (`SMS_IconsRGBA.c`) and `g_IconBallRGBA` (`SMS_BallRGBA.c`) instead of IPU blobs, leaving `g_IconBall[316]`, `27` `s_Icon*` arrays, `6` icon pointer/size tables (`s_pBrowserIcons`, `s_SizeBrowserIcons`, `s_pMiscIcons`, `s_SizeMiscIcons`, `s_pBrowserDevIcons`, `s_SizeBrowserDevIcons`), stale `extern g_IconBall` (`SMS_PlayerBallSim.c:26`), and `IPU_UnpackImage` unused.
- Fix: Delete `g_IconBall`, all `27` `s_Icon*` arrays, the `6` legacy pointer/size tables, the stale `extern`, and `IPU_UnpackImage` (declaration and definition).
- Benefit: Removes `19,216` B of dead `.data` arrays plus tables, ~`740` B `.text` (`IPU_UnpackImage`, `INFERRED`), and ~`1,440` lines.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-021
- Title: `2.06 MB` of committed AI handoffs, redundant/unused IOP binaries, stale `lng/` packs, and orphan `vu/` objects
- Category: 7 (Build/CI/repo clutter), 1 (Dead code)
- Location(s): `Simple-Media-System/docs/investigations/`, `Simple-Media-System/irx/bdmfs_vfat.irx.xz`, `Simple-Media-System/irx/mmcedrv.irx`, `Simple-Media-System/irx/mmcedrv.irx.xz`, `Simple-Media-System/lng/`, `Simple-Media-System/vu/`
- Problem: `docs/investigations/` holds `54` committed AI handoff logs (`1,926,290` B); `irx/bdmfs_vfat.irx.xz` (`12,540` B), `irx/mmcedrv.irx` (`11,337` B), and `irx/mmcedrv.irx.xz` (`3,904` B) have zero references in `Makefile` or `src/SMS_IOP.c` (`19` raw `.irx` files also duplicate `.irx.xz` in `irx/`, `494,378` B total); `lng/` (`20` files, `104,459` B) holds obsolete 269/270-line translation packs rejected by `SMS_Locale.c:928` (`311` lines required); and `vu/` (`7` files, `9,169` B) holds `CSC*.o`/`IDCT.o` never linked by `Makefile`.
- Fix: Delete `docs/investigations/`, `irx/bdmfs_vfat.irx.xz`, `irx/mmcedrv.irx*`, `vu/*.o`, and `lng/` (moving any still-needed credits from `lng/README.txt` to `docs/translation/`), and generate `.irx.xz` at build time if offline raw `.irx` recompression is unnecessary.
- Benefit: Removes `2,067,699` B (~`2.06 MB`, excluding kept raw `.irx` sources) of repo clutter and removes broken `lng/` translation packs.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-022
- Title: DTS S/PDIF `memset` heap overflow, AC3 S/PDIF frame truncation/overread, `0xFFFF` bit limit, and packet-boundary delay
- Category: 6 (Bugs and security), 5 (Performance)
- Location(s): `Simple-Media-System/src/SMS_DTS.c:11592,11693,11767-11789`, `Simple-Media-System/src/SMS_AC3.c:541,2611,2638,2751-2761`
- Problem: `_dts_spdif` (`11693`) calls `memset(lpOutBuf + 8 + aLen, 0, retVal - 8 - aLen)` unchecked (when `lnSamples == 512`, `retVal == 2048` while `aLen` up to `16384` wraps `size_t` to a massive heap overflow). `_ac3_spdif` (`2638`) divides `aLen >> 5` (`srl $t6, %2, 5`), dropping `1..31` tail bytes on non-32-byte-aligned frames while reading/writing `32` bytes when `aLen < 32`. Additionally, `_ac3_bitstream_set_ptr:541` hardcodes `0xFFFF` bits regardless of `m_FrameSize`, both decoders exit `while (lBufSize > 0)` before decoding a completed frame when `lBufSize` hits `0`, and block decoder errors (`AC3:2611`, `DTS:11592`) are ignored.
- Fix: Verify `aLen + 8 <= retVal` (and capacity) before `_dts_spdif` `memset`; copy all `aLen` bytes safely in `_ac3_spdif`; pass `m_FrameSize * 8` to `SMS_InitGetBits`; decode immediately once `m_FrameSize` bytes are buffered even at `lBufSize == 0`; check block decoder errors.
- Justification: Prevents massive S/PDIF heap overflow, S/PDIF packet truncation/overread, frame latency, and ignored decode failures.
- Benefit: Fixes 5 S/PDIF and frame-boundary correctness bugs in AC3/DTS (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-023
- Title: `SMS_ContainerAVI.c` EOF player hang (`return 0`), wrong `_UpdateStmTimings` sentinel, unbounded ODML recursion, and integer underflows
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_ContainerAVI.c:169-174,274-300,364-368,426,445,494,506,535,544,585,635-637,653-657,804-806,887,915-921,1034-1039`, `Simple-Media-System/src/SMS_Player.c:1405-1415`
- Problem: `_ReadPacket` (`806`) returns `0` on `AllocPacket` failure or short read, while `SMS_Player.c:1405-1415` only enters EOF drain when `ReadPacket < 0`, causing an infinite loop on truncated AVIs. `_UpdateStmTimings` sets `lEndTime = SMS_MININT64` (`887`) but checks `!= SMS_MAXINT64` (`915`) and divides by `m_Duration` (`921`) without `> 0`; `_LoadODMLIdx` recurses unboundedly (`298-300`) and dereferences `SMS_Realloc` unchecked (`169-174,281-285`); `_LocateFrame` wraps `anEnt - 1` when `0`; `_CheckOffsetBug` ignores short `Read`; and `_ReadHeader` subtracts constants from unsigned `lSize` and computes `(lSize += lSize & 1) + 1` wrapping `0xFFFFFFFF`.
- Fix: Return `-1` from `_ReadPacket` on EOF/error, compare `lEndTime != SMS_MININT64` and guard `m_Duration > 0`, bound ODML recursion depth, check `anEnt > 0` in `_LocateFrame`, check `Read`/`malloc`/`SMS_Realloc` returns, and verify minimum chunk `lSize` before subtraction.
- Justification: Bounds file-supplied counts and return codes that otherwise hang playback or corrupt memory.
- Benefit: Prevents AVI EOF hangs, stack overflow, div-by-zero, and OOB/NULL crashes (`INFERRED`).
- Effort: M. Risk: Low. Confidence: High (READ)

### SMS-024
- Title: `SMS_ContainerASF.c` packet-loop div-by-zero, `~4 GB` skip wrap, ignored `asf_get_packet` error, and `realloc` leak
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_ContainerASF.c:368,407,481-492,536-537,557-560,681,683`
- Problem: Outside `SMS-025`, `asf_get_packet:368` executes `% apASF->packet_size` unchecked, and line `407` lets `packet_size_left` turn negative; the demux loop passes negative `ret` to `File_Skip(..., unsigned int)` (`483`, `~4 GB` wrap) and ignores `asf_get_packet(...) == -1` (`489`); line `537` overwrites `lpASFStm->pkt.data` directly from `realloc`, leaking on OOM while keeping `pkt.alloc > 0`; `557-560` treat any `Read > 0` as full `packet_frag_size`; and `681,683` underflow `(uint32_t)(lGSize - 40)` / `(lGSize - 24)`.
- Fix: Guard `packet_size > 0` and `packet_size_left >= 0`, check `ret >= 0` before `File_Skip`, break on `asf_get_packet < 0`, keep old `pkt.data` on `realloc` OOM, require `Read(...) == packet_frag_size`, and guard `lGSize >= 40` / `>= 24`.
- Justification: Validates untrusted container lengths/packet returns to stop div-by-zero, ~4 GB seek wraps, and OOM leaks.
- Benefit: Eliminates hangs and crashes on malformed ASF/WMA containers (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-025
- Title: Unchecked demuxer math and short-read bugs (`FLAC`, `AAC`, `AC3`, `OGG`, `M3U`, `ASF` header bitrates)
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_ContainerFLAC.c:77`, `Simple-Media-System/src/SMS_ContainerAAC.c:151,157,160`, `Simple-Media-System/src/SMS_ContainerAC3.c:72`, `Simple-Media-System/src/SMS_ContainerOGG.c:21,31-32,61-63`, `Simple-Media-System/src/SMS_ContainerM3U.c:313-315`, `Simple-Media-System/src/SMS_ContainerASF.c:140,149,318,322-329,346-352`
- Problem: `SMS_ContainerFLAC.c:77` and `SMS_ContainerAAC.c:151,160` divide by zero when `m_SampleRate == 0` or computed `lnRB == 0`; `SMS_ContainerAAC.c:157` and `SMS_ContainerAC3.c:72` ignore short `Read()` results; `SMS_ContainerOGG.c:21,31,61` assigns signed read results into `unsigned int lnRead`; `SMS_ContainerM3U.c:313-315` writes `_STR(lpNode)[lLen - 4]` when `lLen < 5`; and `SMS_ContainerASF.c:328,350` writes stream bitrates into `lpASF->stream_bitrates` while `asf_read_header` reads a zeroed local `lBitRate` array (and wraps unsigned skip math at `318`).
- Fix: Guard `SampleRate`/`lnRB`/`lLen` before division or negative indexing, check `Read()` lengths, type `lnRead` as signed, read bitrates from `lpASF->stream_bitrates`, and validate ASF skip sizes.
- Justification: Fixes concrete divide-by-zero, heap/string underflow, stale packet data, and dead ASF bitrate extraction.
- Benefit: Eliminates 6 container-level crashes and data corruption paths on malformed media files.
- Effort: M. Risk: Low. Confidence: High (READ)

### SMS-026
- Title: Unzeroed `$s1` in `DSP_PutNoRndQPel816MC23` causes 8-byte out-of-bounds store
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_DSP_QPel.S:1574-1578,2331-2342,3488-3513,3880-3903`
- Problem: `_put_qpel8_16_h_lowpass` (`1578`) loads nonzero data into `$s1`, and `_put_no_rnd_qpel8_16` (`2331-2341`) branches on `$s1 != 0` to write a 9th row (`sd $t0, 0($a0)` at `2340`). Unlike `DSP_PutQPel816MC23` (`3505`) and all sibling no-round `MC*` routines, `DSP_PutNoRndQPel816MC23` (`3880-3903`) calls `_put_no_rnd_qpel8_16` (`3897`) without zeroing `$s1`.
- Fix: Insert `addu $s1, $zero, $zero` in `DSP_PutNoRndQPel816MC23` before `bgezal $zero, _put_no_rnd_qpel8_16`.
- Justification: One-instruction clear stops deterministic 8-byte overwrites past 8-row QPel destination blocks.
- Benefit: Fixes an 8-byte OOB write in QPel motion compensation (`INFERRED` frame/heap corruption).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-027
- Title: `VoiceTrans` read-mode `core == 4` OOB writes, `VoiceTrans_Write_IOMode` zero shift, and 3 unbounded hardware spin loops (`tools/libsd`)
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/tools/libsd/voice.c:58,84,115`, `Simple-Media-System/tools/libsd/freesd.c:150-154,158,164-173,298,389`
- Problem: For `SD_TRANS_READ`, `VoiceTrans` stores `mode = (chan << 2) | 0x200` (`voice.c:58`), and `TransInterrupt` (`freesd.c:151-154`) sets `core = intr->mode & 0xFF` (`4` when `chan == 1`) while entering Voice Transfer (`!(mode & 0x100)`), indexing `TransIntrHandlers[4]`, `TransIntrCallbacks[4]`, and `VoiceTransComplete[4]` past their 2-element arrays and hitting invalid MMIO (`0xBF900344 + 4096`). `VoiceTrans_Write_IOMode` (`voice.c:84`) casts `(u16)spu_addr >> 17` before shifting (always `0`), and three register spin loops (`freesd.c:298,389`, `voice.c:115`) lack `SMS_SPIN_LIMIT`.
- Fix: Keep `core = chan & 1` (or decode `(chan << 2)` properly) without shifting `chan` into `core`, use `(u16)(spu_addr >> 17)`, and bound the 3 remaining spin loops with `SMS_SPIN_LIMIT`.
- Justification: Prevents IOP OOB array/MMIO writes, upper-SPU transfer failure, and IOP hangs in fallback `libsd_irx`.
- Benefit: Eliminates `core == 4` interrupt corruption, fixes >128 KiB SPU addressing, and bounds 3 IOP spin loops.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-028
- Title: `SMS_MP123.c` side-info OOB index, error `memset` `+68` header corruption, free-format lockup, and unreset `MP123_Init` state
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_MP123.c:4767-4784,5339,5375-5378,5459-5463,5676,6408-6423,6590,6636-6668,6702`
- Problem: `_mp3_get_side_info` (`5459-5463`) indexes `s_BandInfo[aSFreq].m_LongIdx[lR0C + lR1C]` up to `24` past size `23` (`27`); `_mp3_decode_frame` (`error:6420`) clears `((unsigned char*)apRB->m_pPtr) + 68` instead of `+ 80`, overwriting 12 header bytes while leaving the tail unzeroed; false free-format sync at offsets `1..4` computes `m_FreeFmtFrameSize <= 0` (`-1` lockup / negative bit count at `6702`); and `MP123_Init` omits clearing `m_OldFrameSize`, `m_FreeFmtNextHdr`, `m_Info.m_FreeFmtFrameSize`, and `m_InBuf`.
- Fix: Reject `lR0C + lR1C >= 23`, change `+ 68` to `+ 80` at `6420`, require `m_FreeFmtFrameSize > 4 + padding`, and zero all listed fields in `MP123_Init`.
- Justification: Stops OOB table read, error-path metadata corruption, free-format hang, and cross-track reservoir leakage.
- Benefit: Eliminates 4 MP1/2/3 decoder safety/correctness bugs (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-029
- Title: AC3 `s_BAPTab[-4]` OOB read, `DELTA_BIT_RESERVED` uninitialized `lpDeltBA`, and coupling-band underflow
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_AC3.c:264,1015,1033-1036,1067,1095,1195,2047-2049`, `Simple-Media-System/include/SMS_AC3.h:156-162`
- Problem: When `m_BAI & 7 == 7` (`lFloor = 160`) and `lpExp[i] == 0` with `lMask == -160`, `(s_BAPTab + 156)[lMask + 4 * lpExp[i]]` reads `s_BAPTab[-4]` (`1033-1036,1067,1095,1195`). Additionally, `m_DeltBAE == DELTA_BIT_RESERVED (3)` leaves `apBA->m_DeltBA` uninitialized, and `lCplEndF + 3 - lCplBegF == 0` (`2047-2049`) underflows `lnCplSubnd - 1` to `-1`.
- Fix: Clamp `156 + lMask + 4 * lpExp[i]` into `[0, 304]`, reject `DELTA_BIT_RESERVED (3)`, and check `lCplEndF + 3 > lCplBegF`.
- Justification: Prevents OOB array reads, uninitialized stack/struct reads, and negative coupling loop bounds on corrupt AC3 frames.
- Benefit: Fixes 3 AC3 bitstream/BAP memory-safety bugs (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-030
- Title: `libmpeg` `23.0F * 1000 / 1001` NTSC frame-rate typo, div-by-zero, `.sbss` OOB load, and stale KSEG1 `FlushCache(2)` order
- Category: 6 (Bugs and security), 1 (Dead code)
- Location(s): `Simple-Media-System/src/libmpeg.c:43-48,277-280`, `Simple-Media-System/src/libmpeg_core.S:83,322,2300-2323`
- Problem: `s_FrameRate[1]` (`libmpeg.c:44`) computes `(23.0F * 1000.0F) / 1001.0F` (`22.977` fps) instead of `24000 / 1001` (`23.976` fps), and invalid `m_FRCode` (`0,9..15`) divides `1000.0F / 0.0F` (`278`); `_MPEG_CSCImage:322` reads `lw $v0, s_CSCParam + 16` (`4` bytes past `.space 12` at end of `.sbss`); and `_MPEG_Set16` (`2300-2323`) invokes `FlushCache(2)` *before* patching code via KSEG1 (`0x20000000`), leaving cached instruction lines stale.
- Fix: Fix `s_FrameRate[1]` to `(24.0F * 1000.0F) / 1001.0F`, reject `s_FrameRate[code] <= 0.0F`, delete the dead `lw $v0, s_CSCParam + 16`, and move `FlushCache(2)` *after* patching `s_16_0..s_16_3` (or replace self-modifying code with a shift/branch).
- Justification: Restores standard NTSC film timing, removes an OOB `.sbss` read, and fixes EE I-cache coherency.
- Benefit: Fixes 23.976 fps MPEG-1/2 drift (~4.3% `INFERRED`), removes 1 OOB `.sbss` read, and eliminates an I-cache coherency hazard (`INFERRED`).
- Effort: S. Risk: Med. Confidence: High (READ)

### SMS-031
- Title: Unbounded XZ footer/index pointer math in `lzma2.c` and leak/ignored error in `GSFont_Load`
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/lzma2.c:51-122`, `Simple-Media-System/src/SMS_GS_2.c:553-581`
- Problem: `lzma2_get_uncompressed_size` computes `footer = buf + size - 12` and `index = footer - ((backward_size + 1) * 4)` without checking `size >= 12` or `index >= buf`, then `lzma2_decode_size` walks variable-length integers for `records` iterations with no input-end bound. `GSFont_Load` (`SMS_GS_2.c:569-579`) leaks any prior `g_MBFont`, ignores `lzma2_uncompress` failure, and returns an uninitialized buffer on error.
- Fix: Check `size >= 12`, `index >= buf`, and input-end bounds in `lzma2.c`; free prior/failed `g_MBFont` buffers and verify `lzma2_uncompress == XZ_OK` in `GSFont_Load`.
- Justification: Prevents OOB pointer arithmetic on corrupt `.mbf`/`.xz` headers and stops leaking/returning corrupt font buffers.
- Benefit: Eliminates XZ header OOB reads and a font heap leak/uninitialized return.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-032
- Title: Broken DivX packed-B VOP scan (`lpBuf` instead of `lpBuf + i`) and unchecked `realloc` in `SMS_MPEG4.c`
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_MPEG4.c:3360-3397`
- Problem: In DivX packed-B VOP splitting (`3369-3378`), the scan loop tests `SMS_unaligned32(lpBuf) == 0xB6010000` on every iteration without adding `i`, missing subsequent VOP start codes or matching at `i = lCurPos` immediately; line `3391` then calls `realloc` and passes it to `memcpy` unchecked.
- Fix: Change `SMS_unaligned32(lpBuf)` (`3371`) to `SMS_unaligned32(lpBuf + i)` and guard `realloc` against `NULL` before `memcpy`.
- Justification: Fixes DivX packed-B VOP boundary detection and avoids NULL dereferences under OOM.
- Benefit: Restores packed-B MPEG-4 VOP splitting and guards `realloc`.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-033
- Title: Broken `DSP_MDCTInit` OOM check (`!DSP_iFFTInit(...) < 0`), unchecked `memalign`, and `DSP_GMCn_8` chroma index typo (`SMS_DSP.c`)
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_DSP.c:289-290,945,966-975`
- Problem: `DSP_MDCTInit` (`945`) dereferences `memalign(...)` unchecked (`966-969`), and line `972` checks `if ( !DSP_iFFTInit(...) < 0 )`, which is always false because `!ret` is `0` or `1`, returning `1` with `m_pExpTab == NULL`. In `DSP_GMCn_8` (`290`), `lSrcCrY` indexes `[lY]` instead of `[lX]` unlike `lSrcCbY` on line `289`.
- Fix: Check `s->m_pCos == NULL`, rewrite `972` to `if ( !DSP_iFFTInit ( &s -> m_FFT, s -> m_nBits - 2 ) )`, and fix `DSP_GMCn_8:290` to `[lX]`.
- Justification: Prevents OOM NULL dereferences in audio MDCT init and fixes Cr corruption in MPEG-4 GMC macroblocks.
- Benefit: Eliminates 2 MDCT OOM NULL-deref paths and fixes GMC Cr pixel corruption.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-034
- Title: Empty mono DTS stub (`_dts_float_to_int_1`) and unchecked `s_FloatToInt[anChannels - 1]` dispatch in DTS and AC3
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_DTS.c:11511-11513,11565,11753-11759`, `Simple-Media-System/src/SMS_AC3.c:2585,2725-2731`, `Simple-Media-System/src/SMS_Player.c:105`
- Problem: `_dts_float_to_int_1` (`SMS_DTS.c:11511-11513`) contains only `jr $ra; nop`, leaving mono DTS PCM output uninitialized (`silence`/garbage). Both `SMS_DTS.c:11565` and `SMS_AC3.c:2585` declare 2-entry `s_FloatToInt[2]`, while `DTS_Decode`/`AC3_Decode` accept up to `6` channels from the parser without `[1, 2]` validation inside the codec (relying on `SMS_Player.c:105`).
- Fix: Implement mono conversion in `_dts_float_to_int_1` and clamp/validate `anChannels` to `[1, 2]` in `AC3_Decode` and `DTS_Decode`.
- Justification: Fixes silent/corrupt mono DTS playback and prevents OOB function-pointer calls when multichannel configs reach the codecs directly.
- Benefit: Restores mono DTS audio output and guards OOB function-pointer dispatch (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-035
- Title: Palette decrement wraparound reads `g_Palette[-1]` and empty skin/font list handlers dereference `NULL`
- Category: 6 (Bugs and security), 2 (Duplication)
- Location(s): `Simple-Media-System/src/SMS_GUIMenuSMS.c:2104-2128,2190-2258,2348-2396,2562-2585`
- Problem: Four palette helpers wrap downward via `else if ( lVal < 0 ) lVal = 16;`, letting `lVal` become `0` so `_txtclr_handler:2231` and `_sltxt_handler:2253` read `g_Palette[-1]` out of bounds; `_usebg_handler` and `_smbcs_handler` also dereference `lpNode->m_pNext`/`m_pPrev`/`_STR(lpNode)` without checking `lpNode == NULL` on empty skin/font lists.
- Fix: Change the lower bound to `lVal < 1`, return early when `lpNode == NULL`, and merge the 4 palette cyclers and 2 list cyclers into two shared helpers.
- Justification: Fixes concrete OOB read and `NULL` crash while collapsing duplicated UI cyclers.
- Benefit: Fixes 2 UI crash/OOB bugs and removes ~`90` lines of duplicated handler code.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-036
- Title: Unchecked status buffer overflow and handler boilerplate in `SMS_GUIMenuSMS.c`
- Category: 6 (Bugs and security), 2 (Duplication), 3 (Overengineering)
- Location(s): `Simple-Media-System/src/SMS_GUIMenuSMS.c:456-482,1865-1935`
- Problem: `GUIMenuSMS_UpdateStatus` (`459-478`) appends breadcrumb titles into `char lBuf[1024]` using `strncpy`/`lIdx += m_Len` without checking remaining buffer capacity, and `SMS_GUIMenuSMS.c:1865-1935` defines 12 nearly identical single-line octet handlers (`_ip1..4_handler`, `_nm1..4_handler`, `_gw1..4_handler`).
- Fix: Track remaining capacity (`snprintf`/bounds check) in `GUIMenuSMS_UpdateStatus`, and replace the 12 single-line octet wrappers with a shared parameter helper or table encoding pointer+index.
- Justification: Stops stack overflow on long status strings while shrinking menu boilerplate.
- Benefit: Fixes a stack buffer overflow hazard and removes ~`60+` lines of octet handler wrappers (in addition to `SMS-035` cyclers).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-037
- Title: `16` unpatched legacy `!BDM` bugs across `iop/SMSUMS/` and `iop/USBD/`
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/iop/SMSUMS/src/ums_fat.c:139,403,420-441,526,566,652,697-706,1002-1046`, `Simple-Media-System/iop/SMSUMS/src/ums_driver.c:102-112,125,169-172,184,300-315,398-401`, `Simple-Media-System/iop/SMSUMS/src/ums.c:100,120,161-170,251,264-462`, `Simple-Media-System/iop/USBD/src/usbd.c:104-106`, `Simple-Media-System/iop/USBD/src/hcd.c:306-308,340-341,475-479`, `Simple-Media-System/iop/USBD/src/driver.c:105-106,150-168`, `Simple-Media-System/iop/USBD/include/usbd_macro.h:136-138`
- Problem: The unbuilt/fallback `!BDM` `SMSUMS` and `USBD` driver sources harbor `16` legacy IOP defects across FAT handling, USB mass driver transfers/callbacks, and HCD/driver initialization (`SMS-018` notes default `BDM=1` uses `BDM` drivers instead).
- Fix: Retire legacy `!BDM` `SMSUMS`/`USBD` drivers outright per `SMS-018` (zero added code); only if `!BDM` remains supported, audit and fix those `16` sites or switch to the active BDM USB stack.
- Benefit: Eliminates `16` legacy `!BDM` USB/FAT defect sites (by retiring `!BDM` or hardening before re-enabling).
- Effort: M. Risk: Low. Confidence: High (READ inventory of defect sites; INFERRED individual runtime triggers unless `!BDM` built)

### SMS-038
- Title: Unchecked `(int)` pointer casts and bit-31 pointer tagging in container and JPEG paths
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_ContainerJPG.c:29,43-50`, `Simple-Media-System/src/SMS_ContainerM3U.c:217,299-301,496`, `Simple-Media-System/src/SMS_JPEG.c:1837`, `Simple-Media-System/src/SMS_ContainerMPEG_PS.c:692`, `Simple-Media-System/src/SMS_ContainerAAC.c:187`, `Simple-Media-System/src/SMS_ContainerAC3.c:99`, `Simple-Media-System/src/SMS_ContainerASF.c:699`, `Simple-Media-System/src/SMS_ContainerAVI.c:1160`, `Simple-Media-System/src/SMS_ContainerFLAC.c:112`, `Simple-Media-System/src/SMS_ContainerMOV.c:1527,1542`, `Simple-Media-System/src/SMS_ContainerMP3.c:107`, `Simple-Media-System/src/SMS_ContainerOGG.c:94`
- Problem: Ten container entry points check pointers via `(int)ptr <= 0` (`clang` warns `-Wpointer-to-int-cast`), while `SMS_ContainerJPG.c` and `SMS_ContainerM3U.c` overload bit `31` (`0x80000000`) to tag prebuilt `SMS_List*` pointers (`SMS_ContainerJPG.c:48` overwrites `lpList` with bit 31 set, causing `_Destroy:29` to skip freeing it unless cleared later; `SMS_ContainerM3U.c:217` stores a pointer in `m_DTS` via `unsigned int`).
- Fix: Replace `(int)ptr <= 0` with `!ptr`, pass prebuilt playlists via a dedicated struct field instead of setting bit `31` on pointers, and remove `(unsigned int)` pointer bit-twiddling.
- Justification: Prevents 64-bit host test truncation and tagged-pointer memory leaks/aliasing cleanly.
- Benefit: Eliminates `15` unsafe pointer-to-int casts and fixes the unfreed tagged `SMS_List` edge case in `SMS_ContainerJPG.c`.
- Effort: M. Risk: Med. Confidence: High (READ)

### SMS-039
- Title: `SMS_IPU.c` broken `mtc0 $a1, $11` write, `s_VIFQueue`/`s_DMAVIFDraw` overflow (`64 B` `.bss`), and 4 duplicate/stub handlers
- Category: 6 (Bugs and security), 2 (Duplication), 4 (Size and memory)
- Location(s): `Simple-Media-System/src/SMS_IPU.c:38,46,186-202,568-580,584-644,841-857`
- Problem: Audio-only `IPU_InitContext` (`850-857`) computes `0xFFFFFFFF` into `$t0` (missing `"t0"` clobber) and then runs `mtc0 $a1, $11`, writing `IPU_DummyVBlankStartHandler` (`$a1` from `SMS_SetAlarm`) into CP0 `Compare` (`$11`). Separately, `IPU_DummySuspend`/`IPU_DummyResume` duplicate `IPU_Suspend`/`IPU_Resume`, `IPU_DummyChangeMode`/`IPU_DummyPan` duplicate empty `IPU_Dummy`, and `s_VIFQueue[16]` overflows `s_DMAVIFDraw` (`8` 128-bit qwords) whenever >8 items queue before `IPU_Flush`.
- Fix: Fix or drop the `mtc0` block (`mtc0 $t0, $11` + `"t0"` clobber if disabling compare), delete `IPU_DummySuspend`, `IPU_DummyResume`, `IPU_DummyChangeMode`, and `IPU_DummyPan`, and shrink `s_VIFQueue` to `8` entries (or guard an 8-entry cap).
- Justification: Guarding queue capacity (or matching array size) prevents out-of-bounds writes into adjacent `.bss` while deleting `4` redundant stub functions.
- Benefit: Fixes unintended CP0 Compare write, deletes `4` duplicate/stub functions (~`30` lines) + `64` B `.bss`, and removes the `s_DMAVIFDraw` overrun (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-040
- Title: `SMS_ContainerMOV.c` corrupt `_mov_read_stsd` skip, wrong `_mov_read_dflt` atom, invalid inline asm, `_mov_read_esds`, and global table mutation
- Category: 6 (Bugs and security), 3 (Overengineering)
- Location(s): `Simple-Media-System/src/SMS_ContainerMOV.c:107-115,330-380,460-465,728-738,880-912,1529,1544`
- Problem: `_mov_read_stsd:372` skips `m_CurPos - lStartPos` (`16` consumed bytes instead of remaining bytes, corrupting atom sync, while `338,373,737` lack `lSize >= 16` and `lnEntries` bounds); `_mov_read_dflt:910` compares `lTotalSize < lAtom.m_Size` (last child) instead of `apAtom->m_Size`; `_read_int_be` (`107-115`) uses conflicting `"=r"`/`"r"` operands around `dsll32`/`dsrl32`; `_mov_read_esds` (`460-465`) ignores consumed bytes and short `Read`; and `SMS_GetContainerMOV`/`M4A` mutate global `s_MOVParseTbl[0].m_Func`.
- Fix: Delete the `5`-line inline asm block in `_read_int_be`, skip `lSize - (m_CurPos - lStartPos)` with `lSize`/`lnEntries` checks in `stsd`/`stsd_m4ap`, compare `apAtom->m_Size` at `910`, check `Read == lLen` at `463`, and select MOV vs M4A handler via context instead of mutating `s_MOVParseTbl`.
- Justification: Prevents MOV/MP4 demux desync and `~4 GB` seek wrap on malformed atoms while removing invalid inline asm.
- Benefit: Fixes non-AV `stsd` track skipping, `dflt` child-atom parsing, and invalid GCC asm constraints (`5` lines removed, `INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-041
- Title: `SMS_ContainerAAC.c` wrong ADTS probe seek offset and 8-byte uninitialized `m_pUserData` read
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_ContainerAAC.c:90-122,212-215`, `Simple-Media-System/src/SMS_AAC.c:8262-8264`
- Problem: After scanning 5 ADTS frames in `SMS_AACProbe` (`90-122`), line `108` seeks to `m_CurPos - lSize - 16`, landing near frame 4 and returning frame 5's header; lines `212-215` call unchecked `malloc(16)`, populate only `8` bytes (`lHdr`), and pass `abs(-16) == 16` bytes (`SMS_AAC.c:8264`) into `_NeAACDecInit` with `8` uninitialized heap bytes.
- Fix: Record and seek to the start offset of the first matched ADTS frame, check `m_pUserData != NULL`, and allocate/pass `8` bytes (`-8`) or `calloc(1, 16)`.
- Justification: Preserves initial AAC frames and eliminates uninitialized heap reads into the AAC decoder.
- Benefit: Stops skipping initial ADTS frames on playback start and fixes an `8`-byte uninitialized heap read plus OOM NULL deref (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-042
- Title: `SMS_JPEG.c` unbounded `++apFileName` loop in `_display_text`, unchecked `memalign`, and 64-byte over-rounding
- Category: 6 (Bugs and security), 4 (Size and memory)
- Location(s): `Simple-Media-System/src/SMS_JPEG.c:588-589,1079-1084,1102-1126,1583-1591`
- Problem: `_display_text` (`1583-1591`) loops `while(1) { ++apFileName; ... }` without checking `!*apFileName` when prefix + quotes + `"..."` exceeds `lW`, scanning past string bounds indefinitely. `SMS_JPEGViewerInit` (`1079-1084,1106`) omits `NULL` checks on `memalign`, and lines `588-589` round `(lnAlloc + 64) & ~63`, wasting `64` B on exact 64B multiples.
- Fix: Stop the truncation loop when `!*apFileName`, guard `retVal` and `m_pStsPgFg` against `NULL`, and change `+ 64` to `+ 63` at `588-589`.
- Justification: Bounds loop termination on narrow displays and prevents OOM `NULL` writes.
- Benefit: Eliminates infinite OOB string read/hang, OOM crashes, and `64` B/buffer over-allocation (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-043
- Title: `SMS_ContainerMPEG_PS.c` seek div-by-zero & dead `m_ID == lID` scan, `SMS_Rescale.c` row-end over-read, and `SMS_VLC.c` unchecked `malloc`s
- Category: 6 (Bugs and security), 1 (Dead code)
- Location(s): `Simple-Media-System/src/SMS_ContainerMPEG_PS.c:634-636,663,714-716,783,803`, `Simple-Media-System/src/SMS_Rescale.c:238-260`, `Simple-Media-System/src/SMS_VLC.c:223-227,268`
- Problem: `SMS_ContainerMPEG_PS.c:635` divides by `(float)(m_Duration - m_StartTime)` without guarding `<= 0` (and `663` ignores EOF), while line `783` compares `(*lIt)->m_ID` (PES start codes `0x80..0x1EF`) to `SMS_CodecID` (`1..16`), making the scan dead (`714-716` already skips duplicates). `_copy_buffer` (`SMS_Rescale.c:246-252`) uses `lw` for 3-byte RGB24 strides, reading `1` byte past row end; `SMS_RL_Init`/`SMS_VLC_RL_Init` (`SMS_VLC.c:223-227,268`) dereference unchecked `malloc` pointers.
- Fix: Guard `m_Duration > m_StartTime` and `File_GetUInt` EOF, delete lines `783`'s dead scan, pad or safely read trailing RGB24 pixels in `SMS_Rescale.c`, and check `malloc` pointers in `SMS_VLC.c`.
- Justification: Prevents MPEG-PS seek div-by-zero, removes dead stream scans (`3` lines), and closes row-end over-reads and VLC OOM crashes.
- Benefit: Eliminates seek div-by-zero, OOB row read, VLC OOM derefs, and dead scan code (`INFERRED`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-044
- Title: Divide-by-zero on `g_MPEGCtx.m_SliceHeight` in `SMS_MSMPEG4.c`
- Category: 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_MSMPEG4.c:2147-2151,2332`
- Problem: Line `2151` sets `m_SliceHeight = m_MBH / ( lCode - 0x16 )` (divisor up to `9`), yielding `0` on `< 144`-px frames (`m_MBH < 9`) or if slice decoding reaches `2332` before an I-frame sets `m_SliceHeight`, trapping on `m_MBY % g_MPEGCtx.m_SliceHeight`.
- Fix: Clamp `m_SliceHeight >= 1` at `2151` and guard `m_SliceHeight == 0` at `2332`.
- Justification: One-line clamp/guard avoids deterministic divide-by-zero traps on small or corrupt MSMPEG4 streams.
- Benefit: Eliminates integer divide-by-zero crash path.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-045
- Title: `g_ASCII[4572]` vs `[5416]` extern size mismatch in `SMS_GS_2.c` forces needless `5.4 KB` heap allocation
- Category: 6 (Bugs and security), 4 (Size and memory)
- Location(s): `Simple-Media-System/src/SMS_GSFont.c:11`, `Simple-Media-System/src/SMS_GS_2.c:35,50-53,502-508`, `Simple-Media-System/src/SMS_Config.c:292-304`
- Problem: `SMS_GSFont.c:11` defines `unsigned char g_ASCII[ 5416 ]`, while `SMS_GS_2.c:35` declares `extern unsigned char g_ASCII[ 4572 ]`, causing `s_FontSizes[0]` (`51`) to under-report `4572` so `SMS_Config.c:296-297` heap-allocates `malloc(lSize)` on `4573..5416`-byte ASCII `.mtf` files instead of copying in place.
- Fix: Change `src/SMS_GS_2.c:35` to `extern unsigned char g_ASCII[ 5416 ]`.
- Benefit: Avoids `~5.4 KB` unnecessary heap allocation on custom ASCII `.mtf` loads and fixes the array size mismatch.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-046
- Title: Move `150+ KB` of immutable MP123/DTS/OGG/WMA tables from writable `.data` to `.rodata` and delete `1,057 B` of duplicate MP123 rows
- Category: 4 (Size and memory), 2 (Duplication)
- Location(s): `Simple-Media-System/src/SMS_MP123.c:62-1134,3189-4740,5920`, `Simple-Media-System/src/SMS_DTS.c` (85 `static const` tables), `Simple-Media-System/src/SMS_OGG.c:385-2452`, `Simple-Media-System/src/SMS_WMA.c:56-1122`
- Problem: `SMS_MP123.c` omits `const` on `41` read-only lookup arrays (`28,184` B) and stores `1,057` B of zero/duplicate rows (`s_PreTab2[22]` = `88` B zeros used only at `5920`; duplicate rows in `s_MapBuf0`, `s_MapBuf2`, `s_LongLimit`, `s_ShortLimit`, and `s_BandInfo`), while `SMS_DTS.c` (`85` tables, `89,193` B), `SMS_OGG.c` (`vwin*`, `32,672` B), and `SMS_WMA.c` (`28` tables) explicitly force `const` tables into `section(".data")` instead of `section(".rodata")` (unlike `SMS_AAC.c`/`SMS_FLAC.c`).
- Fix: Delete `s_PreTab2` (inline `0` at `5920`) and share duplicate MP123 row pointers/indices, add `const` on the 41 `SMS_MP123.c` tables, and move large `section(".data")` constant tables in `SMS_DTS.c`, `SMS_OGG.c`, and `SMS_WMA.c` to `section(".rodata")` (watching `-G8192` GP window bounds noted in `SMS-052`).
- Benefit: Saves `1,057` B exact duplicate MP3 data and moves `150,049` B+ (`28,184` MP123 + `89,193` DTS + `32,672` OGG + `28` WMA tables) out of writable `.data` to `.rodata`.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-047
- Title: Deduplicate AC3/DTS S/PDIF loop and SBR invocation; delete `455 B` dead DTS tables, `SMS_swab`, and dead AAC code
- Category: 1 (Dead code), 2 (Duplication)
- Location(s): `Simple-Media-System/src/SMS_DTS.c:127-143,150-152,1162-1171,1184-1193,10105,10112,11601-11637,11666-11691`, `Simple-Media-System/src/SMS_AC3.c:2636-2661`, `Simple-Media-System/src/SMS_AAC.c:514,1963-1969,3809,5030,5778,6304,6552-6557,6589-6594,6720-6726,7369,7414-7431,7724-7743,8251`
- Problem: `_ac3_spdif` and `_dts_spdif` copy-paste the EE 128-bit S/PDIF byte-swap loop while `SMS_swab` (`37` lines) sits unused in `SMS_DTS.c`; `SMS_DTS.c` keeps `455` B of zero-caller tables (`dts_sample_rates`, `dts_bit_rates`, `dts_bits_per_sample`, `lossy_quant`, `lossless_quant`); and `SMS_AAC.c` keeps dead `huffman_codebook`/`case 12:`, dead `output_channels == 2` branch, unused `mes[]`, stray `16428320` literals, duplicated SBR call blocks, and `_aac_decode:8251` returning `0` on `error:`.
- Fix: Delete the 5 dead DTS tables (`455` B), delete dead AAC codebook/branch/`mes[]`/`16428320` code, unify AC3/DTS byte-swap into one fixed `SMS_swab` helper (incorporating `SMS-011`/`SMS-022`), share one SBR invocation helper, and return an error cleanly from `_aac_decode`.
- Justification: Unifying duplicated S/PDIF and SBR logic across existing call sites removes net code while eliminating divergent bug fixes.
- Benefit: Removes `455` B of dead tables, `37`-line dead `SMS_swab`, and duplicated S/PDIF + SBR blocks.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-048
- Title: `13` unused exported DS34 controller RPC client wrappers in `libds34bt.c` and `libds34usb.c`
- Category: 1 (Dead code), 4 (Size and memory)
- Location(s): `Simple-Media-System/src/libds34bt.c:45-182`, `Simple-Media-System/src/libds34usb.c:60-154`
- Problem: Thirteen DS34 RPC wrappers (`ds34usb_deinit`, `_reinit_ports`, `_set_rumble`, `_set_led`, `_reset`, and `ds34bt_deinit`, `_reinit_ports`, `_init_charging`, `_set_rumble`, `_set_led`, `_reset`, `_get_version`, `_get_features`) have zero callers in `src/` or `include/`. (`CROSS-004` tracks cross-repo driver dedup and the `SifCallRpc` bug.)
- Fix: Delete the `13` unused wrappers from `src/libds34bt.c`, `src/libds34usb.c`, `include/libds34bt.h`, and `include/libds34usb.h`.
- Benefit: Removes `13` unused functions (~`1.3 KB` `.text` `INFERRED`, ~`80` lines).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-049
- Title: Dead assembly round-rect code, unused PAD functions, `PAD_Quit` collision, `#if 0` stub, and unused `encode()`
- Category: 1 (Dead code), 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_GS_0.S:37-42,60-61,827-1075`, `Simple-Media-System/include/SMS_GS.h:1274`, `Simple-Media-System/src/SMS_PAD.c:33,48-56,123-142,348-372`, `Simple-Media-System/include/SMS_PAD.h:17-34,142`, `Simple-Media-System/src/SMS_GUI.c:90-91`, `Simple-Media-System/src/SMS_MPEG.c:904-935`, `Simple-Media-System/src/lzma2.c:10-26`
- Problem: `GS_RenderRoundRect_asm`, `GS_RRV` (`_gs_rrv`), and `s_SinCos` remain uncalled after migrating `GS_RenderRoundRect` to C (`SMS_GS_1.c:626`). `ReadPadStatus`/`ReadCombinedPadStatus`/`old_pad[2]` only reference each other; `#define PAD_Quit() PadDeinitPads()` (`SMS_PAD.h:30`) collides with `int PAD_Quit(void)` (`142`); `s_PadBuf0` (`SMS_GUI.c:90`) is dead under `BDM=1`; `SMS_H263_DCTUnquantizeInter` (`SMS_MPEG.c:904-935`) is an empty `#if 0` stub; and `lzma2.c:10-26` (`encode`) is uncalled.
- Fix: Delete `GS_RenderRoundRect_asm`, `GS_RRV`, `s_SinCos`, `ReadPadStatus*`, `old_pad`, `PadDeinitPads`, the `#define PAD_Quit()` macro, `s_PadBuf0` under `BDM`, `encode()`, and the empty `SMS_H263_DCTUnquantizeInter` stub.
- Benefit: Removes ~`1 KB` `.text`, ~`536` B `.data`, ~`330` dead lines, and resolves the `PAD_Quit` conflict.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-050
- Title: Unused exports (`GS_XYZF`, `RC_Shutdown`, `RCX_Shutdown`), dead `.else`, broken/orphaned headers, and unchecked `SMS_RC.c` `malloc`
- Category: 1 (Dead code), 6 (Bugs and security)
- Location(s): `Simple-Media-System/src/SMS_GS_0.S:52,168-229,448-456`, `Simple-Media-System/include/SMS_GS.h:1268`, `Simple-Media-System/src/SMS_RC.c:111-113,454-497`, `Simple-Media-System/include/SMS_RC.h:83-84`, `Simple-Media-System/include/SMS_H263.h:29-31`, `Simple-Media-System/include/SMS_JPEG.h:109`, `Simple-Media-System/include/SMS_PAD.h:32-33,144-145`, `Simple-Media-System/include/SMS.h:302,305,308-309`, `Simple-Media-System/include/SMS_DSP.h:77`, `Simple-Media-System/include/SMS_MPEG.h:286`, `Simple-Media-System/include/SMS_PlayerMenu.h:21`, `Simple-Media-System/include/xz.h:291`
- Problem: `GS_XYZF`, `RC_Shutdown`, and `RCX_Shutdown` have zero callers (`SMS_GS_0.S:168-229` also retains `62` unassembled `.else` lines; `SMS_RC.c:111-113` passes unchecked `malloc(lSize)` to `fioRead`). Header issues include an unclosed `extern "C" {` at `SMS_H263.h:29-31`, typo `SMS_ContainedrJPEG` (`SMS_JPEG.h:109`), `//` comment macros for `PAD_OpenPort`/`PAD_ClosePort` (`SMS_PAD.h:32-33`), and orphaned declarations (`SMS_Initialize`, `SMS_Align`, `SMS_StartNetwork`, `SMS_ResetIOP`, `SMS_DSP_Init`, `SMS_MPEG_SCB`, `PlayerMenu_Init`, `xz_crc64_init`).
- Fix: Delete `GS_XYZF`, `RC_Shutdown`, `RCX_Shutdown`, lines `168-229` of `SMS_GS_0.S`, and the orphaned declarations; check `lpData != NULL` in `SMS_RC.c:111`; close/drop `extern "C"` in `SMS_H263.h`; fix `SMS_ContainerJPEG` in `SMS_JPEG.h`; remove or `do {} while (0)` `PAD_OpenPort`/`PAD_ClosePort`.
- Justification: One `NULL` check prevents OOM writes in `_load_rmman`; rest is pure dead-code/header cleanup.
- Benefit: Shrinks linked ELF (`GS_XYZF`, `RC_Shutdown`, `RCX_Shutdown`), deletes ~`105` broken/dead lines, and fixes C++ `SMS_H263.h` builds.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-051
- Title: `4,566 B` duplicate headers (`mprintf.h`, `stdint.h`, `dev9.h`) inside `iop/SMSUdpfs/` and `iop/SMSDev9/`
- Category: 2 (Duplication), 7 (Build/CI/repo clutter)
- Location(s): `Simple-Media-System/iop/SMSUdpfs/common/mprintf.h`, `Simple-Media-System/iop/SMSUdpfs/ministack/include/mprintf.h`, `Simple-Media-System/iop/SMSUdpfs/ministack/include/stdint.h`, `Simple-Media-System/iop/SMSUdpfs/smap/include/stdint.h`, `Simple-Media-System/iop/SMSUdpfs/udpfs/include/stdint.h`, `Simple-Media-System/iop/SMSUdpfs/smap/include/dev9.h`, `Simple-Media-System/iop/SMSDev9/include/dev9.h`
- Problem: Byte-identical headers are duplicated within `iop/`: `mprintf.h` (`322` B x2), `stdint.h` (`1,229` B x3 across `ministack`, `smap`, `udpfs`), and `dev9.h` (`1,786` B x2 between `SMSUdpfs/smap` and `SMSDev9`).
- Fix: Keep one `mprintf.h`, one `stdint.h`, and one `dev9.h` in `iop/SMSUdpfs/common/` (or `SMSDev9/include`) and adjust `-I` include flags in the sub-Makefiles.
- Benefit: Removes `4,566` B of duplicate IOP headers and keeps subprojects synced.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-052
- Title: `SMS_DIAG=1` forces `-G4096` GP-window workaround because temporary diagnostic buffers stay compiled
- Category: 3 (Overengineering), 4 (Size and memory), 7 (Build/CI/repo clutter)
- Location(s): `Simple-Media-System/Makefile:71-81`, `Simple-Media-System/src/SMS_GUIMenuSMS.c:105,1065-1086`, `Simple-Media-System/src/SMS_Config.c:69`
- Problem: `Makefile:72-80` drops small-data threshold from `-G8192` to `-G4096` because `SMS_DIAG` overflows the GP-relative window, and diagnostic scratch arrays like `g_SaveDiag[64]` (`SMS_Config.c:69`) remain compiled in release `.bss`.
- Fix: Remove closed hardware-repro diagnostic buffers/strings from release builds rather than tuning `-G` thresholds around them.
- Benefit: Cleaner EE small-data addressing and smaller release `.bss`/`.rodata` (`INFERRED` ~`1 KB`).
- Effort: M. Risk: Med. Confidence: Med (READ)

### SMS-053
- Title: Windows checkout EOL drift fails `docs-site/build.py --check`, plus host test script issues (`test_boot_config.py`, `test_converter.py`)
- Category: 7 (Build/CI/repo clutter), 6 (Bugs and security)
- Location(s): `Simple-Media-System/.gitattributes:1-16`, `Simple-Media-System/docs/assets/ui/capture-manifest.json`, `Simple-Media-System/docs-site/static/assets/ui/capture-manifest.json`, `Simple-Media-System/tools/sms_converter/test_converter.py:521`, `Simple-Media-System/tools/test_boot_config.py`, `Simple-Media-System/tools/test_mpeg_ps_probe.py`
- Problem: `.gitattributes` pins `text eol=lf` on `docs-site/**` and `docs/data/*.json` but omits `docs/assets/*.json`, converting `capture-manifest.json` to CRLF on Windows and failing `docs-site/build.py --check`. Host checks also hit an unclosed file handle (`ResourceWarning`) at `test_converter.py:521`, `-no-pie` clang warnings, `test_mpeg_ps_probe.py` exit `1` at `e65ced39`, and `test_boot_config.py` BOM/CRLF/spaces failures on Windows.
- Fix: Add `docs/assets/*.json text eol=lf` to `.gitattributes`, wrap `test_converter.py:521` in `with`, and fix the Windows `clang`/CLI flags and path handling in the failing `tools/test_*.py` scripts.
- Justification: Minimal config and test-script fixes restore deterministic cross-platform host verification.
- Benefit: Fixes failing/warning host checks (`docs-site/build.py --check`, `test_boot_config.py`, `test_converter.py`).
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-054
- Title: Duplicated color-probe and RGBA-baker scripts (`compare.py`, `bake_loading_rgba.py`), broken default `OUT` path, and stale comment
- Category: 2 (Duplication), 7 (Build/CI/repo clutter)
- Location(s): `Simple-Media-System/tools/mpeg_color_probe/compare.py:1-43`, `Simple-Media-System/tools/mpeg_color_probe/compare_series.py:1-73`, `Simple-Media-System/tools/bake_icons_rgba.py:21,50-70,75,95-97`, `Simple-Media-System/tools/bake_loading_rgba.py:10,14-28`
- Problem: `compare.py` (`43` lines) duplicates `compare_series.py`'s CLI flags, untiling, and error metrics; `bake_loading_rgba.py` (`28` lines) duplicates `bake_icons_rgba.py`'s GS alpha scaling and C-array writer; and `bake_icons_rgba.py:21` defaults `OUT` to nonexistent `/work/SMS-v1/src/SMS_IconsRGBA.c` while comments at `75,97` still say `11 dev` for the `13`-slot `DEV` array.
- Fix: Delete `compare.py` (updating `tools/mpeg_color_probe/README.md`), fold `bake_loading_rgba.py` into `bake_icons_rgba.py`, fix `OUT` default to `/work/src/SMS_IconsRGBA.c`, and update `11 dev` to `13 dev`.
- Benefit: Removes ~`71` lines of duplicate tooling scripts and fixes the broken default `OUT` path.
- Effort: S. Risk: Low. Confidence: High (READ)

### SMS-055
- Title: Duplicated drag-and-drop event handlers in `tools/sms_converter/ui/drop_zone.py`
- Category: 2 (Duplication)
- Location(s): `Simple-Media-System/tools/sms_converter/ui/drop_zone.py:15,69-90`, `Simple-Media-System/tools/sms_converter/ui/main_window.py:267-290`
- Problem: `DropZoneWidget` (`15,69-90`) duplicates `MainWindow`'s (`267-290`) `dragEnterEvent`, `dragMoveEvent`, and `dropEvent` local-file URL filtering verbatim without doing any drop-hover styling (`main_window.py:281` also uses inline `import os as _os`).
- Fix: Remove `setAcceptDrops(True)` and the 3 drag/drop event overrides from `DropZoneWidget` so drops bubble to `MainWindow`, and move `import os` to module scope in `main_window.py`.
- Benefit: Removes ~`25` lines of duplicate Qt drag-and-drop boilerplate.
- Effort: S. Risk: Low. Confidence: High (READ)

---

## Files Not Read (`Simple-Media-System`)

1. **EE application/UI/player implementation files outside `sms_core_inc` and codec/tool shards (`~17` C files):** `src/SMS_Player.c` (`90-130, 725-765, 835-875, 1405-1415` read), `src/SMS_IOP.c` (`228,437,584-693` read), `src/SMS_GUI.c` (`90-91` read), `src/SMS_GUIMenuSMS.c` (`105,425,456-482,1065-1086,1865-2585` read), `src/SMS_Config.c` (`69,292-304` read), `src/SMS_FileDir.c` (`420` read), `src/SMS_Codec.c` (`80-150` read), `src/SMS_RingBuffer.c` (`61-160` read), `src/SMS_ContainerFLAC.c` (`77,112-138` read), `src/SMS_ContainerJPG.c` (`29-64` read), `src/SMS_ContainerOGG.c` (`21-108` read), `src/SMS_ContainerAC3.c` (`72-135` read), `src/SMS_ContainerMP3.c` (`1-124` read), `src/SMS_Data.c` (IRX/data table ranges), `src/SMS_DXSB.c`, `src/SMS_PCM.c`, `src/SMS_MPEG12.c`, `src/SMS_MPEGInit.c`, `src/SMS_SubtitleContext.c`, `src/SMS_Spectrum.c`, `src/SMS_SPU.c`, `src/SMS_DirTree.c`, `src/SMS_MC.c`, `src/SMS_SMB.c`, `src/SMS_GUIcons.c` (`25-1447`), `src/SMS_PlayerBallSim.c`, `src/SMS_IPU_1.c`, `src/lzma2.c` (`1-122`), `src/libds34bt.c`, `src/libds34usb.c`, `src/SMS_PAD.c`, `src/SMS_EE.c`: remaining UI widget/table boilerplate and data blobs were skipped once core containers, codecs, IOP drivers, headers, and tools were read in full.
2. **Large generated static hex/numeric tables (`> 1,500` lines each):** `include/s_iop_image.h:51-3779`, `src/SMS_GSFont.c:12-2296` (non-header glyph bitmap tables), `src/SMS_OS.c:20-1964`, `src/SMS_DTS.c:1207-3254,3779-9922`, `src/SMS_MP123.c:1136-3187`. Headers and tails were read via `read_file` and bodies verified via `python -B` to contain exclusively hex/numeric table literals.
3. **Host tests not re-read line-by-line after batch run (`tools/test_*.py` beyond `check_devmenu.py`, `test_converter.py`, `test_end_to_end.py`, `test_file_size.py`, `test_geometry.py`, `test_multitrack.py`, `test_boot_config.py`, `test_mpeg_ps_probe.py`) & `SMSUdpfs/{smap,ministack}` low-level packet loops:** Packet TX/RX loops in `smap`/`ministack` beyond targeted checks (`xfer.c`, `udptty`, headers) and `59`-script `tools/test_*.py` bodies were evaluated via execution/diff/hash rather than line-by-line reading.
4. **Generated/archival documentation (`docs/*.html`, `wiki/*.md`, `docs-site/content/*.html`, `docs/investigations/*`):** Inventoried by directory size/file count and checked with `docs-site/build.py --check`.
