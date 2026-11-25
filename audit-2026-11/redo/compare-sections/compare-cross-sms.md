# Comparison — Cross-repo / SMS + network stacks (OLD final vs REDO final)

Cluster: SMS / network stacks. Method: mechanism-level matching, bodies read on both sides;
every PARTIAL/NONE re-derived from the read-only clones at the shared pins
(RiptOPL `2e347d5ac885`, Launchstation `9fb0598a79f0`, POPSLoader `29272cddbf01`,
Simple-Media-System `e65ced39a27e`, PS2-Servers `c5f021c4d386`).
OLD final scope taken from `Sable-VERIFY2-CROSS.md` + `Sable-MASTER-INDEX.md` X-rows;
REDO final scope from `Redo-Cross.md`, `Redo-SMS.md`, `Redo-NEW-FINDINGS.md`,
`Redo-VERIFY-Cross.md`, `Redo-MASTER-INDEX.md`.

**Counts (11 rows): MATCH 1 · PARTIAL 2 · NONE 8.**

| # | OLD ID | REDO ID(s) | Verdict |
|---|---|---|---|
| 1 | X-21 | — (none) | **NONE** |
| 2 | X-29 | — (none) | **NONE** |
| 3 | X-35 | — (none) | **NONE** |
| 4 | X-36 | — (none) | **NONE** |
| 5 | X-47 | RE-X-22 + RE-X-32 (instances only) | **PARTIAL** |
| 6a | X-32 item D4 | RE-X-06 | **MATCH** |
| 6b | — (none) | RE-X-07 | **NONE** (redo-only, real) |
| 6c | X-32 item D5 (site examined, ruled equivalent) | RE-X-08 | **PARTIAL** (old false negative) |
| 6d | — (none) | RE-X-09 | **NONE** (redo-only, real) |
| 7 | — (none) | RE-X-27 | **NONE** (redo-only, real) |
| 8 | — (adjacent SMS-152 only) | RE-X-32 | **NONE** (redo-only, real) |

---

## 1. X-21 — SMS file-copy failure handling — **NONE** (old finding real, redo missed it)

- **Final old scope (VERIFY2):** every *streaming* copy path leaves truncated copies
  indistinguishable from complete ones; the buffered `_copy_file_to_mc` path is the one
  counterexample and already deletes on failure (X-21 REVISED/WEAKENED to that scope).
- **Redo search:** no copy-failure / truncation / delete-on-failure mechanism anywhere in
  `Redo-SMS.md`, `Redo-Cross.md`, or `Redo-VERIFY-SMS.md`. Redo's only adjacent filing is a
  different mechanism at the same file: `_copy_file_to_mc`'s whole-file `malloc(lSize)` +
  read (file-size-trust family, `Redo-SMS.md` RE-SMS finding at `src/SMS_GUIFileCtxMenu.c:143-196`)
  — it never mentions that function's delete-on-failure behaviour, nor `SMS_CopyFile`'s
  failure exits. Redo did read `SMS_CopyTree.c` (cites `:150-151` for an unchecked
  `SMS_DirTreeInit` result), so this is not a files-not-read gap.
- **Adjudication: REAL.** Clone, SMS `src/SMS_CopyTree.c` (`SMS_CopyFile`, ~:298-387):
  read loop breaks on `lnRead <= 0` — a mid-stream read error is indistinguishable from EOF
  and returns success with a short file; write failure sets `retVal = 3` via the `fioSync`
  short-write check, cancel returns 2 — and there is **no `fioRemove` of the destination
  anywhere in the function** (the file's only `fioRemove` is in `SMS_DeleteTree`).
  `SMS_CopyTree`'s error exits likewise never remove the partial destination.
  Counterexample confirmed in clone: `src/SMS_GUIFileCtxMenu.c` `_copy_file_to_mc` —
  `if (!retVal) { fioRemove(apDst); fioRmdir(apDst); }` after the write. The final-scoped
  old finding stands exactly as reconciled; redo has no equivalent at any level.

## 2. X-29 — SMS positional SMB list, blank line shifts every record — **NONE** (real, redo missed it)

- **Redo search:** no positional-format / blank-line / resync mechanism in `Redo-SMS.md` or
  `Redo-Cross.md`. Redo examined this exact loader and cleared its parsing: worker section
  `redo-sms-W4.md` states `SMS_LoadSMBInfo` "is bounded per-field; its remaining defects are
  the allocation ones…, not the parsing"; the final's only findings touching these lines are
  allocation-NULL-deref mechanisms (`SMS_ListPushBack (…) -> m_Param` at `src/SMS_Config.c:366`).
  Different mechanism, same lines.
- **Adjudication: REAL.** Clone, SMS `src/SMS_Config.c` `SMS_LoadSMBInfo` (~:314-379): six
  unconditional `File_GetString` calls per record (fields 7-8 gated on a single EOF check);
  no keys, comments, or blank-line skipping. `File_GetString`
  (`src/SMS_FileContext.c:2176-2208`) returns `""` for a blank line while consuming it, so one
  extra line shifts every subsequent field of every subsequent record, and nothing in the
  format can resynchronise framing. The clone's later local mitigations (all-digit port check,
  non-empty IP/name acceptance gate) can only *drop* shifted records — they cannot recover
  alignment, because consumption is purely sequential. Old X-29 (STANDING in final) stands.

