# Sable — SMS Round 8, Task 3: IOP non-parsing internals (SMSTCPIP output/mem, USBD OHCI TD internals, SMSMAP init/register)

- **Repo / pin:** Simple-Media-System at `e65ced39a27eee8d996e9257e21594d1cac1c93d` (verified by `git rev-parse HEAD` this round; `git status` clean before and after; read-only, nothing edited).
- **Scope / pass:** Category 6/1 ONLY, three classes: (i) unchecked return values whose failure corrupts state (crash, hang, false-ready, stale accounting), (ii) dead code proven by repo-wide grep, (iii) obvious bounds errors in output-path queue/segment handling. Input/parsing paths were **not** re-audited — SMS-109/129/130/131/132/152 and the rest of rounds 1–7 are not repeated.
- **Labels:** candidates are `CAND-T3-n` (coordinator assigns SMS-158+). **READ** = code chain read at the cited lines this round. **INFERRED** = reachability/effect that code reading alone cannot settle (allocation-exhaustion timing, caller state, asm control flow, hardware behaviour).
- **Task-estimate corrections (READ, by `wc -l` at the pin):** the brief's line estimates do not match this pin. The seven named SMSTCPIP files total **4,503** lines (not ~10,100 — that figure was the R7 whole-tree remainder incl. headers and out-of-scope sources). `iop/USBD/src/hcd.c` totals **498** lines (not ~2,830); the OHCI TD/ED queue internals are split across `hcd.c` + `mem.c` + `usbio.c` + `usbd.c`, all read (see ledger). SMSMAP `smap.c` minus the RX path is 984 lines (close to the brief's ~1,080, which did not count `main.c`).

---

## 1. Exact per-file ledger

### (a) `iop/SMSTCPIP` output / mem remainder

| File | Lines total | Read this round | Still not read |
|---|---|---|---|
| `iop/SMSTCPIP/src/tcp.c` | 914 | 1–914, in full | 0 |
| `iop/SMSTCPIP/src/tcp_out.c` | 579 | 1–579, in full | 0 |
| `iop/SMSTCPIP/src/sockets.c` | 1,292 | 1–1,292, in full | 0 |
| `iop/SMSTCPIP/src/api_lib.c` | 583 | 1–583, in full | 0 |
| `iop/SMSTCPIP/src/api_msg.c` | 731 | 1–731, in full | 0 |
| `iop/SMSTCPIP/src/mem.c` | 235 | 1–235, in full | 0 |
| `iop/SMSTCPIP/src/memp.c` | 169 | 1–169, in full | 0 |
| **Subtotal** | **4,503** | **4,503** | **0** |

Supporting reads (not ledger files): `pbuf.c` windows :180–317 (alloc NULL paths, `pbuf_header` semantics), `tcp.h`/`ip.h`/`udp.h`/`lwipopts.h` constants (`MEMP_NUM_NETCONN` 15, `MEMP_NUM_PBUF` 40, `MEMP_NUM_TCP_PCB` 15, `MEMP_NUM_TCP_SEG` 40, `TCP_SND_QUEUELEN` 16, `TCP_MAXRTX` 12), `exports.tab` (the `lwip_*` functions are IRX exports), and a grep pass over `ps2ip.c` (it contains no socket functions and no fd validation — the `lwip_*` API is consumed directly by other IOP modules, e.g. `iop/SMSSMB/src/SMSMB.c` imports `lwip_close/connect/recv/send/socket`).

### (b) `iop/USBD` OHCI TD/ED queue internals

| File | Lines total | Read this round | Still not read |
|---|---|---|---|
| `iop/USBD/src/hcd.c` | 498 | 228–498 in full (268 new lines); 1–227 re-read for the queue call graph (descriptor pass was R7) | 0 |
| `iop/USBD/src/mem.c` | 230 | 1–230, in full (TD/ED alloc + list ops — required: this is where `allocTd`/`addToHcEndpointList` live) | 0 |
| `iop/USBD/src/usbio.c` | 309 | 1–309, in full (TD construction + busy-endpoint queues) | 0 |
| `iop/USBD/src/usbd.c` | 270 | 1–270, in full (done-queue processing `processDoneQueue_*`) | 0 |
| `iop/USBD/src/usbdpriv.h` | 315 | Targeted: queue/TYPE constants :219–272 (`GENTD_QUEUE`/`ISOTD_QUEUE`, `TYPE_CONTROL` 0x3F, `TYPE_BULK` 0x40, `TYPE_ISOCHRON` 0x41) | Remainder = struct layouts already exercised in R7 |
| `iop/USBD/src/hub.c` | 694 | Spot windows only: :200–360 (161 lines), solely to verify the `killEndpoint`/`removeEndpointFromDevice` queue-removal call graph | ~533 — hub descriptor/state machine; R7 scope, not TD internals |
| `iop/USBD/src/driver.c`, `interface.c` | 252 + 230 | Not read | All — driver/API dispatch layer, not TD/ED queue internals; out of (b) scope |

