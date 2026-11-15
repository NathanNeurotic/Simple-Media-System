# Sable — SMS FULL-VERIFICATION Phase 2, Section E (SMS-093…102, 123, 156–157, 172–178, 181)

- **Repo / pin:** `Simple-Media-System` at `e65ced39a27eee8d996e9257e21594d1cac1c93d` — `git rev-parse HEAD` confirmed equal to the pin before any read. Read-only throughout; all scripts run from `/tmp`.
- **Stance:** adversarial — each finding was re-derived from the code at the pin and survives only where the code proves it.
- **Counts (as written in the table below):** VERIFIED 17 · VERIFIED-DRIFT 2 · WEAKENED 1 · REFUTED 1 · total 21.

---

## 1. Verdict table

| ID | Verdict | One-line evidence (path:line anchors) |
|---|---|---|
| SMS-093 | VERIFIED | Uncapped load loops `src/SMS_History.c:41-52,78-89,97-108` (allocs `:48,:85,:103`); cap only on add at `:20,:141`; `SMS_ListPushBackBuf` has no NULL check `src/SMS_List.c:57-73` vs sibling check `src/SMS_List.c:40`; PushBackBuf's only callers are the three history sites (whole-tree grep) |
| SMS-094 | VERIFIED | Dead test `src/SMS_DSP.c:972` (`!DSP_iFFTInit(...) < 0`); callee returns only 0/1 (`src/SMS_DSP.c:892,929`); callers ignore the result `src/SMS_AAC.c:5993-5994`, `src/SMS_OGG.c:3979-3980`, `src/SMS_WMA.c:1539` |
| SMS-095 | VERIFIED-DRIFT | No NULL check between allocation and writes — allocation is at `src/SMS_DSP.c:945` (not `:952`), derived pointers `:953-954`, fill loop writes `:961-970`; contrast checked allocation `src/SMS_DSP.c:892` |
| SMS-096 | VERIFIED | Dead clamp computed `src/SMS_Container.c:320-321`, unused by `Read` at `:322`; unalloc against stream 0 at `:326` vs allocation from `m_pStm[*apIdx]` at `:313-315`; unchecked `_AllocPacket` result dereferenced at `:322` (NULL return path `src/SMS_Container.c:62-82`) |
| SMS-097 | VERIFIED-DRIFT | Division by file-supplied `m_BitRate` at `src/SMS_Container.c:239-240` with no nonzero gate (provenance `src/SMS_ContainerASF.c:350,676`, `src/SMS_ContainerMOV.c:449`, `src/SMS_Container.c:342`); product wraps in 32-bit arithmetic (see reasoning — it is unsigned, not `int`) |
| SMS-098 | VERIFIED | Explicit path `src/SMS_Container.c:198` indexes `s_CCreator[aPrefCont]` unchecked and discards the return, vs checked automatic path `:151-154`; sole explicit callers `src/SMS_ContainerM3U.c:186,485` passing constants from `:469-474`-region assignments |
| SMS-099 | VERIFIED | Backward slash scan with no start-of-string stop `src/SMS_Container.c:292-298` (starts at last character, decrements until `'\\'`/`'/'` only); callers as cited in `SMS_ContainerAAC.c:217`, `SMS_ContainerASF.c:714`, etc. |
| SMS-100 | VERIFIED | Same record loop three times: `src/SMS_History.c:41-52` (fio/CWD), `:78-89` (fio/BDM, body identical except fd source), `:97-108` (libmc, genuinely different API) |
| SMS-101 | VERIFIED | Read-whole-file block three times: `src/SMS_Locale.c:838-848` (fio/CWD), `:871-881` (fio/BDM), `:885-895` (libmc); lowercase retry twice at `:832-836` and `:866-870` |
| SMS-102 | VERIFIED | `src/SMS_Container.c:149` dimensions `lCC[sizeof(s_CCreator)]` = 52 slots for the 13-entry table at `:45-60` (12 creators + NULL) — 39 unused pointers = 156 stack bytes; loop/memcpy in the same function use the correct forms (`:153,160,165`) |
| SMS-156 | VERIFIED | Declaration/definition only, no caller in whole-tree grep: `GS_XYZF` (`src/SMS_GS_0.S:52,449`, decl `include/SMS_GS.h:1268`), `SMS_iTimerReset` (`src/SMS_Timer.s:28,154`, decl `include/SMS_Timer.h:25`), `inet_ntoa` (`iop/SMSTCPIP/src/inet.s:18,618`, decl `iop/SMSTCPIP/include/lwip/inet.h:49`); sibling `SMS_TimerReset` is live (`src/SMS_GUI.c:850` etc.) |
| SMS-157 | VERIFIED | Export-only names sharing live bodies: `GS_RRV`/`_gs_rrv` `src/SMS_GS_0.S:60,828-829` (live via `:1037`); `_MPEG_AlignBits`/`_ipu_align_bits` `src/libmpeg_core.S:23,501-502` (live via `:524`-region call); `ip_cksum`/`_ip_cksum` `iop/SMSTCPIP/src/inet.s:12,53-54` (live via `:227,266,356`); `inet_aton`/`_inet_aton` `iop/SMSTCPIP/src/inet.s:16,397-398` (live via `inet_addr` at `:607`) — no caller uses the exported name in any case |
| SMS-123 | REFUTED | Reopened claim (a *reachable* off-by-one OOB read) fails: script recount + first-stage segment tiling prove max reachable offset = last valid index on every two-step codebook (table in §2) — use sites `src/SMS_AAC.c:6464,6531`, checks `:6467,6534` |
| SMS-172 | VERIFIED | General rule at `wiki/Devices.md:48,56`, `wiki/Settings.md:64`, `docs-site/content/devices.html:62`, `docs-site/content/settings.html:80`, `docs/devices.html:85`, `docs/settings.html:103`; MX4SIO/MMCE rows under literal `if (1)` at `src/SMS_GUIMenuSMS.c:669,676` with the design comment immediately above; per-device truth at `wiki/Mx4sio.md:23,65`, `wiki/Mmce.md:70` |
| SMS-173 | VERIFIED | `aBias` occurs only in the two signatures `src/SMS_AC3_imdct.c:533,579` (whole-file grep) and nowhere in either body; caller plumbing `src/SMS_AC3.c:546,586,2401,2426-2434,2440,2475-2480,2593`, decls `src/SMS_AC3.c:174-175`, field `include/SMS_AC3.h:94` |
| SMS-174 | VERIFIED | Integer tables defined only: `src/SMS_DTS.c:1162` (`lossy_quant[32]`), `:1184` (`lossless_quant[32]`); no code reference anywhere in the tree; sole consumers use the `_d` twins at `src/SMS_DTS.c:11171-11172` — 2×32×4 = 256 bytes |
| SMS-175 | VERIFIED | `do_accept` body `iop/SMSTCPIP/src/api_msg.c:590-609` never posts the completion mbox; dispatch slot `api_msg.c:726`, enum `iop/SMSTCPIP/include/lwip/api_msg.h:55`, macro `api_msg.h:94`; every `msg->type` assignment in `api_lib.c` enumerated (`:154,177,275,307,330,359,441,481,510,568`) — `API_MSG_ACCEPT` is assigned nowhere |
| SMS-176 | VERIFIED | `_smap_emac3_ready` occurs only at its definition `iop/SMSMAP/smap.h:60-72` in a whole-tree grep; no caller in any `.c`/`.h`/asm file |
| SMS-177 | VERIFIED | `_ifft2` dead dataflow `src/SMS_AC3_imdct.c:290-295`: loads at `:290-291` feed only the `pcpyld`s at `:292,294`, both overwritten by the loads at `:293,295` before any read; `BUTTERFLY_ZERO` `pxor $at` at `:376` overwritten by `nor $at` at `:377` which does not read `$at` |
| SMS-178 | VERIFIED | `_ifft128` first half `src/SMS_AC3_imdct.c:522-525` is statement-for-statement `_ifft64`'s body at `:513-516` (same calls, offsets, `s_Roots64`, `aN=16`) instead of a single `_ifft64(apBuf)` call |
| SMS-181 | WEAKENED | Stubs returning success are real (`iop/USBD/src/interface.c:216-229`, exports `iop/USBD/src/exports.tab:18-20`, `USB_RC_OK`=0 at `iop/USBD/include/usbd.h:204`), but the header *does* disclose their status — `// these aren't implemented:` at `iop/USBD/include/usbd.h:248` — contrary to the finding's "no hint" claim (see §2) |

