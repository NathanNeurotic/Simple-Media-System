## Reading these files

These are the investigators' records at `c275476`, each finding followed by its
checkers' `VERDICT` blocks. Until `docs: put each investigation verdict under the
finding it assessed`, many verdicts sat under a neighbouring finding: the
checkers were numbered in severity order, and the files attached them in
original order. That commit rebuilt the placement from the investigation log;
the work list's "independent check" fields were corrected at the same time.
Findings marked "Not independently checked" were below the verification
cut-off, or their checker did not finish.

Lines starting `> Review note:` were added during review of the branch. The
status of every finding is in `../2026-11-11-open-issues-triage.md`.
