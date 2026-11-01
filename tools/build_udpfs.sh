#!/bin/sh
# Build the UDPFS network-drive stack (smap + ministack + udpfs_ioman) for SMS.
#
# WHAT THIS IS
#   UDPFS ( "UDP File System", protocol UDPRDMA, port 0xF5F6 / service 0xF5F5 ) serves a
#   PC *folder* over UDP; SMS mounts it as the iomanX device "udpfs:" and browses/streams
#   it like any other filesystem. Source: ps2max32/neutrino iop/{smap,ministack,udpfs},
#   vendored in SMS-v1/iop/SMSUdpfs ( AFL v3.0 -- see that tree's ATTRIBUTION ).
#   NOT to be confused with UDPBD ( legacy udpbd, port 0xBDBD ), a *block device* needing
#   a disk image. UDPBD is a subset of UDPFS; SMS no longer ships it.
#
# THE TRAP THIS SCRIPT EXISTS TO AVOID ( same class as build_ata_bd_dev9v11.sh )
#   IOP loadcore binds a client to a provider by library NAME + MAJOR version only, then
#   resolves every stub BY ORDINAL. neutrino's smap is written against dev9 *v1.9*, but
#   SMS embeds a dev9 *v1.1* clone ( SMS-v1/iop/SMSDev9 ):
#       v1.1 (SMS):  11 = dev9RegisterPreDmaCb   12 = dev9RegisterPostDmaCb   (no 13)
#       v1.9 (sdk):  11 = Dev9RegisterPowerOffHandler  12 = PreDmaCb  13 = PostDmaCb
#   Built stock, smap's PostDmaCb lands on ordinal 13 ( does not exist ) and PreDmaCb on
#   SMS's PostDmaCb -> the SMAP DMA callbacks are never armed -> the driver loads clean
#   and then never transfers a byte. This is EXACTLY why the old smap_udpbd.irx never
#   worked on any server, and it is invisible: not a load error, nothing reports it.
#   Fix: SMS's own dev9.h ( v1.1 ABI ) is dropped into smap/include/, and neutrino's
#   Rules.make puts -Iinclude FIRST, so it shadows ps2sdk's v1.9 header. smap imports
#   exactly the 7 symbols SMS v1.1 exports {4,5,7,8,9,11,12} -- no source edit needed,
#   and no PowerOffHandler import to strip ( unlike ata_bd ).
#   The gate at the bottom asserts the binding. DO NOT REMOVE IT.
#
# Requires the ps2dev/ps2dev:v2.0.0 image (IOP toolchain). Run from anywhere:
#   sh SMS-v1/tools/build_udpfs.sh
#
# Output: SMS-v1/irx/udpfs_{smap,ministack,ioman}.irx + matching .xz blobs.
set -e
REPO="$(cd "$(dirname "$0")/../.." && pwd)"      # repo root (parent of SMS-v1/)
SRC="$REPO/SMS-v1/iop/SMSUdpfs"
OUT="$REPO/SMS-v1/irx"

# Docker on Windows needs native C:/... paths; Git Bash reports /c/... and would also
# mangle the ":/src" suffix without MSYS_NO_PATHCONV.
DREPO="$REPO"
case "$(uname -s)" in
  MINGW*|MSYS*|CYGWIN*) DREPO="$(cd "$REPO" && pwd -W)"; export MSYS_NO_PATHCONV=1 ;;
esac
DSRC="$DREPO/SMS-v1/iop/SMSUdpfs"
DOUT="$DREPO/SMS-v1/irx"

docker run --rm -v "$DSRC":/src -v "$DOUT":/out ps2dev/ps2dev:v2.0.0 sh -c '
  set -e
  apk add --no-cache build-base >/dev/null 2>&1     # image ships no make/host cc
  cp -r /src /tmp/udpfs && cd /tmp/udpfs
  export PATH="/usr/local/ps2dev/iop/bin:/usr/local/ps2dev/ee/bin:$PATH"

  make -C smap      clean all
  make -C ministack clean all
  make -C udpfs     clean all UDPFS_IOMAN=1

  # Renamed on the way out to mirror wLaunchELF-R3Z and to keep them visually distinct
  # from SMS own SMSMAP/PS2SMAP blob. The registered LIBRARY names (smap / mstack) are
  # baked into the modules and are unaffected by the filename.
  cp smap/irx/smap.irx            /out/udpfs_smap.irx
  cp ministack/irx/ministack.irx  /out/udpfs_ministack.irx
  cp udpfs/irx/udpfs_ioman.irx    /out/udpfs_ioman.irx
  cd /out && for f in udpfs_smap.irx udpfs_ministack.irx udpfs_ioman.irx; do
    echo "built $f ($(wc -c < $f) bytes)"
  done
'

# ---- ABI GATE: a v1.9 binding here is a silent, total failure. Fail the build. ----
python3 "$DREPO/SMS-v1/tools/irx_imports.py" --assert dev9=1.1 "$DOUT/udpfs_smap.irx"

# ---- XZ blobs (CRC32 / LZMA2, matching SMS's embedded xz decoder) ----
python3 - "$DOUT" <<'PY'
import sys, os, lzma
out = sys.argv[1]
for name in ("udpfs_smap.irx", "udpfs_ministack.irx", "udpfs_ioman.irx"):
    p = os.path.join(out, name)
    b = open(p, "rb").read()
    c = lzma.compress(b, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32,
                      filters=[{"id": lzma.FILTER_LZMA2, "preset": 9, "dict_size": 1 << 20}])
    open(p + ".xz", "wb").write(c)
    assert lzma.decompress(c, format=lzma.FORMAT_XZ) == b
    print("%s.xz %d bytes (was %d, round-trip OK)" % (name, len(c), len(b)))
PY