---

## 2. Full reasoning for every non-VERIFIED item, and for SMS-123

### SMS-095 — VERIFIED-DRIFT (anchor corrected, mechanism intact)

The finding cites the `m_pCos` allocation at `src/SMS_DSP.c:952`. At the pin, line 952 is inside the inline-asm block that follows the allocation; the allocation itself is at **`src/SMS_DSP.c:945`**:

```c
s -> m_pCos = ( float* )memalign ( 16, n4 * sizeof ( float ) * 2 + n2 * sizeof ( float ) );   // :945
s -> m_pSin = s -> m_pCos + n4;                                                                // :953
s -> m_pTmp = s -> m_pSin + n4;                                                                // :954
for ( i = 0; i < n4; i += 2 ) { … s -> m_pCos[ i ] … s -> m_pSin[ i ] … }                       // :961-970
```

There is no NULL test anywhere between :945 and the writes at :966-969, while the neighbouring `DSP_iFFTInit` does check its identical allocation (`src/SMS_DSP.c:892`, `if ( !s -> m_pExpTab ) return 0;`). The defect, its trigger (allocation failure), and the proposed one-line fix all stand exactly as filed; only the allocation's line number drifts by 7.

### SMS-097 — VERIFIED-DRIFT (arithmetic type corrected, both consequences intact)