### (c) `iop/SMSMAP` init / register (RX excluded)

| File | Lines total | Read this round | Still not read |
|---|---|---|---|
| `iop/SMSMAP/smap.c` | 1,050 | 1–984 in full. The embedded-asm init blocks (:52–690: `_smap_phy_write/read`, `_smap_bd_init`, `_smap_reset_fifo_emac3`, `_smap_phy_reset`) were read as a register-sequence pass (control flow + which registers/flag words are written), not cycle-verified. C init/TX :692–984 line-read | 985–1,050 (66 lines) = `SMap_HandleRXInt` — deliberately excluded, SMS-131 (R7) |
| `iop/SMSMAP/main.c` | 345 | 1–345, in full (init/register focus :242–345; TX-queue asm :32–92 and interrupt glue :93–169 read as a sequence pass) | 0 |
| `iop/SMSMAP/smap.h` | 110 | 1–110, in full | 0 |
| **Subtotal** | **1,505** | **1,439** | **66 (RX, by instruction)** |

---

## 2. Candidates

### CAND-T3-1 — Sockets layer: `get_socket` indexes the 15-entry socket table with a completely unchecked fd
- **Category:** 6
- **Locations:** `iop/SMSTCPIP/src/sockets.c:104-115` (`get_socket`); table `sockets.c:46-67` (`NUM_SOCKETS` = `MEMP_NUM_NETCONN` = 15, `iop/SMSTCPIP/include/lwipopts.h:45`); every public entry funnels through it (`lwip_accept/bind/close/connect/listen/recvfrom/send/sendto/select`-via-`lwip_selscan:540,551/getsockopt/setsockopt/ioctl/getpeername/getsockname`)
- **Problem:** `get_socket` does `&sockets[aSD]` and reads `->conn` with no `aSD < 0 || aSD >= NUM_SOCKETS` check anywhere — not in `get_socket`, not in any caller, not in `ps2ip.c`. The `lwip_*` functions are IRX exports (`iop/SMSTCPIP/src/exports.tab`) called by other IOP modules with their own socket state (e.g. SMSSMB closes `s_ServerCtx[i].m_Socket` guarded only by `>= 0`). An out-of-range fd reads adjacent `.bss` as a `struct lwip_socket`; if the garbage `conn` field is non-NULL the caller proceeds into `netconn_*` calls on a garbage `netconn*` (write path: `lwip_close` frees it, `lwip_send` posts API messages with it). `lwip_selscan` additionally trusts caller-supplied `maxfdp1` into the same unchecked index.
- **Fix:** in words: add the two-sided range check at the top of `get_socket` (the single choke point) and return NULL/`EBADF` for out-of-range fds; no other call site changes.
- **Benefit:** closes an OOB read + potential garbage-pointer free/call reachable from any module using the exported socket API with a stale or corrupted fd.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### CAND-T3-2 — `lwip_close` dereferences NULL on any invalid-but-in-range fd (double close crashes)
- **Category:** 6
- **Locations:** `iop/SMSTCPIP/src/sockets.c:226-255`, sink `:250` (`sock_set_errno(sock, lErrNo)` unconditional); macro `sockets.c:99-102`; `get_socket` NULL contract `sockets.c:104-115`
- **Problem:** When `get_socket(s)` returns NULL (in-range fd whose `conn` is already NULL — i.e. a double close, or close of a never-opened slot), the `else` is skipped but line 250 still executes `sock_set_errno(sock, …)`, whose macro writes `sk->err` through the NULL pointer. Every other `lwip_*` entry returns immediately on `!sock`; `lwip_close` is the only one that falls through to a dereference. Result: closing an already-closed socket crashes the IOP instead of returning `EBADF`.
- **Fix:** in words: only execute the `sock_set_errno` when `sock` is non-NULL (or set the global `errno` directly in the NULL branch, as `get_socket` already does).
- **Benefit:** double-close — a routine error path for the SMB client that imports `lwip_close` — returns an error instead of taking down the network stack.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### CAND-T3-3 — `lwip_accept` uses the accepted connection and the caller's address buffers with no NULL checks
- **Category:** 6
- **Locations:** `iop/SMSTCPIP/src/sockets.c:159-172`; NULL producer `iop/SMSTCPIP/src/api_msg.c:196-201` (`err_tcp` posts NULL to `acceptmbox`); unchecked sink `netconn_peer`, `iop/SMSTCPIP/src/api_lib.c:211-233` (switches on `conn->type` at :215)
- **Problem:** (i) `netconn_accept` can return NULL — the listener's `err_tcp` posts a NULL into `acceptmbox` when the listening PCB errors — and `lwip_accept` passes the result straight to `netconn_peer`, which dereferences it. (ii) Lines 170–172 dereference `addrlen` and `memcpy` into `addr` unconditionally, so the POSIX-legal `accept(s, NULL, NULL)` form crashes. Contrast `lwip_recvfrom:355`, which does guard `from && fromlen`.
- **Fix:** in words: return -1/`ECONNABORTED` when `netconn_accept` yields NULL, and guard the address copy with `addr && addrlen` exactly as `lwip_recvfrom` already does.
- **Benefit:** a listener error (e.g. PCB exhaustion/reset during accept) degrades to an accept error instead of a NULL dereference in the accepting module.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ; the NULL-post producer is read at `api_msg.c:196-201`)

