# Building from Source

Build the EE application with the repository's existing legacy GCC toolchain. IOP module rebuilding is a separate task; a normal player build uses the committed compressed modules.

You do not need a compiler to run SMS. Use [the prebuilt downloads](https://nathanneurotic.github.io/Simple-Media-System/releases.html) for ordinary installation.

## Prerequisites

- Docker running on your host and a source checkout, including `irx/`, artwork and fonts.
- The known-working `ps2dev/ps2dev:v1.0` image for the application, using EE GCC 3.2.3. Do not silently replace it with a current SDK or LLVM build.
- A separate checkout/build directory when comparing variants. Changing make flags without cleaning can reuse objects built for a different variant.

## The pinned application toolchain

The old Alpine image can compile SMS, but its musl runtime cannot run modern GitHub Actions JavaScript steps. CI checks out and uploads on the host and runs only make inside Docker. A tag selects the expected toolchain; verify actual output hashes rather than assuming a tag alone guarantees every future build is identical.

## Standard and packed builds

### Linux / macOS / Git Bash

```
git clone https://github.com/NathanNeurotic/Simple-Media-System.git
cd Simple-Media-System
docker run --rm -v "$PWD":/src -w /src ps2dev/ps2dev:v1.0 \
  sh -c 'apk add --no-cache build-base git zip && make all pack'
```

### Windows PowerShell

```
docker run --rm -v "${PWD}:/src" -w /src ps2dev/ps2dev:v1.0 sh -c 'apk add --no-cache build-base git zip && make all pack'
```

Run the PowerShell command from the repository root. Shell quoting and line continuation differ from POSIX/cmd.exe; do not copy the backslash-continued command into cmd.exe unchanged.

Outputs are `bin/SMS.elf` and `bin/SMS-packed.elf`. BDM defaults to enabled; that is the distributed full-device player.

## Diagnostic and compatibility variants

In a disposable build checkout, clean between variants, then use the same container setup with:

```
make clean
make SMS_DIAG=1 all
# Save/copy bin/SMS.elf before cleaning again.
make clean
make BDM=0 all
```

`SMS_DIAG=1` adds on-screen diagnostic labels. `BDM=0` verifies the legacy build still links; it is not the standard rolling download. Cleaning removes that checkout's build outputs, so keep any binaries or symbols you need before running it.

## Embedded IOP modules

The Makefile converts committed `irx/*.irx.xz` into embedded arrays. SMS decompresses and starts them on the IOP. Normal installation/building does not require a separate IRX build or copying loose modules next to the player.

The modules include iomanX/fileXio, BDM/FAT/exFAT, USB, MX4SIO, ATA, i.LINK, MMCE, card/pad support and network stacks. Consult the [actual Makefile](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/Makefile) for the selected objects; the enabled stack depends on the build flags and runtime choices.

## Rebuilding the MX4SIO driver

The embedded MX4SIO module carries [a local driver patch](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/patches/mx4sio_bd-sms.patch). Preserve its watchdog, bounded SIO2 waits, batch limit and partial-read progress handling. [Patch notes](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/patches/README.md) describe the rebuild; [the historical investigation](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/MX4SIO_SOLUTION.md) records the original reproducer and earlier read strategy.

An IOP module's compiler/SDK requirements can differ from the EE application. In particular, the UDPFS module build uses its own script/toolchain and must retain SMS's dev9 import ordinals. See [IOP and network development](https://nathanneurotic.github.io/Simple-Media-System/developers.html#drivers). Do not replace compressed modules with arbitrary SDK builds.

## Build and publication workflow

| Trigger | Result |
| --- | --- |
| Push / PR | Host regressions, ABI checks, standard/packed/DIAG and separate legacy builds; downloadable per-run artifacts. |
| Successful `master` push | Updates the rolling player assets. Separate converter jobs update Windows, Win7 and Linux assets on the same release. |
| `v*` tag | Versioned release publication. |
| Master test-media workflow | Regenerates synthetic media. This does not execute console playback. |
| Documentation changes | Checks shared generation, local links/anchors, search targets and browser-script syntax. Pages serves `master:/docs`. |

See [host test prerequisites](https://nathanneurotic.github.io/Simple-Media-System/developers.html#build-tests) and the [workflow sources](https://github.com/NathanNeurotic/Simple-Media-System/tree/master/.github/workflows). A successful compile is not a device test. Keep the build manifest, symbols and exact media when reporting results.

## Next steps

[Architecture and contributing](https://nathanneurotic.github.io/Simple-Media-System/developers.html) · [Known issues and acceptance](https://nathanneurotic.github.io/Simple-Media-System/known-issues.html) · [Build identity and downloads](https://nathanneurotic.github.io/Simple-Media-System/releases.html).

---
[Searchable manual](https://nathanneurotic.github.io/Simple-Media-System/building.html) · [Edit the maintained source](https://github.com/NathanNeurotic/Simple-Media-System/blob/master/docs-site/content/building.html)
