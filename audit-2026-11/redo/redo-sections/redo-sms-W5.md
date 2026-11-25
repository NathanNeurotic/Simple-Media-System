# REDO Audit — Simple-Media-System — W5: IOP network stack (iop/SMSTCPIP)

Pin: `e65ced39a27eee8d996e9257e21594d1cac1c93d` (verified via `git rev-parse HEAD` in the clone).
Clone read-only; nothing edited. Blind redo — no prior audit material was read.

## (a) Coverage note

**Read in full** (every line): `src/ps2ip.c` (463), `src/api_msg.c` (731), `src/api_lib.c` (583),
`src/sockets.c` (1292), `src/tcpip.c` (133), `src/mem.c` (235), `src/memp.c` (169), `src/pbuf.c` (540),
`src/tcp.c` (914), `src/tcp_in.c` (1204), `src/tcp_out.c` (579), `src/udp.c` (803), `src/ip.c` (391),
`src/icmp.c` (205), `src/netif.c` (184), `src/etharp.c` (737), `src/ip_frag.c` (320),
`src/loopif.c` (121), `src/ip_addr.c` (38), `include/lwipopts.h` (222), `include/ps2ip_internal.h`,
`include/arch/sys_arch.h`, `include/arch/cc.h`, `include/lwip/sys.h`, `include/lwip/api_msg.h`,
`include/lwip/err.h`, `include/lwip/memp.h`, `Makefile`, `src/exports.tab`, `src/imports.lst`,
plus the SMS repo `AGENTS.md` (attribution policy only; no effect on a read-only audit).
Total C source read in full: 10,309 lines (`src/*.c`).

**Scanned at mechanism level** (bounds / length-trust / alloc-return paths only, not line-audited):
`src/inet.s` (667, MIPS asm checksums + `inet_aton`/`inet_ntoa`), and headers
`include/lwip/{opt,api,tcp,sockets,ip,ip_addr,pbuf,mem,netif,udp,icmp,inet,ip_frag,raw,dhcp,debug,def,arch,stats,snmp,tcpip}.h`,
`include/netif/{etharp,loopif}.h`, `include/irx_imports.h` (≈4,500 lines).
Censuses (counts by grep, not by eye): `udp_new()` ×9 and `tcp_new()` ×3 in `api_msg.c`;
`memp_malloc` call sites per file; `get_socket` call sites ×19 in `sockets.c`;
`pbuf_alloc` call sites ×21 tree-wide.

Not present in the tree (so not auditable): `raw.c`, `dhcp.c`, `sys.c` — see CAND-W5-31/33/34.

## (b) Candidates (sorted by benefit)

### CAND-W5-1 — ip_reass uses the byte-swapped total length to allocate and copy the reassembled packet
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/ip_frag.c:198-199`, `iop/SMSTCPIP/src/ip_frag.c:213`, `iop/SMSTCPIP/src/ip_frag.c:222`; aggravated by `iop/SMSTCPIP/src/pbuf.c:176-185`
Problem: On completion `ip_reasslen += IP_HLEN; ip_reasslen = swap16(ip_reasslen)` so the value can be
stored with `IPH_LEN_SET`. The *swapped* value is then passed to `pbuf_alloc(PBUF_LINK, ip_reasslen, PBUF_POOL)`
and used as the copy bound in `mips_memcpy(q->payload, &ip_reassbuf[i], min(q->len, ip_reasslen - i))`.
Example: a 300-byte reassembled datagram (0x012C) becomes length 0x2C01 = 11,265. `pbuf_alloc` PBUF_POOL
returns a *single* pool pbuf (capacity `PBUF_POOL_BUFSIZE` 1540) and blindly sets `p->len = p->tot_len = length`,
so ~11 KB is copied from the 5,780-byte `ip_reassbuf` into a 1,540-byte pool buffer, corrupting the pbuf pool
and adjacent memp pools. Any remote host that sends a complete fragmented IPv4 datagram triggers this;
fragmentation is compiled in (`IP_REASSEMBLY 1`, lwipopts.h). Even with the swap fixed, any reassembled
length >1540 overflows the same way because this port's PBUF_POOL allocation never chains.
Fix: Keep a host-order `totlen` for allocation/copy; use the swapped value only in `IPH_LEN_SET`.
Reject (drop) reassembled lengths > `PBUF_POOL_BUFSIZE` at the same point, or chain pool pbufs in
`pbuf_alloc` — the drop is the LESS fix and matches the 1540-byte link MTU reality of this stack.
Benefit: Removes a remotely triggerable IOP heap/pool overwrite on ordinary fragmented traffic.
Effort: Small. Risk: Low — fragmented datagrams >1540 bytes are dropped instead of corrupting memory.
Confidence: READ

### CAND-W5-2 — get_socket() indexes the fixed socket table with no bounds check
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/sockets.c:104-115` (definition, `&sockets[aSD]` unchecked); all 19 call sites in
`sockets.c` (e.g. :152, :203, :235, :262, :295, :325, :397, :445, :816, :850, :881, :1077, :1261);
`sockets.c` `lwip_selscan` loops `i < maxfdp1` calling `get_socket(i)` with caller-supplied `maxfdp1`
Locations (cont.): table size `NUM_SOCKETS = MEMP_NUM_NETCONN = 15` (`sockets.c:52`, `lwipopts.h`),
while `FD_SETSIZE` is 16 (`include/lwip/sockets.h:197`) — even a legitimate `select(FD_SETSIZE, …)` reads slot 15.
Problem: Any negative or ≥15 descriptor reads/writes outside `sockets[15]`. Worse than a bad read:
`lwip_close` writes `sock->conn = NULL`, `sock->lastdata = NULL` through the returned pointer, so an
out-of-range fd corrupts whatever static lies adjacent. Every exported `lwip_*` entry point is reachable
with an arbitrary int fd via the IRX export table (`src/exports.tab`).
Fix: One check in `get_socket`: `if (aSD < 0 || aSD >= NUM_SOCKETS) { set_errno(EBADF); return NULL; }`.
All callers already handle NULL. Also clamp `maxfdp1` to `NUM_SOCKETS` in `lwip_select`/`lwip_selscan`.
Benefit: Closes an out-of-bounds read/write primitive on every socket entry point with a 2-line fix.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-3 — lwip_close() writes errno through a NULL socket pointer on any bad descriptor
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/sockets.c:235-250` (`sock_set_errno(sock, lErrNo)` after the `!sock` branch)
Problem: When `get_socket` returns NULL (any in-range but unopened fd — no out-of-range value needed),
the error branch sets `retVal = -1`, then unconditionally executes `sock_set_errno(sock, lErrNo)`, whose
macro body is `sk->err = (e)` — a NULL-pointer write. Closing an unopened descriptor crashes the stack
instead of returning `EBADF`.
Fix: In the `!sock` branch use `set_errno(EBADF)`; call `sock_set_errno` only in the non-NULL branch.
Benefit: Turns a guaranteed crash on a routine error path into the intended -1/EBADF.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-4 — do_connect() discards tcp_connect()/udp_connect() results; a synchronous TCP failure is never posted back, so netconn_connect() blocks
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/api_msg.c:505-525` (TCP arm at :517-519: `tcp_connect(...)` return dropped, no
`sys_mbox_post` in that arm); UDP arm :512-514 (`udp_connect(...)` return dropped, `conn->err` untouched);
caller blocks at `iop/SMSTCPIP/src/api_lib.c` `netconn_connect()` (`sys_mbox_fetch(conn->mbox, &mem)`)
Problem: For TCP, the only `conn->mbox` post on the success path is inside the `do_connected` callback,
which `tcp_connect` invokes only on SYN-ACK. When `tcp_connect` fails synchronously (e.g. `tcp_enqueue`
returns ERR_MEM under pool pressure, `tcp.c` `tcp_connect()`), nobody posts the mbox and `conn->err` is
never set from the return value: the calling thread waits until the SYN retransmission machinery aborts
the PCB and `err_tcp` happens to post the mbox — or indefinitely if that path is not taken (duration
INFERRED). For UDP, a failed `udp_connect` (bind failure) is reported to the application as success.
Fix: Capture the return in both arms: `msg->conn->err = tcp_connect(...)`; if it is not ERR_OK,
`sys_mbox_post(msg->conn->mbox, NULL)` immediately (do_connected still posts on the async success path).
Same for `udp_connect`.
Benefit: `connect()` fails fast with the real error instead of hanging the application thread.
Effort: Small. Risk: Low — must not double-post on success; post only on synchronous error.
Confidence: READ (hang-until-timeout duration INFERRED)

