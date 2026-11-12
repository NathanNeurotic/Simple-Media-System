# Handoff material for `fix/open-issues-2026-11`

Working material behind `../2026-11-11-open-issues-triage.md` (the work list) and
`../2026-11-11-review-notes.md` (review points and hardware tests). Delete this
folder before the branch is merged to `master`.

## findings/

One file per investigation (nine open issues, six subsystem hunts), as written
by the investigators at `c275476`, followed by the independent checker's
verdict on each finding. Where a verdict disputes the proposed fix it gives a
corrected one; the committed change follows the verdict. Paths to the
investigators' private scratch files were removed; those files were not kept.

## samples/

Media used by the hardware tests in the review notes and by two triage entries.

| File | What it is |
| --- | --- |
| `adts_len0.aac`, `adts_len4.aac` | ADTS stream with one frame's length field set to 0 and to 4 (shorter than the header) |
| `png512.bin` | the first 512 bytes of a PNG; must not be taken for MP3 |
| `wmav2_ch0.wma`, `wmav2_ch6.wma`, `wmav2_ch65535.wma` | valid stereo WMA v2 with the header channel count edited |
| `asf_frag_crash.asf` | mutated ASF that crashed the old reader (fragment offset past the packet) |
| `wmv_bitrate_1_2.wmv` | WMV whose bitrate table uses stream numbers 1 and 2 |
| `short1.mp3`, `short3.mp3`, `short5.mp3` | very short MP3s that must still be detected |
| `t*.mp3`, `t50ms.mp2` | tiny encoder output: CBR, VBR/Xing, ID3v1/v2, 22.05 kHz mono, 8 kHz, MP2 |

## scripts/

- `host-tests.sh [tools/test_x.py ...]`: runs the CI host tests (all of them
  with no arguments) from the repository root on Linux or WSL Debian.
- `ps2-build.sh <out dir> [make args]`: builds the committed tree in a copy
  with the CI Docker image. Run it with `all pack`, `BDM=0 all` and
  `SMS_DIAG=1 all`.
