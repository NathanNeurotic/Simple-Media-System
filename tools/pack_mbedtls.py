#!/usr/bin/env python3
"""Make third_party/mbedtls-3.6.7-src.tar.gz from the official mbedTLS release.

SMS builds a TLS 1.2 client for Check for updates. Rather than add several hundred
upstream files to this repository, the part of mbedTLS the build needs is kept as one
archive that the Makefile unpacks into obj/. Nothing in it is modified: it is LICENSE,
everything under include/, every header under library/, and the library sources listed
in SOURCES, byte for byte from

  https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.7/mbedtls-3.6.7.tar.bz2

  python3 tools/pack_mbedtls.py mbedtls-3.6.7.tar.bz2            rewrite the archive
  python3 tools/pack_mbedtls.py mbedtls-3.6.7.tar.bz2 --check    compare with the committed one

The output is deterministic ( sorted names, fixed times and owners ), so --check passes
for anyone who downloads the same release.
"""
import gzip, hashlib, io, sys, tarfile
from pathlib import Path

RELEASE_SHA256 = "a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6"
PREFIX = "mbedtls-3.6.7/"
# Keep in step with MBEDTLS_SRCS in the Makefile ( tools/test_update_core.py checks its own copy ).
SOURCES = """aes asn1parse asn1write bignum bignum_core cipher cipher_wrap constant_time ctr_drbg
ecdh ecdsa ecp ecp_curves entropy gcm md oid pk pk_ecc pk_wrap pkparse platform platform_util rsa
rsa_alt_helpers sha256 sha512 ssl_ciphersuites ssl_client ssl_msg ssl_tls ssl_tls12_client x509
x509_crt""".split()
TARGET = Path(__file__).resolve().parents[1] / "third_party/mbedtls-3.6.7-src.tar.gz"


def wanted(name):
    if name == "LICENSE" or name.startswith("include/"):
        return True
    if name.startswith("library/") and name.count("/") == 1:
        stem, _, ext = name[len("library/"):].rpartition(".")
        return ext == "h" or (ext == "c" and stem in SOURCES)
    return False


def build(release):
    data = Path(release).read_bytes()
    digest = hashlib.sha256(data).hexdigest()
    assert digest == RELEASE_SHA256, "not the official 3.6.7 release archive: " + digest
    files = {}
    with tarfile.open(fileobj=io.BytesIO(data)) as tar:
        for member in tar:
            name = member.name[len(PREFIX):] if member.name.startswith(PREFIX) else None
            if name and member.isfile() and wanted(name):
                files[name] = tar.extractfile(member).read()
    assert sum(1 for n in files if n.endswith(".c")) == len(SOURCES), "a listed source is missing"
    raw = io.BytesIO()
    with tarfile.open(fileobj=raw, mode="w", format=tarfile.USTAR_FORMAT) as out:
        for name in sorted(files):
            info = tarfile.TarInfo("mbedtls/" + name)
            info.size, info.mode, info.mtime = len(files[name]), 0o644, 0
            out.addfile(info, io.BytesIO(files[name]))
    packed = io.BytesIO()
    with gzip.GzipFile(fileobj=packed, mode="wb", compresslevel=9, mtime=0, filename="") as gz:
        gz.write(raw.getvalue())
    return packed.getvalue(), len(files)


def main():
    args = [a for a in sys.argv[1:] if a != "--check"]
    if len(args) != 1:
        sys.exit(__doc__)
    packed, count = build(args[0])
    if "--check" in sys.argv:
        assert TARGET.read_bytes() == packed, "committed archive differs from the release"
        print("PASS: %s matches the official release ( %d files )" % (TARGET.name, count))
    else:
        TARGET.write_bytes(packed)
        print("wrote %s: %d files, %d bytes" % (TARGET.name, count, len(packed)))


if __name__ == "__main__":
    main()