### CAND-W5-5 — do_bind() uses udp_new()/tcp_new() results without the NULL check its sibling functions have
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/api_msg.c:393-410` (`udp_new()` at :393/:398/:403 fed straight into
`udp_setflags`/`udp_recv`/`udp_bind`; `tcp_new()` at :409-410 fed into `setup_tcp`/`tcp_bind`)
Problem: `do_newconn` (:285-330) and `do_connect` (:455-500) check every allocation and post ERR_MEM;
`do_bind` checks none. When the 15-entry UDP/TCP PCB memp pools are exhausted, `bind()` on a fresh
netconn dereferences NULL inside the tcpip thread, taking down the whole stack, not just the caller.
Fix: Copy the exact check pattern already in `do_connect` (set `conn->err = ERR_MEM`, post mbox, return).
Benefit: Pool exhaustion becomes a clean EBADF/ENOMEM-style failure instead of a tcpip-thread crash.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-6 — Client netconns never get a semaphore, but netconn_write() waits on it
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/api_lib.c:146` (`conn->sem = SYS_SEM_NULL`, never created in
`netconn_new_with_proto_and_callback`); waits at `api_lib.c:521` and `api_lib.c:545` (`netconn_write`);
contrast `api_msg.c:252` (accepted conns *do* get `sys_sem_new(0)`) and the guarded wait at `api_lib.c:574`
(`netconn_close` checks `sem != SYS_SEM_NULL`); signallers `api_msg.c` `poll_tcp`/`sent_tcp` both skip
`SYS_SEM_NULL` semaphores, so a client conn can never be woken by design
Problem: The send-flow-control mechanism is half-wired: for every outbound (client) connection — the only
kind a media client normally creates — `conn->sem` stays 0. A `netconn_write` larger than `TCP_SND_BUF`
(5,840 bytes) reaches `WaitSema(conn->sem)` on semaphore 0, which no one will ever signal; at best it
returns an IOP error immediately and the write busy-loops posting zero-progress writes, at worst it waits
on an unrelated kernel semaphore (exact IOP behaviour INFERRED). `poll_tcp`/`sent_tcp` were written to
signal this semaphore and silently never do for client conns.
Fix: Create the semaphore in `netconn_new_with_proto_and_callback` exactly as `accept_function` does
(`conn->sem = sys_sem_new(0)`, fail the constructor on SYS_SEM_NULL); `netconn_delete` already deletes it
when non-NULL (`api_lib.c:203`).
Justification (if ADDS): One `sys_sem_new` call restoring the mechanism the surrounding code already
assumes; no new abstraction. The alternative (guarding the waits like `netconn_close`) leaves large
writes spin-looping.
Benefit: Large TCP sends (SMB writes, uploads) block-and-resume as designed instead of spinning/hanging.
Effort: Small. Risk: Medium — semaphore lifecycle must match `netconn_delete` (already handled).
Confidence: READ (runtime symptom INFERRED)