## 3. X-35 — SMS main config save never change-gated, fires as a side effect — **NONE** (real, redo missed it)

- **Redo search:** no change-gating / dirty-check / side-effect-save mechanism in `Redo-SMS.md`,
  `Redo-Cross.md`, or the redo cross worker sections (grep for `SaveConfig` / change-gate /
  dirty across `redo-cross-*.md`: zero hits). Redo did read the save function's body — its
  TEMP-diagnostic `sprintf` overflow finding cites `src/SMS_Config.c:943,945,1082-1087`, inside
  `SMS_SaveConfig` — and read the side-effect caller file `SMS_GUIFileCtxMenu.c` for the
  `_copy_file_to_mc` finding, without noting the gating absence or the callers. Not a
  files-not-read gap.
- **Adjudication: REAL.** Clone, SMS `src/SMS_Config.c:878-1101` `SMS_SaveConfig`: full-body
  grep finds no dirty check, snapshot compare, or changed-flag test — the function always
  writes. Unconditional side-effect callers confirmed in clone: `src/SMS_IOP.c:1786`
  (`SMS_SaveConfig();` as step 1 "Persist BEFORE anything is torn down" on every live network
  switch) and `src/SMS_GUIFileCtxMenu.c:212,235,309,334` (save as a gate before copy-to-MC
  operations). Old X-35 (STANDING in final) stands.

## 4. X-36 — SMS carries two UDPFS front-ends it never builds (bd + fhi, 267 lines) — **NONE** (real, redo missed it)

- **Redo search:** no unbuilt-front-end finding in `Redo-SMS.md` or `Redo-Cross.md`. Redo's SMS
  worker inventory (`redo-sms-W6.md`) lists `udpfs/src/udpfs_bd.c (132)` and
  `udpfs/src/udpfs_fhi.c (135)` in its file census but never checks whether they are built;
  redo's only SMSUdpfs-main finding is that `main.c` ignores the `udpfs_init()` /
  `udpfs_bd_init()` / `fhi_init()` returns — a different mechanism, and those calls are
  `#ifdef`-gated per variant, so it does not establish liveness. Redo did file unbuilt-code
  findings elsewhere in SMS (SMSSMB deletion, SMSTCPIP dead code, ministack `udptty`
  "deliberately unbuilt" in Checked-clean), so dead-code hunting was in its SMS scope.
  Partial scope caveat: redo's files-not-read lists "per-directory IOP Makefiles…
  listed/counted only", which covers the variant arms — but the decisive build proof also
  lives in `tools/build_udpfs.sh` and the top-level `Makefile`, both in redo's read set.
- **Adjudication: REAL (verified at the pin).** Clone, SMS `iop/SMSUdpfs/udpfs/src/`:
  `udpfs_bd.c` = 132 lines, `udpfs_fhi.c` = 135 lines (267 total, as filed).
  `iop/SMSUdpfs/udpfs/Makefile` has the three variant arms (`UDPFS_BD` / `UDPFS_IOMAN` /
  `UDPFS_FHI`, default all 0); `tools/build_udpfs.sh:54` builds only
  `make -C udpfs clean all UDPFS_IOMAN=1`; top-level `Makefile:53` embeds only
  `udpfs_ioman_irx.o`. The bd/fhi front-ends are never built by SMS's build. Old X-36
  (STANDING in final) stands.

## 5. X-47 — SMSTCPIP as two diverged forks, no cross-port route — **PARTIAL** (instances only; umbrella absent)

