# SMS regression follow-up ? 2026-10-26

## Build identity and reporter evidence

PR #30 remains unmerged. The rolling release is master
`7b172d0c339f96b80c4eb22249c7f67c99180389`.
However, the reporters explicitly tested artifact commit
`aa801fec23823ab9f9c343fab7b8157fecd1dea7`, GitHub's test merge of master
and PR head `64b1265001924f34f4f289cd721eb94651578e05`.
The artifact is attached to build run 40293002588. Its ELF was downloaded
and inspected. These reports therefore **do include Kimi's changes**.

- #20 comment 6416451418: green coloration persists.
- #23 comment 6416426051: colors are worse after conversion.
- #24 comment 6416206912: high bitrate still stutters on USB and MX4SIO.

Earlier comments directing reporters to rolling were incorrect while the PR
was unmerged. Use an exact Actions run and artifact, with its build-info.json.
Do not interpret a green CI result as gameplay or picture acceptance.

## Changes and remaining uncertainty

### Colors (#20, #23)

Removed b943b12's unconditional BT.601/range transform, restoring the converter's
previous scaling color handling. Reporter feedback contradicts calling that
change a confirmed fix. This is a rollback of an unsuccessful candidate,
not proof that the underlying color problem is fixed.

The MPEG-1/2 decoder discards sequence color metadata (libmpeg.c). Nevertheless,
that alone does not explain the abrupt saturated red/cyan patches in #23.
The original 2.9 imported decoder and current decoder have essentially identical
MPEG logic; the assembly differs in compatibility directives. Inspection of the
exact aa801 ELF's _MPEG_Set16 patch sequence did not establish an address-patching
fault. No additional decoder or PS2 color coefficients are changed here.
The maintainer has no local reporter/source sample available.

Required comparison: the same converted file on 2.9rev4 and this PR, retaining
the PS2 output mode and color-depth settings; also decode the file on the host.
This separates encoder output from a player/display regression. Changing both
converter and player at once does not isolate the cause.

### Stutter (#24)

The BDM path fills synchronously using 4 KiB RPCs. Increasing a buffer from
48 to 96 blocks increases one refill from 48 to 96 blocking calls; it does not
create read-ahead or improve sustained device bandwidth. Reverted that size
increase, retaining the original 48-block allocation.

The new candidate caps a synchronous BDM refill at eight 4 KiB calls (32 KiB),
including initial stream fill, returning data to demux/decode sooner. The
MX4SIO/MMCE per-call limit stays 4 KiB, and UDPFS keeps its separate sizing.
This bounds the batch size, not elapsed I/O time or sustained throughput.
No reader thread, shared fileXio async mode, or protocol changes are introduced.
Hardware testing is still required to establish any stutter improvement.

### Freeze/probe (#29)

A read can fail without advancing m_CurPos even when m_CurPos < m_Size.
The start-code scanner now stops on that condition. The tail PTS scan also
stops when an iteration makes no progress. This closes an actual unbounded
retry path; it has not been identified as the reporter's precise freeze.

### Review and artifact delivery

fc2a02d addressed all eight CodeRabbit inline findings, with an individual reply
per finding. Allocation is atomic across both STIO buffers; failed allocation
preserves the context, and playback restores ordinary reads on stream failure.
The follow-up review found the same unsafe resize pattern in CDDA_Stream; a
shared transactional allocator now covers both paths. Synchronous fileXio
allocates only the single buffer it uses, while CDDA and asynchronous fio keep
two. Fault injection covers both buffer counts and every allocation failure.

The old workflow zipped files before DIAG existed, then uploaded only the
standard ELF. Packaging now happens after both builds and uploads the complete
dist folder. The manifest records source SHA (including a PR merge SHA), head
SHA, event, run URL, and SHA-256 for all three ELFs.

## Validation boundaries

- Host allocation-failure tests exercise the actual source allocation block.
- Host refill tests exercise the actual STIO_Fill and start-code scanner with
  stubbed device I/O: byte continuity, 32 KiB batch cap, short EOF, preserved
  UDPFS sizing, and no-progress scanner termination.
- Converter tests include a real FFmpeg subtitle conversion with special path
  characters, missing alternate audio rejection, and Qt picker/clear behavior.
- PS2 builds use the existing ps2dev v1.0 GCC image: standard, BDM=0, and DIAG.
- No physical-console or emulator acceptance is claimed.