### CAND-T3-4 — `netbuf_ref` dereferences a possibly-NULL `pbuf_alloc` result (UDP send under pbuf exhaustion crashes)
- **Category:** 6
- **Locations:** `iop/SMSTCPIP/src/api_lib.c:70-80`, sink `:76` (`buf->p->payload = dataptr`); NULL return proven at `iop/SMSTCPIP/src/pbuf.c:202-207` (`PBUF_REF` branch returns NULL when `memp_malloc(MEMP_PBUF)` fails; pool is `MEMP_NUM_PBUF` 40); caller `iop/SMSTCPIP/src/sockets.c:413` (`lwip_send` UDP path, also unchecked)
- **Problem:** `netbuf_ref` assigns `buf->p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_REF)` and immediately writes through `buf->p` three times. When the pbuf pool is exhausted — 40 pbufs shared by the whole stack, the exact condition a busy SMB/UDP workload produces — the allocation returns NULL and the write crashes the IOP instead of surfacing `ENOBUFS` to the sender.
- **Fix:** in words: check the allocation in `netbuf_ref` and propagate failure (make it report/leave `buf->p` NULL and have the `lwip_send` UDP path test `buf->p` before `netconn_send`), mirroring the NULL checks `netbuf_new`'s callers already rely on.
- **Benefit:** memory pressure on the output path becomes a send error, not a stack crash.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### CAND-T3-5 — `do_connect` ignores `tcp_connect`'s return: a synchronous failure hangs `netconn_connect` forever
- **Category:** 6
- **Locations:** `iop/SMSTCPIP/src/api_msg.c:515-521` (TCP branch: no `sys_mbox_post` of its own); failure source `iop/SMSTCPIP/src/tcp.c:488-530` (`tcp_connect` returns `tcp_enqueue`'s result), `iop/SMSTCPIP/src/tcp_out.c:296-308` (`ERR_MEM` on segment/pbuf exhaustion); blocked waiter `iop/SMSTCPIP/src/api_lib.c:286-315` (`sys_mbox_fetch(conn->mbox, …)` at :312)
- **Problem:** The TCP branch of `do_connect` relies entirely on the `do_connected` callback to post the completion mbox. If `tcp_connect` fails *synchronously* — `tcp_enqueue` returns `ERR_MEM` when the shared `MEMP_NUM_TCP_SEG` (40) or pbuf pools are exhausted — the callback is never invoked, `err_tcp` never fires (no connection was ever established), and nothing ever posts to `conn->mbox`. The calling thread (e.g. the SMB module in `netconn_connect`) blocks in `sys_mbox_fetch` permanently: a wedged module, not an error return. Every sibling handler either posts unconditionally (UDP branch of the same function, :510-512) or checks its result (`do_write` checks `tcp_write`, :664-673; `do_listen` checks `tcp_listen`, :567-575).
- **Fix:** in words: capture `tcp_connect`'s return in `do_connect`; on error, store it in `conn->err` and post the completion mbox directly, exactly as the early-allocation-failure exits of the same function already do (:462-466).
- **Benefit:** connection attempts under memory pressure fail with `ERR_MEM` instead of deadlocking the connecting module.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ code; INFERRED only on which exhaustion interleaving a given workload hits first)

