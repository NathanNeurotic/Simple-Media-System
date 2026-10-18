# SMS smbman source and rebuild

This directory vendors the SMB client from
[ps2dev/ps2sdk](https://github.com/ps2dev/ps2sdk/tree/329e2bb43287e16af16c4dde1c614e2662c24468/iop/network/smbman),
commit `329e2bb43287e16af16c4dde1c614e2662c24468`:
`iop/network/smbman/src/*` and `common/include/ps2smb.h`.
Original copyright and license notices are retained. The smbman/authentication
sources specify AFL-3.0; the SDK import scaffolding/header specifies AFL-2.0.
Both license texts are included. New SMS conversion, tests and packaging code
is available under AFL-3.0. Changes are identified below; no ownership of the
upstream work is claimed.

The embedded DES implementation retains Eric Young's separate copyright,
redistribution conditions and disclaimer in `src/des.c`; it is not relicensed
under AFL. This product includes software developed by Eric Young
(eay@mincom.oz.au). MD4 retains the Raif S. Naffah attribution and
jimmikaelkael adaptation notice in `src/md4.c`. These unchanged cryptographic
files come from the pinned ps2sdk distribution, not newly authored code.
The release ZIP includes both AFL license texts, this provenance document and
`NOTICE-DES.txt`. Keep these materials with redistributed binary builds.

## SMS-specific changes

- Bidirectional Windows-1251/UTF-16LE mapping, with explicit byte capacities,
  unsigned byte decoding, and no unaligned UTF-16 loads.
- Private `SMB_DEVCTL_SMS_CP1251` selects strict Windows-1251 only when SMS's
  existing display charset is WinCyrillic. The default keeps the original
  byte-to-Unicode writes and ASCII-only reads for other display pages. No UI/font/language files
  or saved configuration formats change.
- Malformed UTF-16, undefined CP1251 byte 0x98 and unrepresentable Unicode
  names return an error in CP1251 mode, not a lossy `?` pathname. This is
  deliberately fail-closed: directory enumeration skips such entries while
  retaining the search ID and advancing; an outgoing invalid name fails.
  This is not general Unicode filename support.
- Bound SMB string writes and directory-reply filename reads; reject oversized
  complete packets and malformed directory transaction bounds. These checks
  are not a complete security audit of the legacy SMB1 client.
- Review follow-up: validate READ_ANDX offsets/lengths against the request and
  NBSS packet before touching the caller's buffer. Drain padding/trailers in
  128-byte scratch chunks, including rejected/short replies; close the socket
  if a receive fails partway and its boundary cannot be recovered.
- Parse RAP's fixed 20-byte share records using bounded little-endian reads,
  EntriesRead (not TotalAvailable), converter-relative comment offsets, and
  terminated strings within the transaction data. Fragmented RAP transactions
  are rejected, not reassembled.
- Always free local directory slots, even before FIND_FIRST or after a failed
  listing/disconnection. Close-all clears local ownership and does not send
  SMB_CLOSE for directory search IDs.
- Reject overlong joined paths, and reserve room for a directory search mask.
  Existing limits remain: names up to 255 local bytes, joined paths below 1024
  bytes, and directory search paths at most 253 bytes before adding `\\*`.
  Long path tests exercise the helper, not a promise of unlimited PS2 navigation.
- Preserve raw tree-connect NTSTATUS errors for SMS's existing error UI,
  including the authentication retry. This behavior was already present in
  SMS's binary module (upstream PR #14); it is retained in the available source.
- Pin ps2ip imports to SMS's existing 1.3 provider and inet_addr ordinal 24;
  close the socket without importing unavailable shutdown ordinal 46.
- Provide the newer iomanX return-value helper with old SDK headers and use
  the legacy v1.0 IOP linker directly. The import ABI is checked against the
  committed SMS module before packing.

## Rebuild

From the SMS repository root on a Docker host:

```sh
docker run --rm -v "$PWD":/src -w /src \
  ps2dev/ps2dev:v1.0@sha256:da02eb0870afcfdb7198195de40d04cd121bd893668c7342fb84b3c4d273e9d1 \
  sh -c 'apk add --no-cache build-base python3 && sh tools/build-smbman.sh && make BDM=1 all'
```

`build-smbman.sh` runs native C tests, builds the IRX, checks every import
library/version/ordinal against `irx/smbman.irx.xz`, then packs that exact
rebuild as XZ (LZMA2, 1 MiB dictionary, CRC32). The main SMS build embeds it.
Use `sh tools/build-smbman.sh --check` instead to assert that a rebuild matches
the committed compressed binary without writing it. Native tests can also run
with `python3 iop/smbman-sms/tests/test_cp1251.py` on a host with `cc` and Python.
`tests/test_smb_regressions.py` compiles exact transport, read, RAP and close
function bodies from the driver with mocked sockets/IOP calls. It runs with
both signedness modes; set `SMS_TEST_SANITIZERS=1` for ASan/UBSan. The CI
workflow runs sanitizer tests on the host and a source/binary check inside
the pinned image. A manual `rebuild=true` workflow run uploads rebuilt IRX
and BDM=1 ELF artifacts without committing anything automatically.

The compiler/linker image is digest-pinned. Alpine dependency repositories and
host Python/liblzma versions are not a complete hermetic environment; use the
same container to compare compressed bytes. CI should use `--check`.

## Hardware results and remaining checks

The reporter confirmed the earlier V2 implementation on a real PS2 Slim with
Windows 11 SMB1 on TCP 445: preset share access without NetShareEnum, Cyrillic
directory display/navigation, Cyrillic file listing/opening, and independent
OPL SMB operation. No separate PC server program was required.

After the PR hardening (bounds, opt-in charset control, strict errors), the
reporter also tested the final PR ELF and confirmed basic operation on that
same setup. This is not confirmation of every extended test below.

The subsequent READ_ANDX/RAP/handle-lifetime review fixes change the module
again. Their rebuilt ELF has automated test/build coverage but still needs
a fresh PS2 hardware pass; the previous confirmation predates those fixes.

Retest with WinCyrillic and a nonempty Share; navigate ASCII, Cyrillic, Ё/ё,
mixed and nested names and open a media file. Then test an empty Share on a
server supporting RAP/NetShareEnum, invalid share, denied access, reconnect,
changing charset, and a name outside CP1251. Check long playback and seeking,
an unrepresentable name between two valid directory entries, large directories
and path boundaries. Other servers, authentication policies,
SMB dialects and PS2 models are not validated by the reporter's test.

SMB1 remains required; this does not implement SMB2/3 or recommend changing
Windows security settings. UDPFS, OPL configuration and Russian localization
are outside this patch.