### CAND-W5-7 — ip_reass trusts IPH_LEN over the actual pbuf chain, and its bitmap index goes one byte out of bounds at exactly 5760 bytes
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/ip_frag.c:58-77` (`copy_from_pbuf` walks `p = p->next` with no NULL stop);
`ip_frag.c:146-159` (`len` from wire `IPH_LEN`, then `copy_from_pbuf(p, &i, …, len)`);
`ip_frag.c:174` and `:195` (bitmap index `(offset+len)/64` / `ip_reasslen/64` == 90 when the total is
`IP_REASS_BUFSIZE` 5760, but `ip_reassbitmap` has 5760/64 = 90 bytes, indices 0-89);
entry gap at `iop/SMSTCPIP/src/ip.c:224-230` (`pbuf_realloc(p, IPH_LEN)` cannot grow, so a header claiming
more data than the chain holds passes through with `tot_len` still short)
Problem: A fragment whose IPH_LEN exceeds the received chain length makes `copy_from_pbuf` copy `len`
bytes by walking off the end of the chain and dereferencing NULL. A fragment completing exactly 5,760
bytes writes/reads one byte past the static bitmap into adjacent statics.
Fix: In `copy_from_pbuf`, stop at chain end and report the shortfall (drop the datagram in `ip_reass`);
change the reassembly bound to `offset + len >= IP_REASS_BUFSIZE` rejection for the final byte, or size
the bitmap `IP_REASS_BUFSIZE/64 + 1`. The chain-end stop is the primary fix.
Benefit: Removes a second remotely triggerable crash/corruption in the same reassembly path as CAND-W5-1.
Effort: Small. Risk: Low.
Confidence: READ

### CAND-W5-8 — icmp_dest_unreach() dereferences its pbuf_alloc() result unchecked and over-reads the offending packet
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/icmp.c:136-148` (`q = pbuf_alloc(...)`, then `idur = q->payload` with no NULL
check; `mips_memcpy(q->payload + 8, p->payload, IP_HLEN + 8)` with no check that `p->tot_len >= IP_HLEN + 8`);
same unchecked-alloc pattern at `icmp.c:169` (`icmp_time_exceeded`, currently compiled out with IP_FORWARD=0);
live callers: `iop/SMSTCPIP/src/udp.c` `udp_input` no-match path and `iop/SMSTCPIP/src/ip.c` `ip_input`
unknown-protocol path
Problem: Any UDP datagram to a closed port (or unknown-protocol IP packet) reaches this function. When
the 12 KB `mem` heap cannot satisfy the RAM allocation, `q` is NULL and the tcpip/driver thread crashes
building the error reply; a short offending packet is over-read by up to 28 bytes regardless.
Fix: `if (q == NULL) return;` after the allocation, and require `p->tot_len >= IP_HLEN + 8` (else copy
only `p->tot_len` / return). Apply identically at :169 for the day IP_FORWARD is enabled.
Benefit: A routine, remotely triggerable path stops being a crash under memory pressure.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-9 — ip_frag() dereferences both of its pbuf_alloc() results unchecked
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/ip_frag.c:262-263` (`rambuf = pbuf_alloc(PBUF_LINK, 0, PBUF_REF)` then
`rambuf->tot_len = …` immediately); `ip_frag.c:310-312` (`header = pbuf_alloc(PBUF_LINK, 0, PBUF_RAM)`
then `pbuf_chain(header, rambuf); netif->output(netif, header, dest)` — `pbuf_cat` tolerates NULL but
`etharp_output`/`pbuf_header` on a NULL header does not)
Problem: Fragmenting an outbound datagram larger than the MTU under memp exhaustion crashes in the
sending thread instead of returning ERR_MEM. `ip_frag`'s contract already returns `err_t`, so callers
(`ip_output_if`) can propagate the failure.
Fix: NULL-check `rambuf` (return ERR_MEM) and `header` (free `rambuf`, return ERR_MEM).
Benefit: Outbound fragmentation fails cleanly under the same pressure that makes CAND-W5-8 fire.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-10 — err_to_errno() maps every non-zero lwIP error to EIO
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/sockets.c:75-91` (table + macro); all users in `sockets.c`
(`lwip_bind`, `lwip_connect`, `lwip_listen`, `lwip_send`, getsockopt/setsockopt paths)
Problem: The guard is `(err) < (sizeof(err_to_errno_table)/sizeof(int))`. `sizeof` yields an unsigned
type, so the negative `err_t` is converted to a huge unsigned value and the comparison is always false
for every real error: ERR_MEM, ERR_USE, ERR_CONN, ERR_RST all surface to applications as `EIO` instead
of ENOMEM/EADDRINUSE/ENOTCONN/ECONNRESET. Diagnosis of every socket failure is destroyed (e.g. an
address-in-use bind is indistinguishable from an I/O error). Latent second defect: the table has 11
entries (0…-10) but `ERR_IF` is -11 (`include/lwip/err.h`), so a naively fixed guard would index one
past the table for ERR_IF.
Fix: Compare in signed space — `(-(err)) < 11` — and extend the table with an entry for ERR_IF (EIO is
fine) so the bound and the error range agree.
Benefit: Applications and logs get truthful errnos from every socket call again.
Effort: Trivial. Risk: Low — behaviour change is only in reported errno values, toward the documented ones.
Confidence: READ

### CAND-W5-11 — lwip_accept() dereferences the accepted conn, addr and addrlen without NULL checks
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/sockets.c:159-186` (`netconn_accept` result used at :161 by `netconn_peer`;
`*addrlen` read at :172 and `mips_memcpy(addr, …)` with no NULL check on `addr`/`addrlen`;
final `sock_set_errno(sock, 0)` applied to the *new* socket after `sock = get_socket(newsock)` at :183)
Problem: `netconn_accept` returns whatever the acceptmbox delivers — `err_tcp` posts NULL there when a
listening PCB errors — so `newconn` can be NULL and `netconn_peer(NULL, …)` / the subsequent code deref
it. Passing `addr == NULL` (legal POSIX for "don't care") crashes at `*addrlen`. On the failure path
(`alloc_socket` == -1) the errno is set on the listening socket, but on success the listening socket's
errno is never cleared — the code clears the new socket's instead.
Fix: Check `newconn == NULL` (return -1/ECONNABORTED), guard the copy with `if (addr && addrlen)`,
and set errno on the listening `sock` before reassigning it.
Benefit: accept() survives listener errors and the standard NULL-address call form.
Effort: Small. Risk: Low.
Confidence: READ

### CAND-W5-12 — getsockopt/setsockopt read and write TCP PCB fields on UDP sockets and on NULL PCBs
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/sockets.c:1004` (SOL_SOCKET flags via `pcb.tcp->so_options`),
`:1037` (IP_TTL via `pcb.tcp->ttl`), `:1037-1063`, writes at `:1197-1247` (`so_options`, `ttl`, `tos`,
TCP flags/keepalive via `pcb.tcp->…`); only the IPPROTO_TCP level checks the conn type (:~985, :~1115)
and even it silently `return 0` for non-TCP
Problem: `pcb` is a union (`api.h`): for a UDP socket, `pcb.tcp` overlays `struct udp_pcb`, whose field
offsets for `so_options`/`ttl`/`tos`/`flags`/`keepalive` do not match `struct tcp_pcb` — so
`getsockopt(udp_fd, IPPROTO_IP, IP_TTL)` returns garbage from the wrong offset and
`setsockopt(udp_fd, SOL_SOCKET, SO_KEEPALIVE)` *writes* into the UDP PCB at the TCP offset, corrupting
neighbouring UDP state. For a netconn whose PCB was never created (socket() then setsockopt before
bind/connect) or was nulled by `err_tcp`, the same lines dereference NULL.
Fix: At the top of the value-processing sections, require `sock->conn->pcb.tcp != NULL`; for
SOL_SOCKET/IP levels either dispatch on `conn->type` to the correct PCB struct or return ENOPROTOOPT
for non-TCP (LESS: these options are only meaningful for TCP in this stack, and the TCP level already
special-cases type).
Benefit: Stops cross-struct memory corruption from two exported, application-facing calls.
Effort: Small. Risk: Low — UDP callers currently get garbage/corruption, not behaviour worth preserving.
Confidence: READ