### CAND-T3-6 — `do_bind` is the only API handler that does not NULL-check its PCB allocation
- **Category:** 6
- **Locations:** `iop/SMSTCPIP/src/api_msg.c:389-435` — UDP branches `:393-405`, TCP branch `:409-410`, first dereferences `udp_setflags`/`udp_recv` (:394-395, :398-399, :404), `setup_tcp` → `tcp_arg` macro (`iop/SMSTCPIP/include/lwip/tcp.h:62`: `pcb->callback_arg = (arg)`), and `udp_bind`/`tcp_bind` at :423/:428; contrast the checked twins `do_newconn` (`api_msg.c:293-325`) and `do_connect` (`api_msg.c:462-497`)
- **Problem:** When `msg->conn->pcb.tcp == NULL` — reachable for a TCP conn whose PCB was NULLed by `err_tcp` (`api_msg.c:183`) and then re-bound, the designed re-bind path — `do_bind` calls `udp_new()`/`tcp_new()` and uses the result without the NULL check both sibling handlers have. On PCB-pool exhaustion (`MEMP_NUM_TCP_PCB`/`MEMP_NUM_UDP_PCB` = 15 each) the very next statement writes through NULL (`tcp_arg`), crashing the tcpip thread and with it the whole stack.
- **Fix:** in words: copy the `do_newconn`/`do_connect` guard shape into `do_bind`: on NULL, set `conn->err = ERR_MEM`, post the mbox, and return.
- **Benefit:** bind under PCB exhaustion returns `ERR_MEM` instead of killing the tcpip thread.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ for the missing check and the dereference chain; INFERRED for the re-bind-after-error reachability)

### CAND-T3-7 — Done-queue isochronous cleanup clears the wrong LUT slot, leaving stale IoRequest pointers
- **Category:** 6
- **Locations:** `iop/USBD/src/usbd.c:100-121`, defect `:106` (`memPool.hcIsoTdToIoReqLUT[arg - memPool.hcIsoTdBuf] = NULL` inside the per-TD cleanup loop); correct twin `iop/USBD/src/usbd.c:181-183` (`processDoneQueue_GenTd` clears `[tdListPos - memPool.hcTdBuf]`)
- **Problem:** When an isochronous ED is halted, `processDoneQueue_IsoTd` walks its remaining TDs, frees each one (`freeIsoTd(curTd)`, :102), and completes its IoRequest as `USB_RC_ABORTED` — but the LUT-clear inside the loop re-clears the *outer* TD's slot (already cleared at :76) instead of `curTd`'s. Each walked TD's LUT entry therefore survives, pointing at an IoRequest that has just been called back / freed (`freeIoRequest` zeroes its `m_pThis` token and recycles it). The generic-TD twin function gets the index right; the isochronous copy does not. The stale pointer is dereferenced by any later LUT scan that trusts the entry (e.g. `killEndpoint`'s LUT sweep in `hub.c:216-237`, which matches on `req->correspEndpoint`).
- **Fix:** in words: clear the LUT slot indexed by the TD actually being freed (`curTd`), mirroring the generic path line-for-line.
- **Benefit:** removes a dangling-pointer entry from the TD→request map on the halted-isochronous path; the map again means what `killEndpoint` and the done-queue assume it means.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ for the wrong index; INFERRED for which later scan dereferences the stale entry first)

### CAND-T3-8 — USBD host-controller init memsets through an unchecked `AllocSysMemory` result, and the failure is then hidden twice
- **Category:** 6
- **Locations:** `iop/USBD/src/hcd.c:340-341` (`memBuf = AllocSysMemory(…); memset(memBuf, 0, memSize);`); masking layers `iop/USBD/src/hcd.c:488` (`hcdInit` ignores `initHcdStructs`' result and returns 0 at :496) and `iop/USBD/src/usbd.c:265-268` (`_start` ignores `hcdInit` and returns 0, "Init done")
- **Problem:** `initHcdStructs` builds the entire OHCI arena (HCCA, TD/ED pools, device/endpoint/request buffers, descriptor storage — tens of KB by the `usbConfig` arithmetic at `hcd.c:316-338`) from one `AllocSysMemory` call whose result is never tested. If IOP memory cannot satisfy it, `memset` writes the full size through NULL and every subsequent pointer rebasing (:344-356) writes through NULL-derived addresses: init crashes instead of failing. Even if the crash were avoided, neither caller propagates failure — the module loads resident and reports success with no host controller behind it.
- **Fix:** in words: test `memBuf` in `initHcdStructs` and return an error; propagate it through `hcdInit` and make `_start` return failure instead of resident success.
- **Benefit:** memory-starved module load becomes a clean load failure the EE side can see, not a NULL-write crash inside interrupt-adjacent init.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ)

