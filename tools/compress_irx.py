#!/usr/bin/env python3
"""Pre-compress the embedded IRX modules to XZ so the ELF ships them compressed.

The Makefile bin2c's irx/*.irx.xz (compressed) into byte arrays, and SMS_IOP.c
loads them via SifExecDecompModuleBuffer (which lzma2_uncompress's then
SifExecModuleBuffer's). The EE build image has no xz/python, so this runs
separately (via the sms-ipu-tools image) and its .xz output is committed.

Format MUST match SMS's embedded decoder (lzma2.c + xz_dec_*, XZ_SINGLE):
standard .xz, LZMA2 filter only (no BCJ/delta), CRC32 check (xz_crc64 is NOT
linked). This is byte-for-byte the same format SMS's existing SMS_Data modules
use, so it is known to decode on real hardware.

Every irx/*.irx found is recompressed over its .xz. smbman.irx.xz is written by
iop/smbman-sms/prepare_module.py instead, so no raw irx/smbman.irx is kept here.

  docker run --rm -v <repo>:/work sms-ipu-tools python3 /work/SMS-v1/tools/compress_irx.py
"""
import glob, os, lzma

IRX_DIR = os.environ.get("IRX_DIR", "/work/SMS-v1/irx")
FILTERS = [{"id": lzma.FILTER_LZMA2, "preset": 9, "dict_size": 1 << 20}]

def main():
    tot = comp = 0
    for f in sorted(glob.glob(os.path.join(IRX_DIR, "*.irx"))):
        b = open(f, "rb").read()
        c = lzma.compress(b, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32, filters=FILTERS)
        open(f + ".xz", "wb").write(c)
        tot += len(b); comp += len(c)
        print("%-22s %7d -> %7d" % (os.path.basename(f), len(b), len(c)))
    if tot:
        print("TOTAL %d -> %d  (-%d%%)" % (tot, comp, 100 * (tot - comp) // tot))

if __name__ == "__main__":
    main()