The division site and provenance are exactly as filed: `src/SMS_Container.c:239-240` divides by `lpCodecCtx->m_BitRate` when `m_FrameSize <= 1`, with no nonzero gate anywhere on the path, and `m_BitRate` is assigned from raw file fields at `src/SMS_ContainerASF.c:350,676` (`*8` of a file value / a file-supplied array), `src/SMS_ContainerMOV.c:449` (raw big-endian file read), and the WAV path at `src/SMS_Container.c:342` (`File_GetUInt(...) * 8`, zero if the field is zero). Reachability of the `m_FrameSize <= 1` branch is in fact stronger than the finding's INFERRED note: `src/SMS_ContainerASF.c:291` explicitly sets `m_FrameSize = 1` for one codec path. Callers are as cited (`SMS_ContainerASF.c:619`, `SMS_ContainerAVI.c:837`, `SMS_ContainerMOV.c:1271`, `SMS_ContainerMPEG_PS.c:574`).

The correction is to the overflow description. The finding calls `apPkt->m_Size * 8 * lpCodecCtx->m_SampleRate` "`int` arithmetic". At the pin `SMS_AVPacket.m_Size` is `uint32_t` (`include/SMS.h`, `SMS_AVPacket` definition) and `m_SampleRate`/`m_BitRate` are `int32_t` (`include/SMS_Codec.h:72-73`), so the product is evaluated in **unsigned 32-bit** arithmetic and wraps modulo 2³², and the wrapped value is then converted to the `int` `lNum`. The numeric example still holds: 65,536 × 8 × 48,000 = 25,165,824,000, which wraps to 3,690,987,520 (≥ 2³¹, converting to a negative `int` on this target) — so large-packet streams still get garbage/negative durations, and a zero bit rate still divides by zero before the `if (lDen && lNum)` guard at `:245` can help. Substance verified; the signed-`int`-arithmetic wording is corrected to unsigned-wrap-then-convert.

### SMS-181 — WEAKENED (stubs and false success are real; "no hint of their status" is false in-repo)