### CAND-T3-9 — SMSMAP `_start` returns resident-end unconditionally over unchecked kernel-object creation
- **Category:** 6
- **Locations:** `iop/SMSMAP/main.c:336-343` (`CreateEventFlag` :338, `CreateMutex` :339, `CreateThread`+`StartThread` :341, `return MODULE_RESIDENT_END` :343)
- **Problem:** The event flag, mutex, and IO-thread handles are stored and used with no validity check, and `_start` declares the module resident regardless. If any creation fails (thread/semaphore exhaustion on a loaded IOP), the module is resident with no IO thread: `smap_module_init` never runs, no netif is ever registered, and the EE side sees a loaded SMAP module that simply never answers — a false-ready driver with no error surface. (The init event is the *only* trigger of `smap_module_init`, via `_smap_io_thread` at `main.c:289-303`.)
- **Fix:** in words: check each creation result in `_start` and return a module-error (non-resident) result if the event flag, mutex, or thread cannot be created/started.
- **Benefit:** a failed SMAP start is visible at load time instead of presenting as a silently dead network interface.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ code; INFERRED for kernel-call failure modes)

### CAND-T3-10 — PHY reset result is discarded and the LINK flag is never cleared: a dead PHY leaves SMAP reporting link-up
- **Category:** 6
- **Locations:** declaration `iop/SMSMAP/smap.c:311` (`int _smap_phy_reset(void)`); discarding caller `iop/SMSMAP/smap.c:692-697` (`SMap_Reset` is `void`, ignores the value); the flag word (`g_SMAP` offset 0) is set only on the asm success path (`smap.c` asm :510-513, `flags |= (1 << 1)`) and by `SMap_Start` (bit 0 only); consumer `iop/SMSMAP/smap.c:904` (`SMap_Send` returns `SMap_Con` only when `SMAP_F_LINK` is clear)
- **Problem:** `_smap_phy_reset` computes a real result (link = 1; timeout/no-link exits return −1/0 — the BMCR-reset poll gives up after 9 retries in the asm) and its success path ORs `SMAP_F_LINK` into `g_SMAP.m_Flags`. No code anywhere — C or asm; the only stores to offset 0 are the two cited — ever *clears* that bit, and `SMap_Reset` throws the result away. So once any reset has seen link, a later reset against a dead/unplugged PHY (the `_smap_reset_queue` path in `main.c:151-169` re-runs `SMap_Reset` precisely when link is suspected lost) leaves the stale LINK bit set: `SMap_Send` keeps accepting frames and returning `SMap_OK` into a dead PHY instead of `SMap_Con`, and the stack's callers lose their only link-down signal.
- **Fix:** in words: have `SMap_Reset` clear `SMAP_F_LINK` before the PHY reset and set it only from `_smap_phy_reset`'s returned link result (or return/propagate the result to the two `SMap_Reset` callers).
- **Benefit:** link state again tracks the PHY; senders get `SMap_Con` on a dead link instead of silent black-holing with a success return.
- **Effort:** S · **Risk:** Low · **Confidence:** INFERRED (C side READ — the `int` return, its discarding, and the :904 consumer; the set-without-clear claim rests on reading the asm flag stores, :510-513 and `SMap_Start`)

### CAND-T3-11 — SMAP transmit: chained-pbuf gather copies `tot_len` bytes into a 1,520-byte static buffer bounded only by the 4,096-byte FIFO credit
- **Category:** 6
- **Locations:** `iop/SMSMAP/smap.c:886-921` — buffer `:888` (`sl_TXBuf[(SMAP_TXMAXSIZE + SMAP_TXMAXTAILPAD + 3) / 4]` = 380 words = 1,520 bytes; `SMAP_TXMAXSIZE` 1514 at :17), the only length gate `:908` (`lTXLen > lpSMap->m_TXFree`, where `m_TXFree` is 4096, `smap.c:708`), gather loop `:914-919` (no per-copy or cumulative bound against `sl_TXBuf`)
- **Problem:** For a chained pbuf, `SMap_Send` linearises the chain into `sl_TXBuf` with `mips_memcpy` per link. The gate that runs first compares the rounded length against the TX *FIFO credit* (4,096), not against the gather buffer's capacity (1,520). Any chain with `tot_len` in (1520, 4096] passes the gate and the gather writes past the static buffer into adjacent `.bss`. No in-tree producer was found that emits >1,514-byte link frames (the netif MTU is 1500, `main.c:250`), so this is latent rather than live — but the function itself enforces the wrong bound, and it is the stack's only TX entry point.
- **Fix:** in words: reject `lPackLen > SMAP_TXMAXSIZE` in `SMap_Send` (return `SMap_TX`) before the gather, so the checked bound is the buffer actually being written.
- **Benefit:** the TX path's safety no longer depends on every present and future caller respecting the MTU; one comparison closes a static-buffer overwrite.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ for the bound mismatch; INFERRED — no in-tree producer of an oversize frame found)

