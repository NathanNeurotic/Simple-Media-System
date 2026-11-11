#!/bin/bash
# Run the CI host tests on Linux or WSL Debian from the repository root.
# usage: docs/investigations/2026-11-11-handoff/scripts/host-tests.sh [tools/test_x.py ...]
# With no arguments it runs every test listed in .github/workflows/compilation.yml.
# setarch -R disables ASLR, which the ThreadSanitizer test needs.
cd "$(git rev-parse --show-toplevel)" || exit 2
tests="$*"; [ -z "$tests" ] && tests=$(grep -oE 'python3 tools/test_[a-z0-9_]+\.py' .github/workflows/compilation.yml | awk '{print $2}' | sort -u)
pass=0; fail=0; failed=""
for t in $tests; do
  if out=$(timeout 900 setarch -R python3 "$t" 2>&1); then pass=$((pass+1)); else fail=$((fail+1)); failed="$failed $t"; echo "=== FAIL $t"; echo "$out" | tail -25; fi
done
echo "PASS=$pass FAIL=$fail"; [ -n "$failed" ] && echo "FAILED:$failed"
exit $fail
