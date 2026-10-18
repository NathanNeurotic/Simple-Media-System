#!/bin/sh
# Run inside the project's ps2dev/ps2dev:v1.0 container, from any directory.
set -eu
cd "$(dirname "$0")/../iop/smbman-sms"
python3 tests/test_cp1251.py
python3 tests/test_smb_regressions.py
make clean all
python3 prepare_module.py "$@"