### CAND-T3-12 — `netconn_addr` lacks the PCB NULL check its sibling `netconn_peer` has
- **Category:** 6
- **Locations:** `iop/SMSTCPIP/src/api_lib.c:235-252` (no NULL test, dereferences at :243/:247); sibling guard `api_lib.c:211-233` (:219-227); NULL producer `iop/SMSTCPIP/src/api_msg.c:183` (`err_tcp` sets `conn->pcb.tcp = NULL`); caller `iop/SMSTCPIP/src/sockets.c:861,868` (`lwip_getsockname` → `naddr->addr`)
- **Problem:** `netconn_peer` explicitly returns `ERR_CONN` when the PCB is NULL; `netconn_addr` performs the same dereference family with no check at all. After a TCP connection is aborted (`err_tcp` NULLs the PCB and records the error — `netconn_addr` checks neither), a `getsockname` on the socket walks `&(NULL->local_ip)` and dereferences it in `lwip_getsockname`, crashing instead of returning the recorded error.
- **Fix:** in words: add the same PCB-NULL guard `netconn_peer` already has to `netconn_addr` (return `ERR_CONN`).
- **Benefit:** post-abort `getsockname` returns an error instead of dereferencing NULL in the socket layer.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ for the asymmetry; INFERRED for the abort-then-getsockname ordering)

### CAND-T3-13 — Dead dispatch slot: `do_accept` / `API_MSG_ACCEPT` can never be posted, and would hang its poster if it were
- **Category:** 1
- **Locations:** `iop/SMSTCPIP/src/api_msg.c:589-609` (`do_accept` body: a switch whose only arms `break`, never posts the completion mbox), dispatch table `api_msg.c:719-731`, enum `iop/SMSTCPIP/include/lwip/api_msg.h:55`, dispatch macro `api_msg.h:94`
- **Problem:** Dead code, proven by enumeration: every `msg->type` assignment in the tree was grepped — `api_lib.c` posts NEWCONN (:154), DELCONN (:177), BIND (:275), CONNECT (:307), DISCONNECT (:330), LISTEN (:359), RECV (:441), SEND (:481), WRITE (:510), CLOSE (:568) — `API_MSG_ACCEPT` is assigned nowhere (acceptance runs through `acceptmbox` + `netconn_accept` instead). The slot is therefore unreachable; worse, the stub never posts to `conn->mbox`, so any future poster would deadlock in `sys_mbox_fetch`. The enum value, table entry, and ~20-line stub are pure residue.
- **Fix:** in words: delete the `do_accept` stub and its table slot (keeping the enum/table indices aligned for the remaining handlers), or — if the slot is kept for upstream-shape reasons — make it post the mbox like every other handler.
- **Benefit:** removes a trap slot from the message dispatch and ~20 lines that imply an accept path that does not exist.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ; zero-producer claim is the repo-wide grep above)