- **Old X-47 (STANDING in final):** an umbrella finding — the whole stack exists as two forks
  at two patch levels (RiptOPL hardened series vs SMS pristine), proven by a full census:
  all 19 shared `.c` files differ, ~11,072 vs ~9,642 lines, per-file diff counts
  (pbuf.c 826, api_lib.c 1,066, etharp.c 969…); the *fix* is not code but a cross-port route
  (attribution note in both trees + port correctness/security fixes to the sibling in the
  same change), with the census as the drift baseline.
- **What redo has:** (a) a one-row census acknowledgement in `Redo-Cross.md` — family
  "TCP/IP | 2 lwIP trees (~60 files each) | … SMS lwIP carries a one-byte bitmap bug
  RiptOPL fixed" — with no file-by-file divergence census, no line totals, no
  hardened-vs-pristine characterisation, and no cross-port-route prescription;
  (b) two cross-repo *instances* of exactly X-47's disease: **RE-X-22** (SMS `ip_frag`
  reassembly bitmap one byte short; RiptOPL carries the `+1` fix) and **RE-X-32**
  (SMS `ip_frag` completeness loop; RiptOPL carries the fixed bound — see row 8);
  (c) per-repo instances on both sides (redo SMS lwIP defects incl. RE-SMS-016's
  IPH_LEN-trust / bitmap-index-OOB in the same `ip_frag.c`; redo RiptOPL SMSTCPIP defects).
- **Adjudication: PARTIAL.** The umbrella claim — total divergence as the finding, with the
  census as baseline and the cross-port discipline as the fix — is **not** matched anywhere
  in the redo final; only instances are. Clone spot-check supports the old census shape:
  `diff -rq` of `RiptOPL/modules/network/SMSTCPIP` vs SMS `iop/SMSTCPIP` shows the trees
  diverged throughout (including every shared header), and RiptOPL's `.c` total reproduces
  at 11,072 lines. Note for fairness: old VERIFY2 itself marked X-47's "SMS pristine"
  sub-characterisation UNVERIFIABLE-HERE (no upstream copy in the clones) — the verdict
  stands on the divergence, which is also the part redo did not census.

## 6a. RE-X-06 (SMS bread EOF/short-read) vs old X-32 item D4 — **MATCH**