### CAND-W5-13 — tcpip_apimsg() silently frees a dropped API message; every netconn caller then blocks forever on a reply that will never come
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/tcpip.c:110-122` (`else memp_free(MEMP_API_MSG, apimsg)` when the
MEMP_TCPIP_MSG pool — 64 entries, lwipopts.h — is exhausted); every caller in `api_lib.c` follows
`api_msg_post(msg)` with `sys_mbox_fetch(conn->mbox, &mem)` (e.g. bind/connect/write/close)
Problem: This is the same discarded-failure shape as CAND-W5-4 but at the transport hop shared by *all*
netconn operations: pool exhaustion converts any bind/connect/write/close/delete into a permanent
block of the calling thread, with the request neither executed nor errored.
Fix: Do not free-and-drop: retry the `memp_malloc` (the tcpip thread continuously drains the pool via
`tcpip_thread`, tcpip.c:56-82, so a bounded spin/yield succeeds), i.e. loop until a slot is available
instead of discarding. That is the smallest change that preserves the synchronous API contract.
Benefit: Removes a whole class of hangs under burst load (e.g. many simultaneous connection attempts).
Effort: Small. Risk: Medium — a spin here must yield to the tcpip thread; on IOP the mbox post/fetch
path already involves semaphore waits, and allocation retry can piggyback on `sys_mbox_post` timing.
Confidence: READ (that the drop deadlocks the caller is code-provable; pool-exhaustion frequency INFERRED)

### CAND-W5-14 — accept_function() registers callbacks on the new PCB before allocating the netconn's mbox/sem, then frees the netconn on failure, leaving the PCB pointing at freed memory
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/api_msg.c:225-270` (`setup_tcp(newconn)` at :240, *then* `sys_mbox_new`/
`sys_sem_new` at :243-260; failure arms `memp_free(MEMP_NETCONN, newconn)` and return ERR_MEM)
Problem: `setup_tcp` installs `recv_tcp`/`sent_tcp`/`err_tcp` with `arg = newconn` on a live, registered
PCB. If mbox/sem allocation fails, the netconn is freed but the PCB is neither closed nor are its
callbacks cleared: the next segment on that connection calls `recv_tcp` with a dangling `conn` pointer
(use-after-free; trigger INFERRED — requires alloc failure at accept time plus subsequent traffic).
The PCB itself also leaks (never returned to any pool).
Fix: Allocate recvmbox/mbox/sem *before* `setup_tcp`; on failure after `setup_tcp`, call
`tcp_close`/`tcp_abort` on the PCB before freeing the netconn (mirrors `do_delconn`'s teardown order).
Benefit: Accept under memory pressure fails the one connection instead of arming a UAF in the tcpip thread.
Effort: Small. Risk: Low.
Confidence: READ (UAF trigger INFERRED)

### CAND-W5-15 — netconn_recv() dereferences the PCB and the received buffer without the NULL checks its own TCP branch demonstrates
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/api_lib.c:403` (`conn->pcb.tcp->state` — `err_tcp` sets `pcb.tcp = NULL`,
`api_msg.c:183`, so a recv after a reset/close races into a NULL deref);
`api_lib.c:455-456` (UDP arm: `buf->p->tot_len` immediately after `sys_mbox_fetch` — `err_tcp`-style
NULL posts and `recv_udp`'s design both allow a NULL/empty delivery to reach here; the TCP arm at
:418-440 handles `p == NULL` explicitly, the UDP arm has no equivalent)
Problem: Error/EOF delivery is a normal event (peer RST, abort). For UDP conns the recv path assumes a
non-NULL netbuf; for TCP conns it assumes the PCB still exists. Both assumptions are falsified by the
stack's own error callbacks in the same files.
Fix: Guard both: `if (conn->pcb.tcp == NULL) { conn->err = ERR_CONN; return NULL; }` before :403, and
in the UDP arm treat `buf == NULL` like the TCP arm treats `p == NULL` (free recvmbox, return NULL).
Benefit: Orderly EOF/error return instead of a crash on connection teardown — a path every long-lived
UDP/TCP session eventually takes.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-16 — lwip_sendto() discards both re-connect results and dereferences `to` unchecked
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/sockets.c:445-472` (`to` dereferenced at :458-459 with no NULL check;
`netconn_connect(...)` at :461 and :468 and `netconn_disconnect(...)` at :470 all return-discarded)
Problem: sendto is implemented as connect → send → restore/disconnect. If the first connect fails
(ERR_MEM/ERR_USE from the api_msg layer), the send still runs against the *previous* peer (or an
unconnected PCB) and its result is returned as if the datagram went to `to`. If the restore connect
fails, the socket is silently left connected to the sendto target, changing the semantics of every
later `send`/`recvfrom` on it. A NULL `to` crashes outright.
Fix: NULL-check `to`; check the first connect and return -1 with its errno on failure; check the
restore and surface its failure the same way (the datagram was sent, so at minimum do not lose the
state corruption silently — disconnect on restore failure).
Benefit: Datagrams stop going to the wrong peer with a success return code.
Effort: Small. Risk: Low.
Confidence: READ

### CAND-W5-17 — lwip_getpeername()/lwip_getsockname() discard the peer/addr result; netconn_addr() itself has no NULL-PCB check
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/sockets.c:827` (`netconn_peer(...)` result dropped, uninitialised `naddr`
then copied to the caller and 0 returned); `sockets.c:861` (same for `netconn_addr`);
`iop/SMSTCPIP/src/api_lib.c:236-260` (`netconn_addr` reads `conn->pcb.udp->local_ip` /
`conn->pcb.tcp->local_ip` with no NULL check, unlike `netconn_peer` at :212-234 which checks)
Problem: getsockname on a fresh (never bound/connected) socket crashes inside `netconn_addr`;
getpeername on an unconnected socket returns success with stack garbage as the "peer address"
instead of ENOTCONN.
Fix: Add the same NULL checks to `netconn_addr` as `netconn_peer`; propagate both functions' return
values in sockets.c (`if (netconn_peer(...) != ERR_OK) { sock_set_errno(sock, ENOTCONN); return -1; }`).
Benefit: Two exported calls stop crashing / lying on the most common misuse (querying before connect).
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-18 — lwip_recvfrom() dereferences netbuf_fromaddr() which is NULL for every TCP buffer
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/sockets.c:355-363` (`addr = netbuf_fromaddr(buf); … addr->addr`);
`iop/SMSTCPIP/src/api_lib.c` `netconn_recv` TCP arm sets `buf->fromaddr = NULL` explicitly
Problem: `recvfrom(tcp_fd, …, &from, &fromlen)` — a legal call — dereferences NULL when filling `from`.
UDP buffers always carry a fromaddr; TCP buffers never do, and the sockets layer does not distinguish.
Fix: `if (addr == NULL)` fill `sin` with zeroes / skip the copy, mirroring how the TCP arm already
signals "no origin" with `fromport = 0`.
Benefit: Removes a crash on a standard call combination.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-19 — tcp_process() SYN_SENT branch dereferences pcb->unacked without a NULL check
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/tcp_in.c:525-527` (debug print and the accept condition both use
`pcb->unacked->tcphdr->seqno`); the same hazard is independently documented in-tree at
`iop/SMSTCPIP/src/loopif.c:82-88` (comment describing `pcb->unacked` still being NULL in tcp_process)
Problem: A SYN-SENT PCB whose SYN segment failed to enqueue (`tcp_connect` proceeds to SYN_SENT even
when `tcp_enqueue` returns ERR_MEM, `tcp.c` `tcp_connect()`) has `unacked == NULL`; a SYN+ACK from the
peer then crashes the tcpip thread inside the state machine. (Trigger path INFERRED — it requires the
enqueue-failure state or an equivalent queue drain; the missing guard itself is READ.)
Fix: `if (pcb->unacked == NULL) return ERR_OK;` (drop the segment; retransmit/timeout machinery owns
the PCB) at the top of the SYN_SENT case.
Benefit: One malformed internal state stops being a remote-triggerable stack crash.
Effort: Trivial. Risk: Low.
Confidence: READ (trigger INFERRED)

### CAND-W5-20 — TCP initial sequence numbers are predictable: a counter starting at 6510 advanced only by the 500 ms tick count
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/tcp.c:909-913` (`tcp_next_iss`: `static u32_t iss = 6510; iss += tcp_ticks;`);
used for both directions at `tcp.c:506` (`tcp_connect`) and `tcp.c:822` (`tcp_alloc`, listen path);
same pattern, lower stakes: `iop/SMSTCPIP/src/ip.c:332` (`static u16_t ip_id` incremented by 1 per packet)
Problem: `tcp_ticks` increments in `tcp_slowtmr` (every 2×160 ms nominal). An attacker who can observe
or guess one connection's ISN — or simply the stack's uptime — can predict the next ISN to within a
few hundred, enabling blind sequence guessing against SMS's SMB/FTP-style sessions (exploitability
INFERRED; predictability is READ from the code — there is no other entropy input anywhere in the function).
Fix: Seed `iss` once in `tcp_init` from the IOP system clock low word (the port already uses
`USec2SysClock`/alarm clocks in `ps2ip.c`/`ip_frag.c`) and advance by a large odd stride per connection
plus ticks, instead of ticks alone. No new subsystem — one initialisation read of an existing clock.
Justification (if ADDS): The change adds a single seed read; the alternative (leaving a constant-start
counter) keeps blind-injection practical against a stack whose peers are on a LAN the user does not control.
Benefit: Raises blind TCP injection from trivial to impractical for this stack's threat model.
Effort: Small. Risk: Low.
Confidence: READ (exploitability INFERRED)

