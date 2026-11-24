#!/bin/sh
# Rebuilds irx/smsnet.irx.xz from iop/smsnet. Run inside the project's ps2dev/ps2dev:v1.0
# container, from any directory. With --check, only verifies the committed module.
set -eu
cd "$(dirname "$0")/../iop/smsnet"
if [ "${1:-}" != "--check" ]; then make clean all; fi
python3 prepare_module.py "$@"
