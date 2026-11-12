"""The embedded bdmfs_fatfs must be the UTF-8 file-name build from tools/build_bdmfs_fatfs.sh.

The stock PS2SDK module converts long names to the Greek codepage 869 and silently falls back
to the 8.3 alias (FAT32) or "?" (exFAT) for any Spanish accent, so an SDK refresh that drops
the rebuilt module brings the bug straight back.
"""
import hashlib, lzma, re
from pathlib import Path

root = Path(__file__).resolve().parents[1]
pin = re.search(r'^UTF8_SHA256=([0-9a-f]{64})', (root / 'tools/build_bdmfs_fatfs.sh').read_text(), re.M).group(1)
raw = (root / 'irx/bdmfs_fatfs.irx').read_bytes()
xz = lzma.decompress((root / 'irx/bdmfs_fatfs.irx.xz').read_bytes(), format=lzma.FORMAT_XZ)
assert xz == raw, 'irx/bdmfs_fatfs.irx.xz does not hold irx/bdmfs_fatfs.irx'
assert hashlib.sha256(raw).hexdigest() == pin, 'irx/bdmfs_fatfs.irx is not the UTF-8 build'
print('bdmfs_fatfs: UTF-8 file-name build embedded')