### CAND-W5-21 — tcp_keepalive() never sets the TCP flags byte; probes go out with whatever flags the recycled heap buffer last held
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/tcp_out.c:539-579` (header filled at :557-568 — src/dest/seqno/ackno/wnd/
urgp/hdrlen/chksum, but no `TCPH_FLAGS_SET` anywhere in the function; every sibling does:
`tcp_enqueue` :219, ACK in `tcp_output` :365, `tcp_rst` :492)
Problem: The pbuf is `PBUF_RAM` from `mem_malloc`; `mem` RAM is zeroed only once in `mem_init` and
recycled thereafter, so the flags byte is leftover data from a previous packet — a keepalive probe can
legitimately leave with SYN, RST or FIN bits set, resetting or confusing the very idle connection it
exists to preserve (SO_KEEPALIVE sockets, `tcp_slowtmr` keepalive path in `tcp.c`).
Fix: One line with the siblings: `TCPH_FLAGS_SET(tcphdr, TCP_ACK);`
Benefit: Keepalive probes stop being random-flag packets.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-22 — netbuf_ref() dereferences its pbuf_alloc() result unchecked
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/api_lib.c:71-79` (`buf->p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_REF);`
then `buf->p->payload = …` unconditionally); sole in-tree caller chain: `lwip_send` UDP arm
(`sockets.c`) → `netconn_send` → `do_send`/`udp_send`, which also assume `buf->p` is valid
Problem: When the MEMP_PBUF pool (40 entries) is exhausted, a UDP `lwip_send` crashes in `netbuf_ref`
instead of returning ENOBUFS — the caller *does* check `netbuf_new()` one line earlier, so the
unchecked second allocation is an inconsistency, not a style.
Fix: In `netbuf_ref`, if the allocation fails leave `buf->p = NULL`; in `lwip_send`'s UDP arm treat
`buf->p == NULL` after `netbuf_ref` as the existing ENOBUFS return (the check scaffolding is already there).
Benefit: UDP send under pool pressure returns an error instead of crashing.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-23 — pbuf_header() can underflow len/tot_len, and its main wire-facing caller strips 14 bytes without any length check
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/pbuf.c:291-315` (RAM/POOL arm adjusts `p->len += header_size` with only a
payload-lower-bound check; a negative step larger than `len` wraps the u16 lengths);
`iop/SMSTCPIP/src/ps2ip.c:245` (live path, QUEUE_INPUT=0: `pbuf_header(pInput, -14)` after
`etharp_ip_input`, with no `pInput->tot_len >= 14` check and the ethertype read at :238 taken from
`pInput->payload` before any length validation); related unchecked header readers:
`iop/SMSTCPIP/src/etharp.c:391-396` (`etharp_ip_input` casts payload to a 34-byte `ethip_hdr` unchecked),
`iop/SMSTCPIP/src/ip.c:202-230` (`ip_input` rejects `iphdrlen > p->len` but never `iphdrlen < IP_HLEN`)
Problem: A short Ethernet frame (runt) from the LAN makes the input path manufacture a pbuf whose
lengths claim far more data than arrived; downstream IP/TCP/UDP parsing then runs on stale pool bytes
as if they were wire data. The pool buffer is 1540 bytes, so this is primarily a data-integrity /
parser-confusion defect rather than a clean segfault (severity INFERRED).
Fix: In `ps2ip_input`, drop (`pbuf_free` + return) when `pInput->tot_len < 14` before touching the
header; in `pbuf_header`'s RAM/POOL arm, fail (return 1) when `header_size < 0 && -header_size > p->len`.
In `ip_input`, reject `iphdrlen < IP_HLEN` alongside the existing `>` check.
Benefit: Runts stop reaching three protocol parsers with fabricated lengths.
Effort: Small. Risk: Low.
Confidence: READ (severity INFERRED)

### CAND-W5-24 — ps2ip_getconfig() strcpy()s an unbounded name into a 4-byte field; netif_find() reads name[2] on any string
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/ps2ip.c:85-93` (`strcpy(pInfo->netif_name, pszName)`; field is
`char netif_name[4]` in `include/ps2ip_internal.h`); `iop/SMSTCPIP/src/netif.c` `netif_find()`
(`num = name[2] - '0'` before any validation)
Problem: Both functions take the interface name from the EE-side caller of the exported
`ps2ip_getconfig`/`ps2ip_setconfig`. A name longer than 3 chars + NUL overwrites `ipaddr`/`netmask`/
`gw` inside the caller's own `t_ip_info` (self-inflicted for a well-behaved caller, but the export
gives no contract), and a 1-2 character name makes `netif_find` read past the string.
Fix: Replace `strcpy` with a bounded copy of at most 3 chars + NUL (interface names in this stack are
exactly 2 letters + digit, cf. `netif_find`'s own matching), and in `netif_find` reject names whose
`name[2]` is not a digit before indexing further.
Benefit: An exported config API stops corrupting its own output struct on malformed input.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-25 — err_tcp() is the only PCB callback that dereferences its netconn argument unchecked
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/api_msg.c:177-205` (`conn = arg; conn->pcb.tcp = NULL; conn->err = err; …`
with no `conn == NULL` test); siblings in the same file all check: `recv_tcp` (:~120), `poll_tcp`,
`sent_tcp`, `do_connected` (:~445, `if (conn == NULL) return ERR_VAL;`)
Problem: `err_tcp` is invoked via `TCP_EVENT_ERR` from abort/timeout paths (`tcp_slowtmr`, `tcp_abort`)
with whatever was last registered by `tcp_arg`. Any path that leaves a PCB alive after its netconn is
gone (cf. CAND-W5-14) turns the error notification itself into a NULL/dangling deref, masking the
original failure with a crash.
Fix: Add the same first-line guard the siblings have; if `conn == NULL`, return after no-op.
Benefit: Error handling stops being a crash amplifier.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-26 — udp_send() leaks the chained header pbuf on the no-route return
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/udp.c` `udp_send()` — header-chain allocation at the `pbuf_header` fallback
(`q = pbuf_alloc(PBUF_IP, UDP_HLEN, PBUF_RAM); pbuf_chain(q, p);`), then the no-route early return
`return ERR_RTE;` at `udp.c:458` skips the `if (q != p) pbuf_free(q);` cleanup that the normal exit performs
Problem: Every UDP send to an unroutable destination (no matching netif and no default) while the
header had to be chained leaks one RAM pbuf + its heap block from the 12 KB heap. Repeated failed
sends (e.g. streaming to a host on a vanished route) drain the heap until CAND-W5-8/9/22 fire.
Fix: On the route-failure return, `if (q != p) pbuf_free(q);` before returning — or move the route
lookup above the header chaining (LESS: one less alloc/free pair on the error path entirely).
Benefit: A repeatable error stops being a slow heap leak.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-27 — netconn_new_with_proto_and_callback()'s error path leaks the mbox it created and the PCB do_newconn created
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/api_lib.c:138-164` (`error:` label frees only the netconn; reached from
three places: mbox-creation failure — nothing to leak, msg-allocation failure at :152 — leaks
`conn->mbox`, and `conn->err != ERR_OK` at :162 — leaks `conn->mbox` *and* the PCB that `do_newconn`
allocated and registered)
Problem: Under exactly the memory pressure that makes these paths likely, each failed `socket()` call
permanently loses a sys mbox (IOP semaphore pair + AllocSysMemory block, `ps2ip.c` `sys_mbox_new`) and,
in the third case, a PCB that stays registered with callbacks pointing at a freed netconn (cf. CAND-W5-25).
Fix: In the error path, if `conn->pcb.tcp != NULL` first post a DELCONN (or set a flag so the caller
does); always `sys_mbox_free(conn->mbox)` when it is not SYS_MBOX_NULL before `memp_free`.
Benefit: Failed socket creation stops consuming finite IOP resources.
Effort: Small. Risk: Low.
Confidence: READ

### CAND-W5-28 — mem_free() never checks the block was actually in use, so a double free silently corrupts the heap free-list
Category: 6 bugs/security
Locations: `iop/SMSTCPIP/src/mem.c:92-113` (`mem->used = 0; plug_holes(mem);` with only a range check
on the pointer; `plug_holes` at :60-84 merges neighbours based on the now-cleared flag)
Problem: Freeing the same block twice re-runs the merge logic on an already-merged block, rewriting
`next`/`prev` of neighbours from stale offsets and corrupting the singly-linked heap structure that
every `mem_malloc` (pbufs, TCP segments, ICMP replies) walks. No in-tree double free is asserted here —
this is the missing seatbelt that turns any present/future double free elsewhere (e.g. the pbuf
refcount paths) from a detectable no-op into heap corruption (reachability INFERRED).
Fix: `if (mem->used == 0) return;` before clearing — one comparison, using the flag the allocator
already maintains.
Benefit: Cheap hardening of the allocator every other subsystem depends on.
Effort: Trivial. Risk: Low.
Confidence: READ (exploitability/reachability INFERRED)

### CAND-W5-29 — ip_output_if() keeps its working IP-header pointer in a function-static variable shared by all callers
Category: 3 overengineering
Locations: `iop/SMSTCPIP/src/ip.c:331-332` (`static struct ip_hdr *iphdr; static u16_t ip_id = 0;`)
Problem: `iphdr` gains nothing from being static — it is assigned from `p->payload` on entry and used
only within the call — but it makes the function non-reentrant across the stack's two real execution
contexts: the tcpip thread (all TCP/UDP output) and the driver-side input thread (`ps2ip_input` calls
`ip_input` directly, whose ICMP echo reply re-enters `ip_output_if`). An interleave between the
assignment and the `IPH_*_SET` sequence writes one packet's header fields through the other packet's
pointer (race INFERRED from the two call contexts; the static itself is READ). `ip_id` being static
is necessary; `iphdr` is not.
Fix: Delete `static` from the `iphdr` declaration (keep it for `ip_id`).
Benefit: Removes a cross-thread shared-scratch hazard for zero cost.
Effort: Trivial. Risk: Low.
Confidence: READ (race INFERRED)

### CAND-W5-30 — The PCB-creation switch is triplicated across do_newconn/do_bind/do_connect — and the copies have already diverged
Category: 2 duplication
Locations: `iop/SMSTCPIP/src/api_msg.c:285-330` (do_newconn), `:386-415` (do_bind), `:455-500`
(do_connect) — census by grep: `udp_new()` ×9 and `tcp_new()` ×3 in this one file, in three
near-identical 4-arm switches (UDPLITE / UDPNOCHKSUM / UDP / TCP)
Problem: Three copies of one mechanism ("create the right PCB for this netconn type and attach
recv_udp/setup_tcp"). The copies have already drifted: do_connect and do_newconn NULL-check every
allocation, do_bind checks none — CAND-W5-5 is a direct product of this duplication.
Fix: Extract one `create_pcb(msg->conn)` helper returning err_t, called by all three; do_bind keeps
only its bind call, do_connect only its connect call. Net deletion of ~60 lines.
Benefit: One place to get allocation checking right; prevents the next divergent copy.
Effort: Small. Risk: Low — mechanical, behaviour-preserving except fixing CAND-W5-5 in passing.
Confidence: READ

### CAND-W5-31 — Dead code shipped in the module: an uncompiled loopif, two debug-only lookup functions that always return 1, a compiled-out input queue, and compiled-out forwarding
Category: 1 dead code
Locations: `iop/SMSTCPIP/src/loopif.c` (whole file, 121 lines — absent from `Makefile` IOP_OBJS and
`LWIP_HAVE_LOOPIF` is 0 in `include/lwip/opt.h:303-304`; the `AddLoopIF` caller in `ps2ip.c` is likewise
compiled out); `iop/SMSTCPIP/src/udp.c:84-141` (`udp_lookup`: every exit returns 1, including the
not-found exit at :136-139, and its header-pointer arithmetic is wrong besides; only referenced by
`ip_lookup` in `ip.c`, itself `#ifdef LWIP_DEBUG`-only with no live caller);
`iop/SMSTCPIP/src/ps2ip.c` `InputCB` + QUEUE_INPUT block (`#define QUEUE_INPUT 0`);
`iop/SMSTCPIP/src/ip.c` `ip_forward()` and `iop/SMSTCPIP/src/icmp.c:162-205` (`icmp_time_exceeded`)
under `IP_FORWARD 0` (lwipopts.h:117)
Problem: ~450 lines that cannot execute are carried in the source the maintainers must read, and two
of the items (loopif's unacked-NULL comment, QUEUE_INPUT's alternate input path) actively mislead
debugging of the live paths.
Fix: Delete loopif.c, udp_lookup/ip_lookup, and the QUEUE_INPUT block; keep ip_forward/icmp_time_exceeded
only if forwarding is a stated roadmap item, otherwise delete with the rest.
Benefit: Smaller, honest source tree; the live input path (`ps2ip_input` direct) becomes unambiguous.
Effort: Small. Risk: Low — all items are provably unreferenced/uncompiled in this build.
Confidence: READ

### CAND-W5-32 — SO_REUSE port-sharing machinery is compiled in for a single-application media client that never shares ports
Category: 1 dead code
Locations: `iop/SMSTCPIP/include/lwip/opt.h:338-340` (`SO_REUSE 1`); machinery:
`iop/SMSTCPIP/src/tcp_in.c` reuse loop with `p->ref++` + `goto again_1`, `iop/SMSTCPIP/src/udp.c`
`udp_input` reuse loops (`again_1`/`again_2`, `p->ref++`), reuse branches in `tcp_bind` (`tcp.c`) and
`udp_bind` (`udp.c`) — several hundred lines across four files
Problem: SO_REUSEADDR/SO_REUSEPORT fan-out (delivering one datagram/segment to multiple PCBs, with
manual pbuf refcount juggling) exists to let multiple processes share a port. SMS is the sole consumer
of this stack; nothing in the tree binds two sockets to one port deliberately. The machinery is the
most fragile code in both input paths (refcount + goto re-scan) and it runs on every packet.
Fix: Set `SO_REUSE` to 0 in lwipopts.h/opt.h (the `#ifndef` guards already provide the plain paths)
and delete the reuse-only branches once the simpler paths are confirmed building. Note the sockets
layer's SO_REUSEADDR *option storage* (`so_options` bits in getsockopt/setsockopt) is independent
and can stay.
Benefit: Deletes the riskiest per-packet branches in tcp_input/udp_input; bind checks become the
simple already-written non-REUSE versions.
Effort: Medium (rebuild + on-hardware smoke test of normal client/server flows). Risk: Medium — any
hidden reliance on double-binding would surface as ERR_USE; that is precisely the behaviour to learn
about in testing rather than in production.
Confidence: READ

### CAND-W5-33 — memp reserves a RAW-PCB pool for a protocol that has no implementation file
Category: 4 size/memory
Locations: `iop/SMSTCPIP/src/memp.c` (`memp_sizes`/`memp_num`/`memp_memory` include
`MEMP_NUM_RAW_PCB` = 4, `include/lwip/opt.h:78-79`); `include/lwip/raw.h` declares the PCB/API;
there is no `raw.c` in `src/` or in the Makefile, `LWIP_RAW` is never defined anywhere in the tree,
and the two `#if LWIP_RAW` call sites (`ip.c` `ip_input`, `api_msg.c` `recv_raw`) compile out
Problem: Four raw_pcb slots are permanently carved out of `memp_memory`, and the header/API surface
advertises a feature that cannot link if used. Small in bytes (~4 × (sizeof(raw_pcb)+header)), but it
is pure waste in a 2 MB-RAM-class IOP budget and a trap for anyone reading the API surface.
Fix: Set `MEMP_NUM_RAW_PCB` to 0 in lwipopts.h (or delete the RAW slot from the memp tables) and
delete `include/lwip/raw.h` + the `raw` member references if the protocol is not planned.
Benefit: Reclaims pool space and removes a phantom API.
Effort: Trivial. Risk: Low.
Confidence: READ

### CAND-W5-34 — Half-finished API surface: six api.h functions have no implementation, and the exported shutdown entry point is an empty stub
Category: 8 half-finished
Locations: `iop/SMSTCPIP/include/lwip/api.h` declares `netbuf_alloc`, `netbuf_free`, `netbuf_data`,
`netbuf_next`, `netbuf_chain`, `netconn_err` — grep of all of `src/` finds no definition for any of
them (api_lib.c implements only new/delete/ref/copy_partial and the netconn_* set minus `netconn_err`);
`iop/SMSTCPIP/src/ps2ip.c` `ps2ip_ShutDown()` (export slot 3, `src/exports.tab`) is `{ return 1; }` —
it stops no thread, alarm, or semaphore, and four further export slots (2, 4, 32, 37) are the empty
`ps2ip_Stub`
Problem: Anything linking against the documented netbuf API fails at link time, not at design time;
and the module advertises a shutdown that does not shut anything down (TimerThread alarms and the
tcpip thread run forever), so "unload/reload the stack" flows leak IOP threads and semaphores.
Fix: LESS option — delete the six unimplemented declarations (nothing in-tree calls them) and either
delete the ShutDown export or rename the expectation: implementing a real teardown is the MORE path
and should only be taken if a reload flow actually needs it. The reserved `ps2ip_Stub` slots are
normal IRX practice and can stay.
Benefit: The declared API becomes exactly the implemented API; no phantom teardown contract.
Effort: Trivial (deletion path). Risk: Low.
Confidence: READ

## (c) Checked-clean notes

- **Allocator-return discipline in the TCP output core is good**: `tcp_enqueue` (tcp_out.c) checks every
  `memp_malloc`/`pbuf_alloc`, unwinds via `tcp_segs_free(queue)` on `memerr`, and keeps `snd_queuelen`
  accounting consistent; `tcp_rst`/`tcp_output` ACK/`tcp_keepalive` all NULL-check their `pbuf_alloc`.
  `etharp_query` likewise checks its ARP-request allocation. The unchecked-alloc findings above are the
  exceptions, concentrated in icmp/ip_frag/api glue.
- **tcpip_thread / tcpip_input** (tcpip.c): `tcpip_input` correctly frees the pbuf and returns ERR_MEM
  when its message allocation fails; the thread loop frees every message after dispatch. No leak found
  there (the flaw is on the API-message hop, CAND-W5-13).
- **memp_init table sizing**: `memp_sizes`/`memp_num` are declared `[MEMP_MAX]` with 10 initialisers for
  11 enum values — the 11th (`MEMP_SYS_TIMEOUT`) is zero-filled, takes the `memp_tab[i] = NULL` branch,
  and `sys_timeout` is a no-op macro in this port (sys.h), so nothing ever allocates that type. No
  out-of-bounds read. Checked and cleared.
- **Checksum assembly** (`inet.s`, mechanism scan only): `inet_chksum_pseudo`/`inet_chksum_pbuf` walk the
  pbuf chain using each pbuf's own `len` field — they do *not* trust the caller-supplied length for the
  data walk, so the attacker-controlled UDPLITE length in `udp_input` cannot drive an over-read there.
  Caveat (INFERRED, not a candidate): because the length argument is used only for the pseudo-header,
  UDPLITE partial-coverage checksums are not honoured as RFC 3828 intends — UDPLITE sockets
  (NETCONN_UDPLITE) are effectively unusable for partial coverage, but nothing in SMS uses UDPLITE.
  `inet_ntoa`'s static 16-byte buffer is non-reentrant but has no in-tree caller (only `inet_addr` is exported).
- **udp_input / tcp_input demux and checksum order**: both verify the pseudo-header checksum before
  delivering to a PCB, drop broadcasts/multicasts for TCP, and `tcp_input` checks `tot_len` against the
  TCP header size before parsing fields. The header-length trusts that remain are CAND-W5-23's scope.
- **ARP timer/queue ownership** (etharp.c): `etharp_tmr` frees queued pbufs on expiry, `find_arp_entry`
  dequeues before recycling a stable entry, and `etharp_query`'s failure to queue the *first* packet to
  a new entry is upstream lwIP behaviour of this vintage (TCP retransmit / upper-layer retry covers it) —
  noted, not counted. `ETHARP_ALWAYS_INSERT 1` means any on-link ARP/IP source updates the cache; that is
  standard unauthenticated ARP behaviour for this era of lwIP and is recorded here as a known property,
  not a candidate (exploitability claims would be INFERRED and the fix is a redesign, against LESS).
- **mem.c allocator structure**: `mem_malloc`/`mem_realloc`/`plug_holes` maintain the block list
  correctly for the single/double-free-free case, all under CpuSuspendIntr; `mem_realloc`'s habit of
  returning the original pointer unchanged for out-of-range or grow requests is benign for its sole
  caller (`pbuf_realloc` shrink path). Only the missing used-check is counted (CAND-W5-28).
- **Build configuration**: `LWIP_STATS 0`, `LWIP_DHCP 0` (unless PS2IP_DHCP), `IP_FORWARD 0` are
  consistently honoured — the DHCP header surface and stats macros compile to nothing and cost no
  runtime memory. The Makefile compiles with `-DLWIP_NOASSERT`, so the `LWIP_ASSERT` sanity checks in
  tcp_in.c are inert in the shipped build; that is a deliberate size choice, recorded not counted.
- **sys_arch semaphore/mbox layer** (ps2ip.c): mbox ring indices are correctly masked
  (SYS_MBOX_SIZE 64, power of two), post/fetch correctly hand off via `iWaitPost`/`iWaitFetch` under
  interrupt suspension. `sys_arch_sem_wait`'s timed path returns `timeout-1` rather than measured
  elapsed time, which makes `sys_arch_mbox_fetch`'s elapsed accounting approximate — observable only
  as slightly-off timeout bookkeeping in drain loops; no correctness failure was provable from the
  code, so it is recorded here rather than counted (INFERRED impact).
- **No findings** in: `ip_addr.c` (trivial constants), `netif_set_*` setters beyond CAND-W5-24's caller,
  `pbuf_ref`/`pbuf_clen`/`pbuf_cat` refcount basics, `tcp_slowtmr`/`tcp_fasttmr` list walks (prev/next
  handling re-checked line by line), `udp_bind`/`udp_connect` insertion logic apart from the reuse
  branches in CAND-W5-32. Linear scans (ARP table 10, socket table 15, PCB lists ≤15) are proportionate
  to the table sizes — no structural performance candidate is warranted in this scope.