Old X-32 §3.2 item **D4** (`Sable-CROSSREPO-ROUND3.md`) files this exact mechanism as a
cross-repo divergence: SMS `udpfs_core_bread` fails only on `ret < 0`, so EOF (0) counts the
whole chunk as read and a short read leaves stale buffer bytes returned as disc data
(SMS `udpfs_core.c:714-717`); RiptOPL treats `ret <= 0` as failure and zero-fills the
short-read tail (`udpfs_core.c:680-688`). RE-X-06 is the same mechanism, same files, same
fix direction (port RiptOPL's two behaviours into SMS), at the same (cross) level. Match.

## 6b. RE-X-07 (SMS SMAP FIFO read drops last byte of odd-sized frames) — **NONE** (redo-only, real; old missed it)

- **Old search:** X-32's item list (D1-D6) covers `udprdma.c` / `udpfs_core.c` drift only —
  the SMAP `xfer.c` FIFO tail is not among them. Grep of the full old per-repo corpora
  (`Sable-Simple-Media-System*.md`, `Sable-RiptOPL*.md`) finds no `smap_fifo_read` tail /
  odd-byte finding; the only old mentions of RiptOPL's `test_udpfs_smap_fifo_tail.py` are
  test-infrastructure discussions (rounds 7/8), never the SMS divergence.
- **Adjudication: REAL.** Clone, SMS `iop/SMSUdpfs/smap/src/xfer.c` `smap_fifo_read` PIO tail:
  u32 loop, then a single u16 store `if (i + 2 <= bytes)` — a trailing single byte (or the
  3rd byte of a 3-byte tail) is never stored, leaving stale buffer content. RiptOPL
  `modules/network/udpfs_smap/src/xfer.c` reads the final padded word once and stores only
  the valid bytes, with an in-tree comment naming exactly this upstream defect
  ("upstream stored a u16 for the tail and had no step for a single last byte… the end of
  every odd-sized file read over UDPFS"). Real redo-only finding.

## 6c. RE-X-08 (SMS ACK-coverage test arithmetically inert) vs old X-32 item D5 — **PARTIAL** (old examined the site and ruled it equivalent — a false negative)

- **Old D5** reads the exact expression — SMS inline test at `udprdma.c:613,682`,
  `diff >= 0 || diff < -2048` on `(int16_t)((acked − sent) & 0xFFF)` — and concludes the
  second clause is unreachable "so the test reduces to the same [0, 2047] set as RiptOPL's.
  **Functionally equivalent today (READ)**", filing it as a no-action record.
- **Redo RE-X-08** (VERIFIED in `Redo-VERIFY-Cross.md`) derives the opposite: the masked
  difference is 0…4095, `(int16_t)` of that is never negative, so `diff >= 0` is **always
  true** and *any* ACK — including a stale one for an older packet — completes the send.
- **Adjudication: redo is correct; old D5's arithmetic is wrong.** Re-derived from the clone
  (SMS `iop/SMSUdpfs/udpfs/src/udprdma.c`, `udprdma_send` and `udprdma_send_ll`): operands are
  12-bit sequence numbers; `(acked − sent) & 0xFFF` ∈ [0, 4095]; every value in that range is
  a non-negative `int16_t` (max 4095 < 32768), so `diff >= 0` holds unconditionally and
  `diff < -2048` is dead — the disjunction is always true. It does *not* reduce to
  [0, 2047]: a stale ACK with forward distance 2048…4095 (e.g. acked = sent − 1 → diff 4095)
  passes SMS's test but fails RiptOPL's real coverage test
  (`udprdma_ack_covers`: `((acknowledged − sent) & 0xFFF) < 0x800`, in
  `modules/network/udpfs_ioman/src/udprdma_seq.h`, used by `_wait_ack`). PARTIAL, not MATCH
  (old filed no defect) and not NONE (old read and discussed the exact code — and got the
  verdict backwards, which also weakens X-32's "four drifted behaviours" completeness claim).

## 6d. RE-X-09 (SMS ignores `SetAlarm` failure; unbounded wait) — **NONE** (redo-only, real; old missed it)

- **Old search:** no `SetAlarm`-failure / `_arm_timeout` / `EF_TIMEOUT` mechanism in X-32's
  item list, the old SMS corpus, or the old RiptOPL corpus (grep hits are unrelated: MMCE
  alarm setting, imports censuses). Old X-32 claimed a full-file diff of both UDPRDMA cores,
  so this is a miss inside read scope, not a coverage gap.
- **Adjudication: REAL.** Clone, SMS `iop/SMSUdpfs/udpfs/src/udprdma.c`: raw
  `SetAlarm(&clock, _timeout_cb, socket)` with the return discarded at **four** wait sites —
  :540 (discover), :603 (send), :672 (send_ll), :730 (recv) (`Redo-VERIFY-Cross.md` corrects
  the filed "three" to four). Every SMS wait ends only on a packet flag or `EF_TIMEOUT`,
  which only the alarm raises, so a failed arming hangs the wait indefinitely. RiptOPL
  `modules/network/udpfs_shared/src/udprdma.c` `_arm_timeout` (:105-119) checks the return
  and raises `EF_TIMEOUT` immediately on failure, with a comment stating this exact
  mechanism. Real redo-only finding.

## 7. RE-X-27 (SMS `ums_fat`: FAT chain walks never validate cluster number; BPB fields unvalidated) — **NONE** (redo-only, real; old missed it)

- **Old search:** no cluster-range-validation or BPB FAT-field mechanism in the old SMS
  corpus. Old SMSUMS findings are different mechanisms in the same file: SMS-103 (LFN
  sequence multiplier), SMS-110 (READ CAPACITY sector size — which even cites the BPB
  whitelist at `ums_fat.c:1048-1052` as the *contrast*), SMS-150 (chain walkers' −1 error
  treated as complete chain in `_set_dir_chain`), SMS-151 (ignored sector-data error return).
  Old round-7's coverage table claims a pass on "`FATFS_Init`, chain/direntry parsers" —
  the validation gap was inside read scope.
- **Adjudication: REAL.** Clone, SMS `iop/SMSUMS/src/ums_fat.c`: `_get_cluster_chain12/16/32`
  (:249-355) take the next-cluster value from the FAT and use it directly to compute the next
  FAT sector and as a data cluster, breaking only on the end-of-chain marks
  (≥0xFF8 / ≥0xFFF8 / ≥0xFFFFFFF8); `_data_clusters()` (:230-232) exists but is used only for
  FAT-type detection, never to bound a chain value — a corrupt non-EOC entry steers
  `CacheRead` and `_cluster2sector1216/32` (:234-242) to arbitrary device sectors.
  `FATFS_Init` (:1045-1105) whitelists only sector/cluster sizes and checks the 0xAA55
  signature; `m_nFATs`, `m_RootSize`, and `m_FATSize` (after the FAT32 fallback — zero if
  both are zero) are copied unchecked, so `m_FATSize == 0` collapses
  `m_RootDirStart == m_Start`. (Redo's Stage-B extension, folded into RE-X-27's final scope,
  adds the same zero-check gap for `m_nResSectors`.) Real redo-only finding.

## 8. RE-X-32 (SMS `ip_frag` completeness loop never checks bitmap byte 89) — **NONE** (redo-only, real; old's adjacent SMS-152 is a different mechanism)

- **Old search:** the old SMS corpus's only `ip_frag` completeness-adjacent finding is
  **SMS-152** (round 7): the reassembly bitmap is *indexed at 90* (of 0-89) when the length
  is exactly 5,760 — the update at `:174` and the completion *read* at `:195` go one byte
  out of bounds. That is the sizing/OOB mechanism (redo's RE-X-22 / RE-SMS-016), not the
  loop-bound mechanism: SMS-152 never states that a bitmap byte is *skipped*, and its fix
  (make the `:151` guard `>=`-strict / clamp the endpoint accesses) would not repair the
  skip for any other length. No other old SMS or cross finding mentions the completeness
  loop bound.
- **Adjudication: REAL, and distinct.** Clone, SMS `iop/SMSTCPIP/src/ip_frag.c:192`:
  `for (i = 0; i < ip_reasslen / 64 - 1; ++i) if (ip_reassbitmap[i] != 0xFF) goto nullreturn;`
  followed by a separate check of byte `ip_reasslen / 64` — so byte `ip_reasslen / 64 − 1`
  is checked by **neither**. For a 5,760-byte datagram that is byte 89 (payload 5696-5759):
  a 64-byte hole there passes as complete and the holed datagram is delivered upward.
  RiptOPL `modules/network/SMSTCPIP/ip_frag.c` carries the fixed loop
  (`i < ip_reasslen / (8 * 8)`) with an explicit comment: "The old bound,
  ip_reasslen / 64 - 1, skipped the byte before the last, so a hole there passed as a
  complete datagram." Real redo-only finding (Stage-C NEW, CAND-03), correctly screened
  by redo itself as distinct from RE-X-22 and RE-SMS-016 — the same distinction applies
  against old SMS-152.

---

## Most consequential findings of this cluster

1. **RE-X-08 / old X-32 D5 is the cluster's worst old-side error.** The old audit read the
   exact ACK-coverage expression, declared it "functionally equivalent" to RiptOPL's, and
   closed it — the arithmetic is backwards (the test is always true; any stale ACK completes
   an SMS send). This is a mis-verdict on read code, not a scope miss, and it means X-32's
   "four drifted behaviours, each correct in exactly one fork" undercounts the UDPRDMA drift
   it claimed to have fully accounted (102 changed lines).
2. **The redo missed all four old SMS cross findings put to it (X-21, X-29, X-35, X-36) —
   every one re-derived REAL at the pin**, in files redo demonstrably read (SMS_CopyTree.c,
   SMS_Config.c including the inside of SMS_SaveConfig, SMS_GUIFileCtxMenu.c, the SMSUdpfs
   inventory). X-29 is the sharpest: redo's worker explicitly cleared the SMB loader's
   parsing ("not the parsing") while the positional-desync mechanism stood.
3. **X-47's umbrella is only instance-matched.** Redo found the two best SMSTCPIP instances
   (RE-X-22, RE-X-32 — both in `ip_frag.c`) and acknowledged the two-tree family in one
   census row, but produced no divergence census and no cross-port-route finding; the old
   umbrella (19/19 files differ, two patch levels, drift discipline as the fix) has no redo
   equivalent.
4. **Net drift picture:** the UDPRDMA fork drift is worse than old X-32 filed (D5 inverted,
   plus RE-X-07's SMAP tail and RE-X-09's alarm hang outside X-32's item list), while the
   one clean cross match in the cluster is RE-X-06 = X-32 D4.
