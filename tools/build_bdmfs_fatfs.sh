#!/bin/sh
# Rebuild bdmfs_fatfs.irx (USB, MX4SIO, HDD and i.LINK FAT/exFAT driver) with UTF-8 file names.
#
# ROOT CAUSE: the stock PS2SDK v2.0.0 module (byte-identical to the one SMS shipped up to
#   v3.0.0-rev2) builds FatFs with FF_LFN_UNICODE 0 and FF_CODE_PAGE 869 (Greek). Every
#   long name is converted UTF-16 -> CP869 for the API; a name holding a letter CP869
#   lacks (a-acute, e-acute, n-tilde, inverted ? and ! -- all of Spanish) cannot be
#   converted, so FatFs drops the long name: FAT32 lists the 8.3 alias ("01TRAT~1.AVI"),
#   exFAT lists "?". SMS's UTF-8 display path never saw those names.
#
# FIX: the same PS2SDK and FatFs sources with exactly one change, FF_LFN_UNICODE 2, so
#   the driver hands names to iomanX as UTF-8 and takes UTF-8 paths back on open.
#   Plain ASCII names come out byte-for-byte as before.
#
# Needs git, docker and python3. Run from anywhere:
#   sh tools/build_bdmfs_fatfs.sh           # UTF-8 build -> irx/bdmfs_fatfs.irx + .irx.xz
#   sh tools/build_bdmfs_fatfs.sh --stock   # unmodified build; must print STOCK_SHA256
# tools/fatfs_names_harness.c runs the same FatFs sources on the host to show the listing
# difference (8.3 aliases and "?" before, real names after).
# DOCKER_ARGS is passed to docker run as-is (e.g. a proxy for the apk step).
set -e
PS2SDK_TAG=v2.0.0
FATFS_COMMIT=18cc3d9e07473a6aa3d783a66224b243a7b4974c   # fjtrujy/FatFs iop-r0.16, the tip when v2.0.0 was built
IMAGE=ps2dev/ps2dev:v2.0.0
STOCK_SHA256=597b23addfdb35346544d742d9e4e67edadb3b064e7c03b9784fbf71cb9e37d6
UTF8_SHA256=e76412c235d9ac5636e202ea860dddab22b2e394c27e6428724b29bdb7f8de76   # checked by tools/test_bdmfs_fatfs_module.py

REPO="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
MODE=utf8
[ "$1" = "--stock" ] && MODE=stock

git clone -q --depth 1 -b "$PS2SDK_TAG" https://github.com/ps2dev/ps2sdk.git "$WORK/ps2sdk"
git clone -q -b iop-r0.16 https://github.com/fjtrujy/FatFs.git "$WORK/ps2sdk/common/external_deps/fatfs"
git -C "$WORK/ps2sdk/common/external_deps/fatfs" checkout -q "$FATFS_COMMIT"
FFCONF="$WORK/ps2sdk/common/external_deps/fatfs/source/include/ffconf.h"
if [ "$MODE" = utf8 ]; then
  sed -i 's/^#define FF_LFN_UNICODE\t0/#define FF_LFN_UNICODE\t2/' "$FFCONF"
  grep -q '^#define FF_LFN_UNICODE	2' "$FFCONF"
fi

docker run --rm $DOCKER_ARGS -v "$WORK/ps2sdk":/ps2sdksrc:ro -v "$WORK":/out "$IMAGE" sh -c '
  set -e
  apk add --no-cache build-base >/dev/null 2>&1     # host make + cc for the SDK build rules
  cp -r /ps2sdksrc /tmp/src; cd /tmp/src
  export PS2SDKSRC=/tmp/src PATH="/usr/local/ps2dev/iop/bin:/usr/local/ps2dev/ee/bin:$PATH"
  make -C iop/fs/bdmfs_fatfs >/dev/null
  cp iop/fs/bdmfs_fatfs/irx/bdmfs_fatfs.irx /out/bdmfs_fatfs.irx'

if [ "$MODE" = stock ]; then
  sha256sum "$WORK/bdmfs_fatfs.irx"
  echo "$STOCK_SHA256  expected"
  exit 0
fi

cp "$WORK/bdmfs_fatfs.irx" "$REPO/irx/bdmfs_fatfs.irx"
# Same XZ format as tools/compress_irx.py (LZMA2 only, CRC32), which SMS's embedded decoder reads.
python3 - "$REPO/irx/bdmfs_fatfs.irx" <<'PY'
import sys, lzma
irx = sys.argv[1]
b = open(irx, "rb").read()
c = lzma.compress(b, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32,
                  filters=[{"id": lzma.FILTER_LZMA2, "preset": 9, "dict_size": 1 << 20}])
open(irx + ".xz", "wb").write(c)
assert lzma.decompress(c, format=lzma.FORMAT_XZ) == b
print("bdmfs_fatfs.irx %d bytes, .xz %d bytes" % (len(b), len(c)))
PY
sha256sum "$REPO/irx/bdmfs_fatfs.irx"
