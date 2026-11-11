#!/bin/bash
# Build the committed tree in a throwaway copy with the CI toolchain image.
# usage: docs/investigations/2026-11-11-handoff/scripts/ps2-build.sh <out dir> [make args, default: all]
# CI builds "all pack", "BDM=0 all" and "SMS_DIAG=1 all". Needs Docker.
# On Git Bash for Windows, run with MSYS_NO_PATHCONV=1 and give <out dir> as a Windows path (C:/...).
OUT="$1"; shift; ARGS="${*:-all}"
[ -n "$OUT" ] || { echo "usage: $0 <out dir> [make args]"; exit 2; }
SRC="$(git rev-parse --show-toplevel)" || exit 2
rm -rf "$OUT"; mkdir -p "$OUT"
( cd "$SRC" && git ls-files -z | tar --null -T - -cf - ) | tar -xf - -C "$OUT"
docker run --rm -v "$OUT":/src -w /src ps2dev/ps2dev:v1.0 sh -c "apk add --no-cache build-base git zip >/dev/null 2>&1; make $ARGS" > "$OUT.log" 2>&1
rc=$?; echo "make $ARGS rc=$rc, log $OUT.log, $(grep -c warning "$OUT.log") warning lines"; ls -l "$OUT/bin" 2>/dev/null; exit $rc
