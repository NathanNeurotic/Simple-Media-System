# SMS extraction

## Verdicts
| ID | Title | Effort | Risk | Verdict |
|---|---|---|---|---|
| RE-SMS-001 | Unknown extension makes the container probe chain call `s_CCreator[-1]` | S | Low | REFUTED |
| RE-SMS-002 | MPEG-4 sprite trajectory: bitstream warp-point count overflows a 4-entry stack array | Trivial | Negligible | VERIFIED |
| RE-SMS-003 | ip_reass uses the byte-swapped total length to allocate and copy the reassembled packet | Small | Low | VERIFIED |
| RE-SMS-004 | FAT long-filename sequence number trusted into a 128-byte name buffer (SMSUMS) | Small | Low | VERIFIED |
| RE-SMS-005 | DTS: stream-controlled `prim_channels` (up to 8) overflows state and stack arrays sized for 5 | Trivial | None identified for valid streams | VERIFIED-DRIFT |
| RE-SMS-006 | `SMS_LZMA2Decompress` decompresses into the buffer it just freed (NULL) on size mismatch | Trivial | Low | VERIFIED |
| RE-SMS-007 | `SifExecDecompModuleBuffer` executes the output buffer on *any* XZ decode error | Trivial | Low | VERIFIED |
| RE-SMS-008 | ISO9660 filename length trusted past a 129-byte TOC buffer (SMSCDVD) | Small | Low | VERIFIED |
| RE-SMS-009 | get_socket() indexes the fixed socket table with no bounds check | Trivial | Low | VERIFIED-DRIFT |
| RE-SMS-010 | DTS: header fields and a cumulative sum index fixed tables with no range checks (downmix / bitalloc / LFE / scale factors) | Small | Low | VERIFIED-DRIFT |
| RE-SMS-011 | DTS SPDIF passthrough: header frame size can make `memset` length negative (huge) and overrun the output buffer | Trivial | None for valid streams | VERIFIED-DRIFT |
| RE-SMS-012 | DXSB subtitle decoder trusts header dimensions against a fixed scratchpad row buffer; packet size never checked | Small | Low | VERIFIED |
| RE-SMS-013 | Stream/codec/user-data allocations are dereferenced without a NULL check in nine container creators | M | Low | VERIFIED-DRIFT |
| RE-SMS-014 | Raw config blob is trusted as array indices with almost no validation | Small | Low | VERIFIED-DRIFT |
| RE-SMS-015 | `sprintf` into the 64-byte `g_SaveDiag` with a 128-byte path overflows it | Small | Low | VERIFIED |
| RE-SMS-016 | ip_reass trusts IPH_LEN over the actual pbuf chain, and its bitmap index goes one byte out of bounds at exactly 5760 bytes | Small | Low | VERIFIED |
| RE-SMS-017 | UDF descriptor lengths trusted: allocation-descriptor offset and name decode unbounded (SMSCDVD) | Small | Low | WEAKENED |
| RE-SMS-018 | Path component copied unbounded into a 257-byte stack buffer (SMSUMS) | Small | Low | VERIFIED |
| RE-SMS-019 | `UmsWarmUp` loops over a device-supplied LUN count past a 16-entry name table (SMSUMS) | Small | Low | VERIFIED |
| RE-SMS-020 | USB device geometry (sector size / LBA) trusted unchecked, including divide-by-zero (SMSUMS) | Small | Low | VERIFIED |
| RE-SMS-021 | `TranslateUTF8` can write `bufSize + 1` bytes into the caller's buffer | Trivial | Low | VERIFIED |
| RE-SMS-022 | AAC: sample-rate index from ADTS/ASC (up to 15) indexes 12-entry window tables unchecked | Trivial | None | WEAKENED |
| RE-SMS-023 | AAC: DRC `excluded_channels` loop writes past fixed 64-entry masks, driven by stream bits | Trivial | None | VERIFIED-DRIFT |
| RE-SMS-024 | WMA: version-1 exponent bands can contain zero-width entries, and `decode_exp_vlc` underflows on a zero entry into a runaway write | Small | Low | VERIFIED |
| RE-SMS-025 | getsockopt/setsockopt read and write TCP PCB fields on UDP sockets and on NULL PCBs | Small | Low | WEAKENED |
| RE-SMS-026 | accept_function() registers callbacks on the new PCB before allocating the netconn's mbox/sem, then frees the netconn on failure, leaving the PCB pointing at freed memory | Small | Low | VERIFIED |
| RE-SMS-027 | tcp_process() SYN_SENT branch dereferences pcb->unacked without a NULL check | Trivial | Low | VERIFIED |
| RE-SMS-028 | ASF packet sync divides by a file-controlled `packet_size` that is never validated non-zero | S | Low | VERIFIED |
| RE-SMS-029 | AVI ODML super-index loading recurses on file-controlled offsets with no depth bound | S | Low | VERIFIED |
| RE-SMS-030 | AAC: codebook-11 escape values can index the 8,192-entry inverse-quant table out of bounds in the `vquant` asm | Small | Low | VERIFIED |
| RE-SMS-031 | AAC: the constant 16428320 (0xFAAD20) is wired into the decode path — one reachable sample overwrite, two dead remnants | Trivial | None | VERIFIED |
| RE-SMS-032 | AC3/DTS: two-entry float-to-int function-pointer table indexed by an unvalidated channel count in both decoders | Trivial | None for any stream the player can currently play | VERIFIED |
| RE-SMS-033 | MP123: granule gain pointer + scalefactor index can walk past the 378-entry `s_GainPow2` table | Small | Low | VERIFIED-DRIFT |
| RE-SMS-034 | MPEG picture allocation path assumes success: unchecked callocs and a nullable picture lookup dereferenced unconditionally | Small | Low | VERIFIED |
| RE-SMS-035 | VLC/RL table construction and the DivX bitstream stash use allocation results without NULL checks | Small | Low | VERIFIED |
| RE-SMS-036 | List allocation contract is inconsistent: `Push`/`PushBackBuf` unchecked, 18 call sites dereference the result | Medium | Low | VERIFIED-DRIFT |
| RE-SMS-037 | History load: unbounded, unchecked, and triplicated record loop | Medium | Medium | VERIFIED |
| RE-SMS-038 | Subtitle loaders: unchecked node allocation and unbounded node count from the subtitle file | Small–Medium | Low | VERIFIED |
| RE-SMS-039 | Object constructors and menu builders dereference `calloc`/`malloc` results without a check | Medium | Low–Medium | VERIFIED |
| RE-SMS-040 | Directory-tree scan/walk/destroy recurse to filesystem depth, and tree init is unchecked | Medium | Medium | VERIFIED |
| RE-SMS-041 | lwip_close() writes errno through a NULL socket pointer on any bad descriptor | Trivial | Low | VERIFIED |
| RE-SMS-042 | do_bind() uses udp_new()/tcp_new() results without the NULL check its sibling functions have | Trivial | Low | VERIFIED |
| RE-SMS-043 | icmp_dest_unreach() dereferences its pbuf_alloc() result unchecked and over-reads the offending packet | Trivial | Low | VERIFIED |
| RE-SMS-044 | ip_frag() dereferences both of its pbuf_alloc() results unchecked | Trivial | Low | VERIFIED |
| RE-SMS-045 | lwip_accept() dereferences the accepted conn, addr and addrlen without NULL checks | Small | Low | VERIFIED |
| RE-SMS-046 | netconn_recv() dereferences the PCB and the received buffer without the NULL checks its own TCP branch demonstrates | Trivial | Low | WEAKENED |
| RE-SMS-047 | lwip_recvfrom() dereferences netbuf_fromaddr() which is NULL for every TCP buffer | Trivial | Low | VERIFIED |
| RE-SMS-048 | netbuf_ref() dereferences its pbuf_alloc() result unchecked | Trivial | Low | VERIFIED |
| RE-SMS-049 | pbuf_header() can underflow len/tot_len, and its main wire-facing caller strips 14 bytes without any length check | Small | Low | VERIFIED |
| RE-SMS-050 | UDPFS `_request` callers never check the reply actually arrived in full (SMSUdpfs) | Small | Low | VERIFIED |
| RE-SMS-051 | smbman-sms: two query replies parsed with none of the validation its sibling functions apply | Small | Low | VERIFIED |
| RE-SMS-052 | smbman-sms `smb_Echo` copies an EE-supplied length into the SMB buffer unchecked | Small | Low | VERIFIED |
| RE-SMS-053 | `lzma2_get_uncompressed_size` walks the XZ index with no bounds at all | Small | Low | VERIFIED |
| RE-SMS-054 | Unbounded whole-file loads sized by the file itself (language, font, asset copy, IRX module) | Medium | Medium for `_copy_file_to_mc`; Low for the caps | VERIFIED |
| RE-SMS-055 | `SMSContainer_DefReadPacket` dereferences a possibly-NULL packet, computes a clamp it never uses, and unallocates from stream 0 regardless of index | S | Low | VERIFIED |
| RE-SMS-056 | ASF fragment reassembly assigns `realloc`'s result and the new capacity before checking either | S | Low | VERIFIED |
| RE-SMS-057 | `SMSContainer_SetName` scans backwards for a path separator with no lower bound | S | Low | VERIFIED |
| RE-SMS-058 | MOV `mdhd` handler dereferences the stream that the M4A `hdlr` handler may already have freed | S | Low | VERIFIED |
| RE-SMS-059 | FLAC: fixed-predictor subframe reads up to 4 samples *before* the channel buffer for predictor orders 0–3 | Trivial | None | VERIFIED |
| RE-SMS-060 | WMA: superframe loop trusts container `block_align` (0 → hang; partial final frame → over-read) | Small | Low | WEAKENED |
| RE-SMS-061 | AC3: `_ac3_bit_allocate` can index `s_LATab` one past its 256 entries | Trivial | None | WEAKENED |
| RE-SMS-062 | CDDA `_read_string` does `strlen` on an unterminated stack buffer filled from disc data | Small | Low | VERIFIED |
| RE-SMS-063 | Fixed-buffer `strcpy` of paths and names that other code allows to be longer | Small | Low | VERIFIED |
| RE-SMS-064 | IOP init failures masked by unconditional success returns (family, all modules) | Medium | Low | VERIFIED |
| RE-SMS-065 | mem_free() never checks the block was actually in use, so a double free silently corrupts the heap free-list | Trivial | Low | VERIFIED |
| RE-SMS-066 | ps2ip_getconfig() strcpy()s an unbounded name into a 4-byte field; netif_find() reads name[2] on any string | Trivial | Low | VERIFIED |
| RE-SMS-067 | err_tcp() is the only PCB callback that dereferences its netconn argument unchecked | Trivial | Low | VERIFIED |
| RE-SMS-068 | ip_output_if() keeps its working IP-header pointer in a function-static variable shared by all callers | Trivial | Low | VERIFIED |
| RE-SMS-069 | `SMS_Realloc` publishes the new size even when `realloc` fails; AVI index loaders write through the result unchecked | Small | Low | VERIFIED |
| RE-SMS-070 | SMSMAP TX gather bounded by the FIFO budget, not by the gather buffer (INFERRED) | Trivial | Low | WEAKENED |
| RE-SMS-071 | SMSDev9 exported callback registration indexes fixed arrays unchecked (INFERRED) | Trivial | Low | WEAKENED |
| RE-SMS-072 | do_connect() discards tcp_connect()/udp_connect() results; a synchronous TCP failure is never posted back, so netconn_connect() blocks | Small | Low | VERIFIED |
| RE-SMS-073 | Client netconns never get a semaphore, but netconn_write() waits on it | Small | Medium | VERIFIED |
| RE-SMS-074 | err_to_errno() maps every non-zero lwIP error to EIO | Trivial | Low | VERIFIED |
| RE-SMS-075 | tcpip_apimsg() silently frees a dropped API message; every netconn caller then blocks forever on a reply that will never come | Small | Medium | VERIFIED |
| RE-SMS-076 | lwip_sendto() discards both re-connect results and dereferences `to` unchecked | Small | Low | VERIFIED |
| RE-SMS-077 | lwip_getpeername()/lwip_getsockname() discard the peer/addr result; netconn_addr() itself has no NULL-PCB check | Trivial | Low | VERIFIED |
| RE-SMS-078 | TCP initial sequence numbers are predictable: a counter starting at 6510 advanced only by the 500 ms tick count | Small | Low | VERIFIED |
| RE-SMS-079 | tcp_keepalive() never sets the TCP flags byte; probes go out with whatever flags the recycled heap buffer last held | Trivial | Low | VERIFIED |
| RE-SMS-080 | udp_send() leaks the chained header pbuf on the no-route return | Trivial | Low | VERIFIED |
| RE-SMS-081 | netconn_new_with_proto_and_callback()'s error path leaks the mbox it created and the PCB do_newconn created | Small | Low | WEAKENED |
| RE-SMS-082 | Packet-duration and FLAC-duration math divide by file-controlled values that can be zero | S | Low | VERIFIED |
| RE-SMS-083 | A Vorbis file with ≥3 channels returns −1 from the OGG creator and aborts the entire probe chain | S | Low | VERIFIED |
| RE-SMS-084 | DTS: mono float-to-int converter is an empty stub — mono DTS "decodes" to stale buffer contents | Small | Low | VERIFIED |
| RE-SMS-085 | WMA: `wma_init` divides by the container-supplied sample rate without a zero check | Trivial | None | VERIFIED |
| RE-SMS-086 | AC3/DTS: `Decode` writes uninitialized `lBitRate`/`lSampleRate` locals to the codec context when sync fails | Trivial | None | VERIFIED-DRIFT |
| RE-SMS-087 | AAC: multichannel frames are mixed through `pcm_synN` with phantom copies of channel 0 for 3–4 channel content | Small | Low | VERIFIED |
| RE-SMS-088 | DivX packed-B start-code scan never advances through the buffer | Trivial | Negligible | VERIFIED |
| RE-SMS-089 | DXSB frames are emitted with `m_EndPTS` never initialized | Trivial | Negligible | VERIFIED |
| RE-SMS-090 | DSP MDCT init: unchecked table allocation and a dead error test | Small | Low | VERIFIED |
| RE-SMS-091 | `MC_SeekS` can return an uninitialised value | Trivial | None | VERIFIED |
| RE-SMS-092 | `SMS_SubContID` can never return FLAC: it compares the wrong suffix | Trivial | Low | VERIFIED |
| RE-SMS-093 | `FindPath` computes a subdirectory's sector count from the root directory's size (SMSCDVD) | Trivial | Low | VERIFIED |
| RE-SMS-094 | UDPFS SMAP PHY-reset retry bound fires on the first poll (INFERRED) | Trivial | Low | VERIFIED |
| RE-SMS-095 | `g_ASCII` is declared 4,572 in its consumer and defined 5,416 in its data file | Small | Low | VERIFIED |
| RE-SMS-096 | `UnicodeToCP`'s binary search can never reach table index 0 (proven by simulation) | Trivial | Low | VERIFIED |
| RE-SMS-097 | Icon loader hand-declares `lzma2_uncompress` with the wrong type and ignores its result | Trivial | Low | VERIFIED |
| RE-SMS-098 | Give `libds34bt.h` / `libds34usb.h` include guards and `(void)` prototypes | Trivial | Very low | VERIFIED-DRIFT |
| RE-SMS-099 | Shared bit reader (`SMS_Bitio.h`) has no end-of-buffer check in any reader — the root mechanism under most stream-side over-reads | None | None | VERIFIED |
| RE-SMS-100 | FLAC: decoder trusts the channel count beyond its 2-entry synth table and a shift-divide (currently saved only by the container's gate) | Trivial | None for any file the current container accepts | WEAKENED |
| RE-SMS-101 | `SMS_GetContainer`'s explicit-format path indexes the creator table with an unchecked caller value | S | Low | VERIFIED |
| RE-SMS-102 | The whole legacy SMSSMB tree is dead and internally duplicated — delete it | Small | Low | VERIFIED |
| RE-SMS-103 | Delete the dead generated header `include/s_iop_image.h` (3,820 lines, ~373 KB) | Trivial | Very low | VERIFIED |
| RE-SMS-104 | Move non-site binaries out of the GitHub Pages tree (`docs/downloads/`, `docs/investigations/` ≈ 11.9 MB) | Medium | Medium | VERIFIED |
| RE-SMS-105 | AC3: `s_Samples` workspace is 192 KB of BSS; the highest offset ever used is ~3,072 floats (~12 KB) | Trivial | Low | VERIFIED |
| RE-SMS-106 | Default (BDM) build carries 23,488 bytes of `g_DataBuffer` segments nothing can reach | Medium | Medium | VERIFIED |
| RE-SMS-107 | Remove ten defined-but-never-called EE functions (≈200 lines, incl. a 105-line IPU routine) | Small | Low | VERIFIED |
| RE-SMS-108 | Delete the thirteen never-called DS34 RPC client stubs | Small | Low | VERIFIED-DRIFT |
| RE-SMS-109 | Delete the unreferenced `utils/` directory (prebuilt Win32 exe + orphan Loader) | Trivial | Low | VERIFIED-DRIFT |
| RE-SMS-110 | Remove the orphan blob `irx/bdmfs_vfat.irx.xz` | Trivial | Very low | VERIFIED-DRIFT |
| RE-SMS-111 | `images/` contains an editor backup and two large source PNGs nothing references | Trivial | Low | VERIFIED |
| RE-SMS-112 | Turkish font pack ships the same font twice under two names | Trivial | Low | VERIFIED |
| RE-SMS-113 | Dead code shipped in the SMSTCPIP module: an uncompiled loopif, two debug-only lookup functions that always return 1, a compiled-out input queue, and compiled-out forwarding | Small | Low | VERIFIED |
| RE-SMS-114 | SO_REUSE port-sharing machinery is compiled in for a single-application media client that never shares ports | Medium | Medium | WEAKENED |
| RE-SMS-115 | memp reserves a RAW-PCB pool for a protocol that has no implementation file | Trivial | Low | VERIFIED |
| RE-SMS-116 | Half-finished API surface: six api.h functions have no implementation, and the exported shutdown entry point is an empty stub | Trivial | Low | VERIFIED-DRIFT |
| RE-SMS-117 | Remove six declaration-only prototypes with no definition anywhere | Trivial | Very low | VERIFIED |
| RE-SMS-118 | Dead H.263 inter-unquantize path: empty function, `#if 0` body that cannot compile, wired but never called | Small | Low | VERIFIED |
| RE-SMS-119 | ASF stream-bitrate table is parsed into one array and applied from a different, never-written one | S | Low | VERIFIED |
| RE-SMS-120 | ASF index support is scaffolding for a feature that was never built | S | Low | VERIFIED |
| RE-SMS-121 | Write-only state: fields and struct members that are stored and never read | S | Low | VERIFIED-DRIFT |
| RE-SMS-122 | DTS: file-scope sample-rate/bit-rate tables are dead duplicates of the live copies inside `_dts_syncinfo` | Trivial | None | VERIFIED |
| RE-SMS-123 | AC3 IMDCT: `aBias` parameter of `_ac3_imdct_512`/`_ac3_imdct_256` is threaded through every call but never used | Trivial | None | VERIFIED |
| RE-SMS-124 | AC3: `s_SAB` is declared `[20]` with only 8 entries initialized; its sole consumer reads 2 of them | Trivial | None | VERIFIED |
| RE-SMS-125 | `lzma2.c` exports dead, collision-prone functions named `encode` and `decode` | Trivial | None found | VERIFIED |
| RE-SMS-126 | Generated icon header (and its generator) still say "11 dev" — there are 13 dev slots | Trivial | None | VERIFIED |
| RE-SMS-127 | `src/SMS_Menu.c` is a dead duplicate stub that is not in the build | Trivial | None | VERIFIED |
| RE-SMS-128 | USBD exports three stubs that report success (USBD) | Trivial | Low | VERIFIED |
| RE-SMS-129 | File mapping is half-finished: every map returns the same fixed virtual address | Small | Medium | VERIFIED |
| RE-SMS-130 | `FATFS_Init` leaks its filesystem allocation on two early returns (SMSUMS) | Trivial | Low | VERIFIED |
| RE-SMS-131 | MOV carries two-to-three parallel copies of the `hdlr`/`stsd`/`esds` parsers that have already drifted apart | L | Med | WEAKENED |
| RE-SMS-132 | Whole-file asset loading is triplicated (skin) and triplicated again (language) | Medium | Medium | VERIFIED |
| RE-SMS-133 | The PCB-creation switch is triplicated across do_newconn/do_bind/do_connect — and the copies have already diverged | Small | Low | VERIFIED |
| RE-SMS-134 | Fold the eleven one-declaration `SMS_Container*.h` stub headers into `SMS_Container.h` | Medium | Low-medium | VERIFIED-DRIFT |
| RE-SMS-135 | MOV/M4A creators select their parser by mutating a shared global parse table at runtime | M | Low | VERIFIED |
| RE-SMS-136 | Fold the redundant explicit object rules in the root Makefile | Trivial | Low | VERIFIED-DRIFT |
| RE-SMS-137 | `dev9.h` duplicated verbatim in two IOP trees | Trivial | Low | VERIFIED |
| RE-SMS-138 | Deduplicate `AGENTS.md` / `CLAUDE.md` (byte-identical files) | Trivial | Very low | VERIFIED |
| RE-SMS-139 | IPU suspend/resume duplicated verbatim as "dummy" variants | Trivial | Negligible | VERIFIED |
| RE-SMS-140 | MOV index builder resets every stream's chunk cursor per chunk, making derived chunk sizing quadratic | S | Med | WEAKENED |
| RE-SMS-141 | `smb_GetShareList` spends one full NetShareEnum RPC per share (smbman-sms) | Medium | Medium | VERIFIED |
| RE-SMS-142 | Replace the hand-maintained 64-line host-test list in CI with discovery | Small | Low | VERIFIED-DRIFT |
| RE-SMS-143 | Drop stale `src/lzma2` references from `.gitignore` and the Makefile link path | Trivial | Very low | VERIFIED |
| RE-SMS-144 | Gate `test-media.yml` regeneration on media-relevant paths | Trivial | Low | VERIFIED-DRIFT |

## Narrowed scopes
- RE-SMS-017: Allocation-descriptor OOB read is real, but `apData` points into the 18,576-byte `s_Buffer[9*2064]`, not a 2,048-byte buffer, and the claimed `UDFDecode` write overflow is unreachable at both call sites (constant 128 exact-fit at :138; u8 `lLFI` ≤255 into `s_ScanTemp[2048]` at :399) — true scope is that OOB read plus a latent decode contract and an OOB source read via `38 + lLIU`.
- RE-SMS-022: ADTS (and ADIF) path fully exposed, but the ASC main index is rejected (`AudioSpecificConfig2` returns −2 when `get_sample_rate()==0`, checked by `_NeAACDecInit2`); ASC exposure survives only via the unvalidated SBR-extension sub-index (12–15) copied to `hDecoder->sf_index`.
- RE-SMS-025: Headline UDP cross-struct corruption is refuted — `so_options`/`tos`/`ttl` sit in the common `IP_PCB` prefix at identical offsets in both PCB types and TCP-only fields are unreachable for UDP; surviving scope is only a NULL-PCB dereference after `err_tcp` at SOL_SOCKET/IPPROTO_IP level.
- RE-SMS-046: A sequential post-reset TCP recv returns at the `conn->err` check first, so the :403 deref needs a narrow cross-thread race (recv passes the check, then `err_tcp` NULLs `pcb.tcp` before :403); the UDP NULL-buf half has no in-tree poster — true scope is the one racy TCP guard.
- RE-SMS-060: Partial-final-frame over-read and zero-`block_align` non-advance are verified as loop properties, but the claimed hang additionally requires `wma_decode_frame` to keep succeeding on a zero-length bit context — if it errors first, the function returns.
- RE-SMS-061: The filed −512 / index-256 mechanism is disproven for parsed data — exponent deltas are bounded to ±2, capping the index at ≤255; residual scope is only a latent one-element over-read contingent on unproven `EXP_REUSE` stale-boundary data.
- RE-SMS-070: The bound mismatch is real in code (1520-byte gather buffer vs 4096-byte FIFO budget), but no in-tree path hands `SMap_Send` a chain over 1520 bytes (MTU/fragmentation caps frames at 1514) — a latent wrong constant, incidentally safe today.
- RE-SMS-071: The unchecked exported callback arrays are real, but every in-tree caller passes constants (interrupts 2–6, DMA channel 1) — true scope is export-surface hardening with no demonstrated in-tree reachability.
- RE-SMS-081: The mbox leak on two error paths is verified, but the orphaned-PCB half is defeated — `do_newconn` sets `conn->err` only where `udp_new()`/`tcp_new()` returned NULL, so no registered PCB is orphaned; the fix scopes to `sys_mbox_free(conn->mbox)` only.
- RE-SMS-100: The synth-table index is latent only — the shipped container gate (<3 channels) and `_decode_frame`'s assignment-mismatch check limit it to 0 or 1, and the claimed divide-by-zero is unreachable outright (channels are a 3-bit field, max 8, so `4096 >> nChannels` is ≥16, never 0).
- RE-SMS-114: The compiled-in, unused SO_REUSE machinery premise holds, but the filed fix (set `SO_REUSE` to 0) deletes nothing because every guard is `#ifdef`/`#ifndef` — removal requires leaving the macro undefined/`#undef`'d or converting the guards to `#if`.
- RE-SMS-131: Duplication and drift are real, but one filed drift example is false (the probe `stsd` also walks child atoms), two ranges are inflated (demuxer `stsd` :322-382, `esds` :429-469), and the ~100-line saving is an upper bound.
- RE-SMS-140: The quadratic per-chunk cursor reset is real, but the filed pure-deletion fix is unsafe — the reset is what guarantees all cursors are 0 between per-stream `_mov_build_index` calls; the remedy is moving the reset to per-build, with offset-ordered chunk tables as a stated precondition.

## Defeating evidence
- RE-SMS-001: The guard at `src/SMS_Container.c:152-153` compares `int lID` against `sizeof(s_CCreator)/sizeof(s_CCreator[0])`, a `size_t` — −1 converts to unsigned 4294967295, `4294967295 < 13` is false, so the branch is never taken for unknown extensions, `s_CCreator[-1]` is never indexed, and execution falls to the default-order `else memcpy` (proven with a compiled test of the identical expression shape).

## Candidates
### CAND-01
- Label: DTS downmix row-index overflow
- Mechanism: At `src/SMS_DTS.c:10261` the downmix lookup `table[flags & DTS_CHANNEL_MASK][input]` takes its row index unchecked. Via the `_dts_decode` else-branch that row index can reach 63 into an [11] table.
- Locations: `src/SMS_DTS.c:10261`; `_dts_decode` else-branch `src/SMS_DTS.c:11567-11571`
- Proposed class: EXTENSION of RE-SMS-010
- Justification: Same downmix table, sink and index-check fix site as RE-SMS-010 — an additional unchecked index at that sink.

### CAND-02
- Label: UDFDecode read-side defects
- Mechanism: `UDFDecode` performs a 16-bit read one element past `aLen` at :99. At the :399 call site a disc-supplied u16 `lLIU` makes the source offset `38 + lLIU` read out of bounds into the 4096-byte `s_ScanBuf`.
- Locations: `iop/SMSCDVD/SMSCDVD_UDFS.c:99`; `iop/SMSCDVD/SMSCDVD_UDFS.c:399`
- Proposed class: EXTENSION of RE-SMS-017
- Justification: Same `UDFDecode` function and bounds fix as RE-SMS-017 — additional read-side instances at that site.

### CAND-03
- Label: WMA v1 exponent rows 1+ never filled
- Mechanism: The WMA v1 band computation at `src/SMS_WMA.c:1354` always writes row 0, while `decode_exp_vlc` selects the exponent row by block size at :1635. Rows 1+ therefore stay zero, triggering the zero-entry underflow without any repeated critical frequency.
- Locations: `src/SMS_WMA.c:1354`; row selection `src/SMS_WMA.c:1635`
- Proposed class: NEW
- Justification: Distinct root cause and fix (rows never populated) from RE-SMS-024's zero-width-entry guard, despite sharing the underflow sink.

### CAND-04
- Label: FAT LFN negative offset
- Mechanism: At `iop/SMSUMS/src/ums_fat.c:421` an LFN sequence byte of 0 gives `lOff = −13`. The subsequent name writes then land before the `m_Name` buffer.
- Locations: `iop/SMSUMS/src/ums_fat.c:421`
- Proposed class: EXTENSION of RE-SMS-004
- Justification: Same LFN offset computation and clamp site as RE-SMS-004 — an additional negative-offset instance of that mechanism.

### CAND-05
- Label: ASFContext calloc unchecked
- Mechanism: The ASF context `calloc` at `src/SMS_ContainerASF.c:157-159` is dereferenced by a `memset` on the next line with no NULL check.
- Locations: `src/SMS_ContainerASF.c:157-159`
- Proposed class: REJECTED
- Justification: Full duplicate — the identical site is already cited in RE-SMS-013 (ASF context calloc + memset at :157-159).

### CAND-06
- Label: Dead defensive check in netconn_recv TCP arm
- Mechanism: The check at `iop/SMSTCPIP/src/api_lib.c:444-446` tests a `buf` already dereferenced at :431-434 and already NULL-checked at :410. It is deletion-only and harmless.
- Locations: `iop/SMSTCPIP/src/api_lib.c:444-446`
- Proposed class: REJECTED
- Justification: Not a defect — the redundant check causes no failure; it is dead code only.

### CAND-07
- Label: CDDA empty-field uninitialised name
- Mechanism: In `src/SMS_FileContext.c:803-822`, on an empty CDDA field `*apBuf` is never assigned. The per-node `m_pName` therefore remains uninitialised malloc memory.
- Locations: `src/SMS_FileContext.c:803-822`
- Proposed class: NEW
- Justification: Explicitly distinct from RE-SMS-062's unterminated-buffer `strlen` — a different trigger (empty field) and failure in the same function.

### CAND-08
- Label: pbuf_header REF/ROM mirror underflow
- Mechanism: In the REF/ROM arm at `iop/SMSTCPIP/src/pbuf.c:306-307`, `header_size - p->len <= 0` passes for almost any negative size. The shared `len += header_size` update then still wraps the u16 lengths.
- Locations: `iop/SMSTCPIP/src/pbuf.c:306-307`
- Proposed class: EXTENSION of RE-SMS-049
- Justification: Same `pbuf_header` underflow mechanism and fix site as RE-SMS-049 — the additional REF/ROM arm.

### CAND-09
- Label: lzma2_decode_size counter type mismatch
- Mechanism: In `src/lzma2.c:51-82` an `int i` is looped against an `unsigned long records` count taken from the file. The signed/unsigned mismatch undermines the loop bound on crafted counts.
- Locations: `src/lzma2.c:51-82`
- Proposed class: EXTENSION of RE-SMS-053
- Justification: Same `lzma2_decode_size` walk and bounds-hardening fix site as RE-SMS-053 — an additional defect instance in that walk.

### CAND-10
- Label: getpeername/getsockname NULL namelen deref
- Mechanism: Both `lwip_getpeername` and `lwip_getsockname` in `iop/SMSTCPIP/src/sockets.c` (~:815-875) dereference `*namelen` with no NULL check. Passing a NULL `namelen` crashes the sockets layer.
- Locations: `iop/SMSTCPIP/src/sockets.c` ~:815-875
- Proposed class: NEW
- Justification: Stated independent of RE-SMS-077 — a different argument and sink from that finding's discarded-result / NULL-PCB mechanism.

### CAND-11
- Label: netconn_addr missing default
- Mechanism: `netconn_addr` at `iop/SMSTCPIP/src/api_lib.c:236-260` has no default case for non-UDP/TCP connection types. It returns ERR_OK with `*addr` and `*port` left unset.
- Locations: `iop/SMSTCPIP/src/api_lib.c:236-260`
- Proposed class: NEW
- Justification: Distinct mechanism (unset outputs on a success return) from RE-SMS-077's NULL-PCB dereference in the same function.

### CAND-12
- Label: Packed SMSSMB.IRX segment is dead bytes in every build
- Mechanism: The packed SMSSMB.IRX segment occupies 19,641 B (19,648 aligned) of resident `g_DataBuffer` EE RAM with no EE consumer in any build.
- Locations: `src/SMS_Data.c`; `include/SMS_Data.h:24,42`
- Proposed class: NEW
- Justification: This byte set is counted by neither RE-SMS-102 (source tree) nor RE-SMS-106 (UMS/USBD/SIO2MAN segments) — a distinct dead segment.

### CAND-13
- Label: Second orphan IRX pair of the RE-SMS-110 class
- Mechanism: `irx/mmcedrv.irx` and `irx/mmcedrv.irx.xz` are not in IOP_OBJS and are unreferenced. Their only mention is a comment at `src/SMS_IOP.c:1023` saying mmcedrv is not required.
- Locations: `irx/mmcedrv.irx`; `irx/mmcedrv.irx.xz`; comment `src/SMS_IOP.c:1023`
- Proposed class: EXTENSION of RE-SMS-110
- Justification: Same orphan-blob mechanism and class as RE-SMS-110 — an additional instance.

### CAND-14
- Label: Explicit-format path discards the creator's return
- Mechanism: At `src/SMS_Container.c:198` the explicit-format path calls the creator without assigning its return to `lSts`. The `lSts < 0` teardown can therefore never fire for explicit probes.
- Locations: `src/SMS_Container.c:198`
- Proposed class: NEW
- Justification: Distinct from RE-SMS-101's bounds claim at the same line — a lost return value, not an unchecked index.

### CAND-15
- Label: FLAC include-guard typo
- Mechanism: The include guard in `include/SMS_ContainerFLAC.h:11-12,28` is spelled `__SMS_CountainerFLAC_H`. It is internally consistent, so it works, but the spelling is fragile.
- Locations: `include/SMS_ContainerFLAC.h:11-12,28`
- Proposed class: REJECTED
- Justification: Not a defect — the guard is internally consistent and functions as written.

### CAND-16
- Label: MOV cursor cross-call invariant
- Mechanism: `m_NextChunk` correctness between per-stream `_mov_build_index` calls in `src/SMS_ContainerMOV.c` depends entirely on the per-chunk reset at :1034-1037 executing in the prior call. A per-build initialisation at entry (:936) would make the invariant explicit regardless of the RE-SMS-140 decision.
- Locations: `src/SMS_ContainerMOV.c:936` (entry); reset `src/SMS_ContainerMOV.c:1034-1037`
- Proposed class: EXTENSION of RE-SMS-140
- Justification: Same chunk-cursor reset mechanism and site as RE-SMS-140 — the additional cross-call framing of that invariant.