What survives, all line-read: the three bodies at `iop/USBD/src/interface.c:216-229` are each only a `dbg_printf("… stub\n")` and `return 0;`; they are exported at `iop/USBD/src/exports.tab:18-20` (slots 14–16); 0 is `USB_RC_OK` (`iop/USBD/include/usbd.h:204`); and a whole-tree grep finds no in-tree caller — the consumers are out-of-tree IOP drivers, as the finding says. A caller that calls `UsbRegisterAutoloader`/`UsbChangeThreadPriority` and checks the return therefore does receive a success that means nothing.

What does not survive is the finding's framing that the functions are "declared in the public header with no hint of their status" and that "every in-tree reader sees a stub, every out-of-tree caller sees success". The public header itself, directly above the three declarations, says so in plain text:

```c
// these aren't implemented:                                  // iop/USBD/include/usbd.h:248
int UsbRegisterAutoloader(UsbDriver *drv);                    // :249
int UsbUnregisterAutoloader(UsbDriver *drv);                  // :250
int UsbChangeThreadPriority(void);                            // :251
```

Any driver author compiling against this header — the only supported way to call these exports — sees that comment. The narrower true claim is therefore: **the runtime contract lies (success is returned for a no-op), but the source-level disclosure exists**; the defect is the dishonest return value, not an undisclosed stub. The proposed fix (return a non-zero `USB_RC_*`, keep the export slots) is unaffected.

### SMS-123 — REFUTED (reopened claim: a *reachable* off-by-one OOB read does not exist at this pin)

**What is literally true in the original finding** (re-verified): in both decoders the table is indexed before the check — `hcb_2_quad_table[cb][offset].bits` is read at `src/SMS_AAC.c:6464` and `hcb_2_pair_table[cb][offset].bits` at `:6531`, while the only range tests are at `:6467` (`if (offset > hcb_2_quad_table_size[cb]) return 10;`) and `:6534`. The ordering defect as a piece of code hygiene stands.

**The reopened question** is whether the file-influenced `offset` can actually reach an index past the *actual* table end at those use sites. It cannot, on any codebook, by script proof:

1. `cw` is bounded by construction: `cw = SMS_SHOW_UBITS(r, ld, hcbN[cb])` (`:6458/:6525`), with `hcbN` = {5,5,5,5,5,6,5} for codebooks {1,2,4,6,8,10,11} (`src/SMS_AAC.c:2136-2138`), and `SMS_SHOW_UBITS` is `SMS_NEG_USR32(cache, num)` = `(uint32_t)cache >> (32 - num)` (`include/SMS_Bitio.h:35`, `include/SMS.h:68`) — a value in `[0, 2^num)`, exactly indexing the 32-entry (64 for cb10) first-stage tables.
2. `offset = hcb_table[cb][cw].offset + SMS_SHOW_UBITS(r, ld, extra_bits)` (`:6459,6463` / `:6526,6530`), where the added value is in `[0, 2^extra_bits)` by the same macro. So the maximum reachable offset for a codebook is `max over first-stage entries of (offset + 2^extra_bits − 1)`, computable entirely from the static tables — the file chooses *which* entry and *which* in-segment value, nothing more.
3. I re-derived both number sets by script from the initialisers at the pin (counting top-level `{…}` entries of each `hcbN_2[]` array, and every `{offset, extra_bits}` pair of each `hcbN_1[]` array) — not trusting either the original finding or SMS-160:

