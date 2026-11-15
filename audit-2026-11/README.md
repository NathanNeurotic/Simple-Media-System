# Five-repo audit — both finals + comparison (reports only)

This folder contains the complete deliverables of two independent LESS-NOT-MORE audits of **NathanNeurotic/Simple-Media-System** at commit **e65ced39a27e** (branch `master`), plus the mechanism-level comparison of the two finals. It is documentation only: **no code in this repository is changed by these files.** The accompanying pull request is a draft, opened at the owner's direction as a delivery vehicle; whether anything here is ever merged or implemented is the owner's decision.

## The two runs

- **Original audit** (`old/`) — nine rounds over 2026-11-13/14 plus two adversarial verification phases. Final: 1,200 numbered items across five repos, 1,182 actionable, 18 withdrawn. Start with `old/Sable-SUMMARY.md` and `old/Sable-MASTER-INDEX.md` (the index's §4 ledger overrides original finding text where they conflict). Implementation guide: `old/Sable-HANDOFF-PROMPT.md`.
- **Blind redo** (`redo/`) — a second team re-audited from the code alone, forbidden from reading the original's deliverables, then went through its own adversarial verification and reconciliation. Final: 885 numbered items, 879 actionable, 6 withdrawn. Start with `redo/Redo-SUMMARY.md` and `redo/Redo-MASTER-INDEX.md`.
- **Comparison** (`redo/Comparison-FINAL.md` and `redo/Compare-*.md`) — every finding in both finals matched mechanism-by-mechanism; non-matching Category 6 items were adjudicated against the code (REAL / NOT-REAL / COVERAGE). Union of the two finals: ≈1,557 distinct mechanisms. Neither final contains the other.

## This repo

Old final: 188 numbered findings, 184 actionable. Redo final: 150 numbered, 149 actionable. Independent overlap: 66 full matches + 22 partial; 264 distinct mechanisms across the two finals for this repo.

**Most consequential old-only finds:** SMS-188 (subtitle width function reads past its glyph table on every SB-mode computation — no crafted file needed), SMS-160 (AAC Huffman declared size arrays wrong in both directions, script-counted), SMS-124 (AAC section_data writes before its bounds check), SMS-131 (SMAP RX length never clamped to the pool slot).
**Most consequential redo-only finds:** RE-SMS-007 (`SifExecDecompModuleBuffer` treats any XZ error as success and executes the undefined output buffer as an IOP module, ~25 call sites), RE-SMS-003 (`ip_reass` byte-swapped length — ~11 KB copy into a 1,540 B pool buffer), RE-SMS-066 (`ps2ip_getconfig` strcpy of an unbounded interface name into a 4-byte field).

## Strike list — do not implement as filed

- REDO **RE-SMS-061** — not real as filed: the AC3 parser's own step bounds cap the index at 255 for parsed data.
- REDO **RE-SMS-001** — already withdrawn by the redo's own verification (unsigned-conversion guard). 
- OLD SMS-013 / SMS-015 / SMS-089 / SMS-123 — already withdrawn in the old final; SMS-123's Huffman-tiling rationale was proven by script in the old verification phase 2. Do not re-file them.
- Cross strike: old X-32 item D5 ruled SMS's UDPRDMA ACK test "functionally equivalent" — **overturned** by redo RE-X-08: any stale ACK completes an SMS send. See `redo/Compare-Cross.md`.

(Cross-repo strikes that affect every repo: old X-32 item D5's equivalence ruling is overturned by RE-X-08; old X-38's attr limb does not exist. Both finals' affirmative "this area is clean" lists should be treated as unverified — each run produced several wrong ones.)

## Layout

- `old/` — original audit: per-repo reports and round files, verification reports (`Sable-VERIFY-*`, `Sable-VERIFY2-*`), cross-repo reports, master index, summary, pins, handoff prompt, and section-level evidence.
- `redo/` — redo audit: per-repo reports (`Redo-*`), its verification reports (`Redo-VERIFY-*`), new findings from verification (`Redo-NEW-FINDINGS.md`), master index, summary, and the comparison reports (`Compare-*.md`, `Comparison-FINAL.md`) with their evidence sections.
- Shared/global files (indexes, summaries, cross-repo reports) are included in every repo's package because the cross findings and indexes span all five repositories.

## Archives (download links expire 2026-11-17 ~10:55 UTC; anyone with a link can download)

- Both finals + comparison, all five repos (699 files, 6,078,940 B, SHA-256 `52824c1ac885f9047003d2ada77d8d3c47ca5b61819fb1d7b7474033c3cbb862`): https://muse.ai/files/570121466194596/2866327167094404/ckbdv71m8bppkfs4su9uz67z/Sable-audit-BOTH-FINALS-2026-11-15.zip
- This repo's package: https://muse.ai/files/570121466194596/1509715437928219/0nq6lu83kx19q0wvi6w2wa1q/Sable-audit-SMS-both-finals-2026-11-15.zip
- Original final only (564 files, SHA-256 `2773df7c03bce277449a0acd293c63a50fe1177824ddcacc02f7885662d7388d`): https://muse.ai/files/570121466194596/1668795564645485/t1kdjzqeri3kpji8hi2aw4b5/Sable-audit-complete-2026-11-14.zip

## Limits

Nothing in either audit is PS2-build- or hardware-verified (no PS2SDK in the audit environment); performance findings are structural; live UDP interop was sandbox-blocked. Pins: RiptOPL `2e347d5ac885`, Launchstation `9fb0598a79f0`, POPSLoader `29272cddbf01`, Simple-Media-System `e65ced39a27e`, PS2-Servers `c5f021c4d386`.