### CAND-T3-14 — Dead helper: `_smap_emac3_ready` is defined in the SMAP header and called nowhere
- **Category:** 1
- **Locations:** `iop/SMSMAP/smap.h:60-72` (definition, the file's only definition)
- **Problem:** Dead code, proven by grep: a repo-wide search for `_smap_emac3_ready` returns exactly the definition in `smap.h` and no caller in any `.c`/`.h`/asm block (the TX path open-codes its own EMAC3 check inline in `SMap_Send`'s caller chain instead). As a `static inline` in a header it costs a dead copy in every translation unit that includes `smap.h` and invites the false impression that TX readiness is centrally checked.
- **Fix:** in words: delete the function from `smap.h`.
- **Benefit:** −13 lines; one less phantom readiness check for the next reader of the TX path.
- **Effort:** S · **Risk:** Low · **Confidence:** High (READ; zero-caller claim is the repo-wide grep above)

---

## 3. Checked-clean (dissolved candidates, with reasoning)

**SMSTCPIP — tcp.c / tcp_out.c**
- `tcp_backoff[pcb->nrtx]` (`tcp.c:568`) looks index-unsafe but is arithmetically bounded: `nrtx` is incremented only in `tcp_rexmit` (`tcp_out.c:528`), which `tcp_slowtmr` calls only from the branch where `nrtx != TCP_MAXRTX (12)` and not (`SYN_SENT && nrtx == TCP_SYNMAXRTX`); `tcp_in.c:729` only resets it to 0. Maximum index actually used is 11 into a 13-entry table. Dissolved.
- `tcp_enqueue` merge path ignores `pbuf_header(queue->p, -TCP_HLEN)` (`tcp_out.c:258`): cannot fail here. The merged head pbuf is always `PBUF_RAM` (copy path or the 0-length header pbuf), and `pbuf_header`'s RAM branch (`pbuf.c:291-317`) fails only when a *negative* size move would cross the allocation start; removing the 20-byte header that :207 just added moves the payload forward, which never trips that check. Dissolved.
- Ignored `ip_output` returns (`tcp_out.c:375, 470, 503, 572`): fire-and-forget by design and non-corrupting — in `tcp_output` the segment is linked onto `unacked` after `tcp_output_segment` regardless (:392-406), so a failed transmit is recovered by the normal retransmission timer. Dissolved.
- `tcp_output` window test (:351, :383) and `tcp_output_segment` payload rebase (:456-461): sequence/window arithmetic and header offset are internal invariants established by `tcp_enqueue` (header added and checked at :207); no external value enters. Dissolved.
- `tcp_next_iss` (`tcp.c:909-913`, ISS = 6510 + `tcp_ticks`) is predictable — observed, **not promoted**: predictability is outside this pass's three classes (not an unchecked return, dead code, or queue/segment bound).

**SMSTCPIP — mem.c / memp.c**
- `mem_malloc` fit test (`mem.c:203`) double-counts the header size (requires payload ≥ `size + SIZEOF_STRUCT_MEM`): conservative overallocation, never an underallocation; the split header always fits by the same inequality. Dissolved.
- `mem_realloc` (`mem.c:140-181`) silently returns the original pointer when asked to grow: its sole caller is `pbuf_realloc` (`pbuf.c:262`), which early-returns unless shrinking (`pbuf.c`, `new_len >= tot_len` guard) — the grow path is unreachable as a grow. Dissolved.
- `memp_free` (`memp.c:151-169`) has no double-free guard (contrast USBD's `freeTd`): no double-free path was found among in-scope callers — TCP segment lists are disjoint by construction (`tcp_rexmit` moves `unacked`→`unsent` and NULLs the source, `tcp.c`/`tcp_out.c:518-523`; `tcp_output` moves `unsent`→`unacked` singly, :385-409; `tcp_pcb_purge` frees each list once). Hardening note only; not promoted without a concrete double-free.
- `netconn_new_with_proto_and_callback` error path (`api_lib.c:138-162`) leaks `conn->mbox` when the API-message allocation fails: a bounded resource leak on an already-failing creation, no state corruption. Dissolved for this pass.

**SMSTCPIP — api_lib.c / api_msg.c**
- `netconn_recv` UDP branch dereferences `buf->p` without a NULL test (`api_lib.c:454-456`): the sole poster, `recv_udp` (`api_msg.c:75-105`), posts only after a successful `netbuf` allocation with `p` attached. The TCP branch's apparently unchecked `conn->pcb.tcp->state` (:403) is guarded two lines earlier by the `conn->err != ERR_OK` return (:399) — `err_tcp` sets `err` before NULLing the PCB (:183-186). Dissolved.
- `recv_udp` does not free `p` when `recvmbox == SYS_MBOX_NULL` (`api_msg.c:87-104`): delivery can only occur after bind/connect, and both wrappers create `recvmbox` first (`api_lib.c:265-270, 298-302`); receive callbacks and the API handlers are serialised on the tcpip thread. Dissolved.
- `err_tcp` lacks the `conn != NULL` test its siblings have (`api_msg.c:176-205`): the callback arg is set to the conn by `setup_tcp` before any error can be delivered, and `do_delconn` NULLs the error callback itself before closing/aborting (:356-367), so a NULL-arg delivery has no producer. Dissolved.
- `do_send` ignores `udp_send`'s return (`api_msg.c:622`): the failure mode is a misreported success to `netconn_send`'s caller, with no state corruption — outside this pass's "failure corrupts state" bar. Dissolved.

**SMSTCPIP — sockets.c**
- `getsockopt`/`setsockopt` read `pcb.tcp->…` on UDP sockets (`sockets.c:1004, 1037, 1041, 1197`): not a type confusion for the options actually processed at `SOL_SOCKET`/`IPPROTO_IP` level — `so_options`, `tos`, `ttl` live in the `IP_PCB` common prefix (`iop/SMSTCPIP/include/lwip/ip.h:75-82`) at identical offsets in the TCP, UDP, and raw PCBs; the TCP-specific options (`flags`, `keepalive`) are gated by an explicit `type != NETCONN_TCP` early return (:955, :1148). Dissolved.
- `err_to_errno` (`sockets.c:89-91`) compares the negative `err` against an unsigned `sizeof` expression, so every real error maps to `EIO` instead of its specific errno: a wrong-errno wart, observed and **not promoted** — it corrupts no state and fits none of this pass's three classes.
- `lwip_recvfrom` with a negative `len` (`sockets.c:349-351`): the copy itself is chain-bounded by `netbuf_copy_partial`'s loop (`api_lib.c:91-117` stops at the chain end), so no over-copy occurs; only the returned count is wrong for invalid input. Dissolved.
- `lwip_select`'s `timeout == 0` test (`sockets.c:642`) is a pointer comparison implementing the intended wait-forever branch, after the zero-timeout poll case is handled at :611. Dissolved.

**USBD — TD/ED queues**
- TD-allocation failure handling in `setupControlTransfer` / `setupBulkTransfer` / `setupIsocronTransfer` (`usbio.c:61-256`) is exemplary: partial allocations are freed and the endpoint is re-queued (`enqueueEndpoint`) rather than half-programmed; `freeTd`/`freeIsoTd` (`mem.c:29-60`) additionally carry explicit double-free guards. Checked-clean.
- `addToHcEndpointList` ED-index arithmetic is an exact fit: `TYPE_CONTROL/BULK/ISOCHRON` are 0x3F/0x40/0x41 (`usbdpriv.h:232-234`), interrupt endpoints use tree indices 0–0x1F, and `initHcdStructs` reserves exactly 0x42 ED slots (`hcd.c:331`) whose last three become the control/bulk/isochronous heads (:441-450). No overflow. Dissolved.
- `hcdProcessIntr` done-queue reversal (`hcd.c:239-263`) walks the hardware DoneHead chain without a cycle check, but the second loop range-tests every TD against `hcTdBuf`/`hcIsoTdBuf` before processing it (:255-258), and the chain is written by the OHCI controller, not by any device-supplied data — the same hardware-trust class R7 applied to SMAP buffer descriptors. Dissolved.
- `initHardware`'s reset poll (`hcd.c:306-308`, `while (HcCommandStatus & OHCI_COM_HCR) {}`, with its own `// add timeout stuff` comment) can spin, but init blocks rather than proceeding — a hang, not a false-ready state. Dissolved for this pass.
- Suspected-dead USBD helpers are all live: `removeHcEdFromList` ← `hub.c:266`; `fetchPortElemByNumber` ← `hub.c:513`; `fetchEndpointById` ← `interface.c:130,149`; `doGetDeviceLocation` ← `interface.c:64`. Dissolved by grep.

**SMSMAP — init/register**
- `SMap_Init`'s `dev9RegisterPreDmaCb`/`dev9RegisterPostDmaCb` (`smap.c:710-711`) and `smap_module_init`'s `dev9RegisterIntrCb` (`main.c:273`) have no returns to check: all three are `void` in `iop/SMSDev9/include/dev9.h:26,47,50`. Dissolved.
- `smap_module_init`'s SIF command/DMA retry loops (`main.c:280-285`) are unbounded, but the failure shape is an init thread that never finishes — a hang, not a false-ready state (the EE notification the loops retry is the module's only readiness signal, and it is not sent early). Dissolved for this pass.
- `_smap_interface_init` returning `ERR_OK` unconditionally after `SMap_Start` (`main.c:242-258`): link state is dynamic and this lwIP vintage has no link-state callback; readiness does not depend on link at init. By design. Dissolved.
- `netconn_close` (with its `do_close` handler) has zero in-tree callers, but it is declared public netconn API (`api.h:148`) of a vendored stack; only the provably unpostable `API_MSG_ACCEPT` slot (CAND-T3-13) was promoted from this family. Dissolved.