| cb | Family | Declared size (source) | Actual entries (script count) | Last valid index (actual−1) | Max reachable offset (script) | Excess past actual end |
|---|---|---|---|---|---|---|
| 1 | quad (`hcb1_2`, `src/SMS_AAC.c:2197`) | 114 (`:2307-2308`) | 113 | 112 | 112 (base 49, 6 extra bits: 49…112) | **0** |
| 2 | quad (`hcb2_2`, `:2229`) | 86 (`:2307-2308`) | 85 | 84 | 84 (base 69, 4 extra bits: 69…84) | **0** |
| 4 | quad (`hcb4_2`, `:2254`) | 185 (`:2307-2308`) | 184 | 183 | 183 (base 56, 7 extra bits: 56…183) | **0** |
| 6 | pair (`hcb6_2`, `:2530`) | 126 (`:2746-2747`) | 125 | 124 | 124 (base 61, 6 extra bits: 61…124) | **0** |
| 8 | pair (`hcb8_2`, `:2565`) | 83 (`:2746-2747`) | 83 | 82 | 82 (base 51, 5 extra bits: 51…82) | **0** |
| 10 | pair (`hcb10_2`, `:2589`) | 210 (`:2746-2747`) | 209 | 208 | 208 (base 145, 6 extra bits: 145…208) | **0** |
| 11 | pair (`hcb11_2`, `:2645`) | 373 (`:2746-2747`) | 374 | 373 | 373 (base 246, 7 extra bits: 246…373) | **0** |

   As a further check, the union of all first-stage segments `[offset, offset + 2^extra_bits − 1]` (with `extra_bits = 0` giving a single index) covers **exactly** `0 … actual−1` for every codebook — the segments tile the second-stage tables with no gap past the end and no segment extending beyond it. My actual counts agree with SMS-160's recount (declared = actual + 1 for 1/2/4/6/10, exact for 8, declared = actual − 1 for 11), confirming that part of the phase-1 record.

   **Conclusion on the reopened claim:** there is no reachable out-of-bounds read at `:6464`/`:6531` (nor at the post-check `sp[]` reads at `:6468-6471`/`:6535-6536`) on any codebook, by 0 entries. The original Problem text's mechanism — "nothing constrains those bits to the valid codeword sub-range, so `offset` can be driven past the segment and the table end" — is the defeating point: the extra bits *are* the sub-range, and every sub-range ends at or before the table end. The check at `:6467`/`:6534` is, with the tables as built, also never triggered (max reachable 112 < 114, …, 373 is not > 373). The reopened claim "a real off-by-one OOB read exists on the stated codebooks" is therefore **REFUTED**; what remains of SMS-123 is a latent mis-ordered, effectively dead guard — worth the reorder as defence-in-depth, not a live OOB.

**Interlock arithmetic (as assigned, verified in its own terms):**

- **(a) Reorder-only fix — threshold arithmetic: TRUE, reachability: NO.** Moving the existing test (`offset > declared`) above the use would *admit* (pass without error) every offset ≤ declared. Against the actual tables that threshold admits two out-of-bounds indices on each of codebooks 1/2/4/6/10 — e.g. cb1: declared 114, actual entries 0…112, so indices 113 and 114 pass the reordered check (max admitted offset = last valid + 2); identically cb2 (85, 86), cb4 (184, 185), cb6 (125, 126), cb10 (209, 210). So as a statement about what the *check* bounds, "still admits 1–2 entries past end" holds (exactly 2 indices by this count). It does **not** make any of those indices reachable: per the tiling proof above, no file input produces them. A correct fix must use the actual counts (actual−1 as max index, i.e. reject `offset >= actual_count`) — which is SMS-160's correction — but the reason is bound correctness, not a presently reachable read.
- **(b) `>=`-on-declared fix falsely rejecting cb11: TRUE, and this one is reachable.** Codebook 11's declared size is 373 (`:2747`) but its actual table has 374 entries (indices 0…373), and offset **373 is genuinely reachable** (first-stage entry base 246 with 7 extra bits, range 246…373 — script-verified above) and is the table's last valid entry. A tightened test of the form `if (offset >= hcb_2_pair_table_size[cb]) return 10;` would therefore reject valid codebook-11 streams at exactly offset 373. Any SMS-123 remediation must not apply `>=` against the declared arrays without first applying SMS-160's corrected bounds.

---

## 3. Closing list — incidental new defects

None. No code-proven defect outside the 21 assigned items was found during this batch's reads; candidates encountered along the way (e.g. the shared static empty packet in `_AllocPacket`, `src/SMS_Container.c:64-66`) did not survive scrutiny as proven defects and are not filed, per instructions to file sparingly and only on code proof.
