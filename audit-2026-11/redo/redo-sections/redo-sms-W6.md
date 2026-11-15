# Redo Audit — SMS IOP Modules (Worker W6)

**Scope:** IOP modules other than SMSTCPIP in Simple-Media-System, clone
`~/workspace/audit-clones-2026-11-13/Simple-Media-System`, pin
`e65ced39a27eee8d996e9257e21594d1cac1c93d` (verified `git rev-parse HEAD`, tree clean, read-only).
Rule applied throughout: LESS, NOT MORE — remove / simplify / consolidate / fix; one mechanism = one candidate, all locations listed.

---

## (a) Coverage note

Line counts by script (`wc -l` over `*.c`/`*.h` per directory). Total scope: **25,194 lines**.

| Directory | Lines | Read fully | Structure-scanned only |
|---|---|---|---|
| iop/SMSCDVD/ | 2,130 | SMSCDVD.c (1,236), SMSCDVD_UDFS.c (728), SMSCDVD.h (100) | Makefile, exports/imports (~66) |
| iop/SMSUMS/ | 2,410 | src/ums_fat.c (1,140), src/ums.c (531), src/ums_driver.c (420), src/ums_cache.c (76) | include/ums.h (via grep), Makefile, exports (~243) |
| iop/USBD/ | 3,371 | src/hcd.c (498), src/hub.c (694), src/interface.c (230), src/driver.c (252), src/usbd.c (270), src/usbio.c (309), src/mem.c (230) | headers, exports, remaining src (~888) |
| iop/SMSSMB/ | 3,894 | src/SMSMB.c (2,002) | src/ps2smb.c (1,857) read as a diff against SMSMB.c (600 diff lines); headers |
| iop/smbman-sms/ | 5,356 | src/smb.c (1,718), src/smb_fio.c (1,169), src/smbman.c (35), src/poll.c, src/sms_path.h | src/smb.h (895, grepped for constants/structs), src/auth.c (88), src/des.c (568), src/md4.c (182), src/sms_cp1251.h (109) — crypto/auth, structure only |
| iop/SMSUdpfs/ | 5,321 | udpfs/src/udpfs_core.c (825), udpfs/src/udprdma.c (804), udpfs/src/udpfs_ioman.c (447), udpfs/src/udpfs_packet.h (244), udpfs/src/udpfs_bd.c (132), udpfs/src/udpfs_fhi.c (135), udpfs/src/main.c (46); ministack/src/*.c (59+91+149+28+73+89); smap/src/smap.c (639), smap/src/xfer.c (287), smap/src/main.c (26) | headers (incl. smap/include/dev9.h, diffed — see CAND-W6-19) |
| iop/SMSDev9/ | 958 | src/ps2dev9.c (874) | include/dev9.h (53) |
| iop/SMSMAP/ | 1,530 | main.c (345); smap.c TX/RX paths (lines 820–1050) | smap.c init/PHY sections, smap.h (110) |
| iop/SMSPWROFF/ | 224 | src/poweroff.c (170) | build files |

**Liveness (how each tree reaches the shipped product), established before claiming anything dead:**

- The EE loads several of these from the packed IOP image whose slots/sizes are defined in `include/SMS_Data.h`:
  PS2DEV9 → iop/SMSDev9 (loaded as module index 1, `src/SMS_IOP.c`), POWEROFF → iop/SMSPWROFF (index 2),
  CDVD → iop/SMSCDVD (loaded by `CDVD_Init`, `src/SMS_CDVD.c:77`), USBD and USB_MASS → iop/USBD and iop/SMSUMS
  (`src/SMS_IOP.c:688` loads the USB_MASS slot), PS2SMAP → iop/SMSMAP (`src/SMS_IOP.c:580`).
- BDM builds embed `irx/*.irx.xz` instead: `usbd.irx`, `smbman.irx` (built from iop/smbman-sms by `tools/build-smbman.sh`),
  and `udpfs_smap/ministack/ioman.irx` (built from iop/SMSUdpfs by `tools/build_udpfs.sh`).
- The packed image still defines an SMB slot (`SMS_SMB_OFFSET`/`SMS_SMB_SIZE`, `include/SMS_Data.h`), but **no EE code
  references `SMS_SMB_OFFSET`** (grep over `src/` and `include/` returns only the header definition), and the SMB path in
  `src/SMS_IOP.c:586-597` loads smbman-sms's `smbman_irx` instead, with a comment saying so. See CAND-W6-4.

---

## (b) Candidates (sorted by benefit)

### CAND-W6-1 — FAT long-filename sequence number trusted into a 128-byte name buffer (SMSUMS)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUMS/src/ums_fat.c:421-441` (`_get_direntry` LFN branch); same trust chain consumed at `iop/SMSUMS/src/ums_fat.c:513` (`_set_dir` `strcpy`); buffer definition `iop/SMSUMS/src/ums_fat.c:21` (`FAT_MAX_NAME 128`), `:124`, `:137`
- **Problem:** For a FAT long-name directory entry, the write offset is computed from the on-disc sequence byte:
  `lOff = ((apLong->m_EntrySeq & 0x3F) - 1) * 13;` — up to 62×13 = **806** — and 13 name characters per entry are then
  written at `apDir->m_Name[lOff++]` into `fat_direntry.m_Name[128]`. The sequence number is never checked against the
  buffer (a legitimate LFN chain needs at most 20 entries, offset ≤ 247 — itself already past 128 for names > 128 chars,
  which this driver truncates nowhere). A crafted USB stick's directory sector therefore writes hundreds of bytes past a
  fixed struct during ordinary directory listing. `_set_dir` then `strcpy`s the assembled name onward unbounded.
  SMSUMS is a live module (packed USB_MASS slot; `src/SMS_IOP.c:688`).
- **Fix:** Clamp in `_get_direntry`: reject/skip an LFN entry whose computed `lOff + 13` exceeds `FAT_MAX_NAME - 1`
  (treat as end of chain), and bound the `_set_dir` copy to the destination size. No new mechanism — one range check at
  the point the disc byte is first trusted.
- **Benefit:** Removes a remotely craftable (malicious/corrupt media) IOP memory-corruption in a shipped module.
- **Effort:** Small (two bounded checks in one file). **Risk:** Low — over-long names are already unrepresentable in a 128-byte field. **Confidence:** READ.

### CAND-W6-2 — ISO9660 filename length trusted past a 129-byte TOC buffer (SMSCDVD)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSCDVD/SMSCDVD.c:1085-1091` (`TocEntryCopy`), `iop/SMSCDVD/SMSCDVD.c:858-863` (`_copy_name`); destination `iop/SMSCDVD/SMSCDVD.h:96` (`char m_Filename[129]`)
- **Problem:** In the non-Joliet branch both copies do `strncpy(dst, src, 128)` and then terminate with
  `dst[lFNameLen] = 0;` where `lFNameLen` is the directory record's own filename-length byte (up to 255). For any name
  longer than 129 bytes the terminator write lands past the buffer (up to 126 bytes past `TocEntry.m_Filename`, inside
  the TOC array / RPC buffer). The Joliet branch is safe (length halved, ≤127). Directory records are disc-controlled;
  SMSCDVD is live (packed CDVD slot, loaded by `src/SMS_CDVD.c:77`). Related: the directory-walk loops
  (`SMSCDVD.c:525-527`, `:565-567`, `:713-715`) advance by the record's `m_Length` and only special-case 0; a length
  smaller than the fixed record header is never rejected before `TocEntryCopy` dereferences the fields.
- **Fix:** Terminate at the copied length, not the claimed length: after the 128-byte `strncpy`, write `dst[128] = 0`
  (and use the truncated length for the `strtok` step). Optionally reject records with `m_Length` below the header size
  in the three walk loops.
- **Benefit:** Removes a crafted-disc IOP overflow in the module every disc browse goes through.
- **Effort:** Small. **Risk:** Low — names >128 chars are already truncated by the `strncpy`. **Confidence:** READ.

### CAND-W6-3 — UDPFS `_request` callers never check the reply actually arrived in full (SMSUdpfs)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUdpfs/udpfs/src/udpfs_core.c:166-171` (`_path_op`), `:373-383` (open), `:519-532` (lseek), `:622-642` (getstat), `:582-587` (dread name copy); helper at `:72-80`
- **Problem:** `_request()` returns the received byte count, but every caller checks only `ret < 0` and then reads the
  full reply struct out of the shared static `g_rx_buf`. A short reply (truncated datagram, version-skewed server)
  leaves the tail of the previous reply in place, and the caller acts on it: open returns a stale `handle`, lseek
  returns a stale position, getstat copies stale timestamps/sizes to the EE. `_path_op` is worse — it checks neither
  length nor `msg_type` and returns `reply->result` verbatim for mkdir/remove/rmdir. In dread, `name_len` is checked
  against the caller's `name_max` but not against the bytes actually received (`ret - sizeof(reply)`), so stale buffer
  bytes are copied out as a filename; and `name_len >= name_max` silently returns success (1) with an empty name.
  This is exactly the failure family the same file already fixed for the read path in `_recv_with_result`
  (`udpfs_core.c:89-136`, with an explanatory comment) — the fix pattern exists in-tree and was not applied here.
- **Fix:** One check in `_request` (or at each call site): fail with `-EIO` when `ret` is smaller than the reply struct
  the caller is about to read; in dread additionally require `name_len <= ret - sizeof(udpfs_msg_dread_reply_t)`.
  Reuses the file's own established pattern; adds no new mechanism.
- **Benefit:** Closes the remaining stale-data path in the live network filesystem; a misbehaving server currently
  produces wrong handles/stats instead of a clean error.
- **Effort:** Small. **Risk:** Low — a conforming server always sends full replies. **Confidence:** READ.

### CAND-W6-4 — The whole legacy SMSSMB tree is dead and internally duplicated — delete it
- **Category:** 1 (dead code) / 2 (duplication)
- **Locations:** `iop/SMSSMB/` (3,894 lines); `iop/SMSSMB/src/SMSMB.c` (2,002) vs `iop/SMSSMB/src/ps2smb.c` (1,857); `iop/SMSSMB/Makefile` (builds only `SMSMB.o` → `bin/smb.irx`); packed-image slot definition `include/SMS_Data.h` (`SMS_SMB_OFFSET`/`SMS_SMB_SIZE`)
- **Problem:** Three layers of waste. (1) `ps2smb.c` is a near-copy of `SMSMB.c` in the same directory (600 diff lines:
  packed attributes, `mips_memcpy` vs `memcpy`, flag values, one DES/name-conversion delta) and is not referenced by
  the module's own Makefile — unbuilt duplicate. (2) The built product, `smb.irx`, is not what SMS ships: the EE's SMB
  path loads smbman-sms's `smbman_irx` (`src/SMS_IOP.c:586-597`, comment states smbman binds to the SMSTCPIP stack
  instead), `tools/build-smbman.sh` builds iop/smbman-sms, and no EE code references the packed `SMS_SMB_OFFSET` slot
  anymore. (3) The tree carries its own full set of unpatched SMB1 parsing bugs (see CAND-W6-8), so its continued
  presence is a maintenance and cherry-pick hazard: fixes land in smbman-sms and this copy silently diverges.
- **Fix:** Delete `iop/SMSSMB/` outright (and, if the packed IOP image is ever regenerated, drop the SMB slot from
  `include/SMS_Data.h`). This is the LESS fix for CAND-W6-8 as well: those bugs do not need patching in a module
  nothing loads.
- **Benefit:** −3,894 lines, one whole SMB client removed from the audit/maintenance surface, no duplicate left to
  diverge.
- **Effort:** Small (deletion + confirming no build script references it; `tools/` and the top Makefile do not).
  **Risk:** Low — hinges on the liveness evidence above; if a non-BDM legacy build variant is ever resurrected it can
  be recovered from git history. **Confidence:** READ.

### CAND-W6-5 — USB device geometry (sector size / LBA) trusted unchecked, including divide-by-zero (SMSUMS)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUMS/src/ums_driver.c:312-313` (`UmsGetCapacity`); divisions by the value at
  `iop/SMSUMS/src/ums_driver.c:395`, `iop/SMSUMS/src/ums_cache.c:40`, `iop/SMSUMS/src/ums_fat.c:226-227` (`_root_dir_sectors`),
  `iop/SMSUMS/src/ums_fat.c:236` and `:395` (`m_SectorSize >> 5` / `m_SectorSize / 32` as divisors)
- **Problem:** `UmsGetCapacity` stores the READ CAPACITY response verbatim: `m_MaxLBA = _int_be(&lBuf[0])`,
  `m_SectorSize = _int_be(&lBuf[4])` — no range check at all. A device reporting sector size 0 makes every division
  above a divide-by-zero (IOP exception); a size below 32 makes the `>> 5` / `/ 32` divisors zero the same way; a huge
  or non-power-of-two size desynchronises every sector computation downstream. `FATFS_Init` does whitelist the **BPB's**
  sector/cluster sizes (`ums_fat.c:1048-1061`) but never reconciles them with the device-reported size, and allocates
  its BPB buffer as `sizeof(FATFS) + lSecSize` from the unvalidated value (`ums_fat.c:1002`).
- **Fix:** Validate once, at the source: in `UmsGetCapacity`, accept only 512/1024/2048/4096 (the same whitelist
  `FATFS_Init` already uses for the BPB) and fail the capacity call otherwise. One check at the trust boundary; all
  downstream divisions become safe by construction.
- **Benefit:** Removes a crafted-device IOP crash class and a geometry-desync class in a live module.
- **Effort:** Small. **Risk:** Low — non-standard sector sizes are already rejected at the BPB layer. **Confidence:** READ.

### CAND-W6-6 — IOP init failures masked by unconditional success returns (family, all modules)
- **Category:** 6 (bugs/security)
- **Locations:**
  - USBD: `iop/USBD/src/hcd.c:340-341` (`AllocSysMemory` result `memset` with no NULL check), `iop/USBD/src/hcd.c:472,482,491` (`CreateThread` unchecked; `initHubDriver()` return ignored — it does check its own allocation and returns −1, `iop/USBD/src/hub.c:44-48`), `iop/USBD/src/usbd.c:263-265` (`CreateSema` unchecked, `hcdInit()` return ignored, `_start` returns 0), `iop/USBD/src/driver.c:246-247` (callback `CreateThread`/`StartThread` unchecked)
  - SMSUMS: `iop/SMSUMS/src/ums.c:125-133` (`CreateMutex` ×2 and `CreateThread` unchecked, `StartThread` unconditional, `_ums_connect` returns 0)
  - smbman-sms: `iop/smbman-sms/src/smb_fio.c:228-244` (`CreateMutex` ×2 + `CreateThread` unchecked, `StartThread` unconditional, `smb_init` returns 0), `iop/smbman-sms/src/smbman.c:32` (`_start` ignores `smb_initdev()`'s failure return and always returns `MODULE_RESIDENT_END`)
  - SMSUdpfs: `iop/SMSUdpfs/udpfs/src/main.c:28-45` (`udpfs_init()`/`udpfs_bd_init()`/`fhi_init()` returns ignored, always `MODULE_RESIDENT_END`), `iop/SMSUdpfs/udpfs/src/udpfs_ioman.c:444` (`AddDrv` result ignored, returns 0)
  - SMSMAP: `iop/SMSMAP/main.c:338-341` (`CreateEventFlag`/`CreateMutex` unchecked; `StartThread(CreateThread(...))` inline and unchecked)
  - SMSPWROFF: `iop/SMSPWROFF/src/poweroff.c:166` (`StartThread(s_ThreadID = CreateThread(...))` unchecked)
  - SMSCDVD: `iop/SMSCDVD/SMSCDVD.c:1186` (directory-cache `AllocSysMemory` stored with no NULL check before use)
- **Problem:** One mechanism repeated across seven modules: a thread/semaphore/allocation creation result is never
  tested, the object is used (or started) unconditionally, and the enclosing init reports success / resident-end
  regardless. On IOP memory exhaustion or thread-table pressure the result is a NULL dereference (hcd.c:341), a
  `StartThread` on an invalid id, or — worst for diagnosis — a module that loads "successfully" with no worker thread,
  no device registered, or no hub driver, failing later in unrelated-looking ways. The contrast cases exist in-tree:
  `hub.c:44-48` checks, `SMSCDVD.c:1224-1234` checks its `CreateThread`, `ums.c`'s `_connect_thread` checks its cache
  allocation — the checks were simply not propagated to the callers/peers.
- **Fix:** Per site, the smallest change: test the result and return the module's existing failure value
  (`MODULE_NO_RESIDENT_END`, −1, or the function's error return) instead of proceeding. No new abstractions — just
  stop discarding results the APIs already return.
- **Benefit:** Converts silent half-dead modules and NULL derefs into clean load failures across the whole IOP set.
- **Effort:** Medium (many sites, each trivial). **Risk:** Low. **Confidence:** READ.

### CAND-W6-7 — smbman-sms: two query replies parsed with none of the validation its sibling functions apply
- **Category:** 6 (bugs/security)
- **Locations:** `iop/smbman-sms/src/smb.c:908-930` (`smb_QueryPathInformation`), `iop/smbman-sms/src/smb.c:799-838` (`smb_QueryInformationDisk`)
- **Problem:** `smb_QueryPathInformation` casts `&SMB_buf.smb.u8buff[QPIRsp->smbTrans.DataOffset]` to
  `BasicFileInfo_t`/`StandardFileInfo_t` and reads the fields without ever checking `DataOffset`, `DataCount`, or the
  received length `r` — a short or hostile server reply makes it read stale buffer contents as file times/size and
  report success. `smb_QueryInformationDisk` likewise checks only `r <= 0` and the status word before reading
  `TotalUnits`/`BlocksPerUnit`/`BlockSize`/`FreeUnits` from a possibly truncated reply. The same file demonstrates
  the intended standard elsewhere: `smb_NetShareEnum` validates offsets/counts (`smb.c:739-795`),
  `smb_FindFirstNext2` validates parameter/data bounds (`smb.c:1540-1581`), `smb_ReadAndX` validates wire length
  (`smb.c:1127-1151`). These two functions are the unpatched remainder, in the live SMB client.
- **Fix:** Apply the file's own existing pattern: require `r` to cover the header plus the info struct, and
  `DataOffset + sizeof(*FileInfo) <= r`, before dereferencing. No new helpers.
- **Benefit:** Brings the last two unvalidated reply parsers in the shipped SMB client up to the file's own standard.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### CAND-W6-8 — Legacy SMB client trusts server offsets and unbounded string copies (only relevant if SMSSMB is kept)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSSMB/src/SMSMB.c:1127,1131` (`SMB_FindFirst`), `:1193,1194` (`SMB_FindNext`), `:1305,1310` (`SMB_ShareEnum`), `:813-815,834-837` (`SMB_Negotiate`), `:1400-1404` (`_DrvDOpen`), `:1041-1046` (`SMB_Mount`), `:964-1000` (`SMB_Login`), `:1117` (`SMB_Open`), `:1232` (`SMB_FindFirst` path copy), `:1774` (LOGOUT loop)
- **Problem:** If CAND-W6-4 (deletion) is not taken, this is the blocker list. FindFirst/FindNext/ShareEnum cast
  `&lPkt[SVAL(lPkt,45)+4]` and `&lPkt[SVAL(lPkt,51)+8]` to parameter/data structs with no check of the server-supplied
  offsets against the received length. `SMB_Negotiate` uses the server's `m_EncrKeyLen` (u16) to form
  `&lBuff[65 + m_EncrKeyLen]` as a `strncpy` source into `m_PDom[80]`. Packet assembly uses unbounded
  `strcpy`/`strcat` into fixed packet buffers from EE-supplied server/share/user/password/path strings (locations
  above). `_DrvDOpen` does `strcpy(s_SEnumBuff, apName)` into the 8,192-byte static (`SMSMB.c:311`) and then reads
  `s_SEnumBuff[lLen-1]`, which underflows to index −1 for an empty name. Separately, the LOGOUT handler's inner loop
  at `:1774` indexes `m_FileCtx[i]` with the **mount** index `i` instead of the loop variable `j`, so logout closes
  the same file context repeatedly and leaks the rest.
- **Fix:** Preferred: CAND-W6-4 (delete). If the tree is deliberately revived, each site needs the received-length
  checks smbman-sms already implements, bounded copies, and `m_FileCtx[j]` at :1774.
- **Benefit:** Documents why the tree cannot simply be switched back on; the LOGOUT index bug is a plain logic error
  independent of the security items.
- **Effort:** Large if patched (another reason deletion wins). **Risk:** —. **Confidence:** READ.

### CAND-W6-9 — smbman-sms `smb_Echo` copies an EE-supplied length into the SMB buffer unchecked
- **Category:** 6 (bugs/security)
- **Locations:** `iop/smbman-sms/src/smb.c:1648-1664` (`smb_Echo`); caller `iop/smbman-sms/src/smb_fio.c:1094-1097` (`smb_EchoServer`, devctl `SMB_DEVCTL_ECHO`)
- **Problem:** The devctl path passes `echo->len` from the EE request straight into `smb_Echo`, which does
  `memcpy(&ER->ByteField[0], echo, (u16)len)` into the shared SMB packet buffer (`MAX_SMB_BUF 4096` + `MAX_SMB_BUF_HDR 128`,
  `iop/smbman-sms/src/smb.h:892-893`) with no cap. A len above the remaining buffer space overruns `SMB_buf` into
  adjacent statics; the `(u16)` cast also silently truncates larger values so the sent `ByteCount` and the copied
  count can disagree with the caller's intent. The internal keepalive caller uses a fixed 18 bytes and is safe — the
  exposure is only the devctl entry, which validates neither `len` nor the devctl argument length.
- **Fix:** Reject `len` greater than the space actually available in the echo request buffer (one comparison in
  `smb_Echo`, covering both callers).
- **Benefit:** Removes an EE→IOP overflow in the live SMB module's only arbitrary-length copy.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### CAND-W6-10 — `UmsWarmUp` loops over a device-supplied LUN count past a 16-entry name table (SMSUMS)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUMS/src/ums_driver.c:214-216` (loop), `:232-248` (writes); table `iop/SMSUMS/include/ums.h:54` (`char m_LUName[16][19]`)
- **Problem:** `lnLUN = UmsGetMaxLUN(apDev)` is the device's own GET MAX LUN byte (0–255), and the warm-up loop runs
  `for (i = 0; i <= lnLUN; ++i)` writing `m_LUName[i]` — a 16-row array. A device reporting more than 15 LUNs writes
  INQUIRY names past the array into the rest of the `USBMDevice` struct. (Secondary: at `:234`,
  `lLen = strlen(m_LUName[i]) - 1` is −1 for an all-zero name, and the dedup suffix write at `:247-248` would then
  touch index −1 if a duplicate were found.)
- **Fix:** Clamp `lnLUN` to 15 where it is read (`UmsWarmUp`), matching the fixed table size; no other site uses the
  unclamped value for indexing.
- **Benefit:** Removes a crafted-device struct overwrite in a live module.
- **Effort:** Small. **Risk:** Low — >16-LUN devices cannot be represented in the existing table anyway. **Confidence:** READ.

### CAND-W6-11 — Path component copied unbounded into a 257-byte stack buffer (SMSUMS)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUMS/src/ums_fat.c:605-626` (`_file_start_cluster`; buffer at `:607`, copy at `:623`)
- **Problem:** Path components are accumulated with `lName[lOff++] = apName[i++]` and no bound; a single path
  component longer than 256 bytes (long filename handed down from the EE/browser) overruns the stack buffer
  `lName[257]`. FAT itself limits a component to 255, but nothing enforces that before the copy.
- **Fix:** Stop with `-ENAMETOOLONG` when `lOff` reaches 256 — the function already returns `-ENOENT`-style errors
  on failure paths, so this is one comparison in the existing loop.
- **Benefit:** Removes a stack overflow reachable with an ordinary (if long) filename.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ.

### CAND-W6-12 — `FindPath` computes a subdirectory's sector count from the root directory's size (SMSCDVD)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSCDVD/SMSCDVD.c:794`
- **Problem:** On descending into a subdirectory, `CachedDirInfo.m_SectorNum = (localTocEntry.m_FileSize >> 11) + ((CDVolDesc.m_RootToc.tocSize & 2047) != 0);`
  — the round-up term tests the **root** directory's size remainder instead of the subdirectory's own
  (`localTocEntry.m_FileSize & 2047`). Copy-paste error: subdirectories whose size is an exact 2048 multiple get a
  phantom extra sector when the root's isn't (cache reads one sector too far), and vice versa lose their final
  partial sector (last entries invisible). 
- **Fix:** Use `localTocEntry.m_FileSize & 2047` in the round-up term.
- **Benefit:** Correct directory caching for all non-root directories; one-word fix.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### CAND-W6-13 — UDF descriptor lengths trusted: allocation-descriptor offset and name decode unbounded (SMSCDVD)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSCDVD/SMSCDVD_UDFS.c:293-345` (`UDFFileEntry`), `iop/SMSCDVD/SMSCDVD_UDFS.c:84-105` (`UDFDecode`); callers `:138`, `:399`
- **Problem:** `UDFFileEntry` reads `lLEA`/`lLAD` (u32s from the disc at offsets 168/172) and forms
  `p = 176 + lLEA`, then reads allocation descriptors at `&apData[p]` with no check that `p` (or the descriptor)
  lies inside the 2,048-byte sector buffer `apData` points into; a large `lLEA` reads out of bounds (and the
  `176 + lLEA + lLAD` sum can wrap). `UDFDecode` writes `apDst[i++]` for the full source length `aLen` — the
  destination size is not a parameter and is never checked; at `:399` `aLen` is the disc-supplied `lLFI` (file
  identifier length). Secondary inconsistency: `UDF_DOpen` returns `1` (`SMSCDVD_UDFS.c:552`) and `UDF_DClose`
  returns `1L` (`:619`) where the ISO paths return 0 on success (impact INFERRED — depends on how the EE/ioman
  layer treats a positive dopen result).
- **Fix:** Bound `p + <AD size> <= 2048` before each `UDFAD` call and pass the destination capacity into
  `UDFDecode` (or clamp `aLen` at both call sites to the known destination sizes). Align the two return values
  with the ISO side.
- **Benefit:** Removes crafted-disc OOB read/write in the UDF half of the live CDVD module.
- **Effort:** Small. **Risk:** Low. **Confidence:** READ (returns: READ; impact INFERRED).

### CAND-W6-14 — USBD exports three stubs that report success (USBD)
- **Category:** 8 (half-finished)
- **Locations:** `iop/USBD/src/interface.c:216-229`
- **Problem:** `UsbRegisterAutoloader`, `UsbUnregisterAutoloader` and `UsbChangeThreadPriority` are exported library
  functions whose entire body is a `dbg_printf("... stub")` and `return 0` (success). Any driver that registers an
  autoloader through this USBD build is silently never autoloaded, with no error to diagnose from.
- **Fix:** Either implement registration (the autoloader list mechanism is the driver's own design) or return a
  negative error so callers can tell the feature is absent. Returning an error is the LESS change and matches the
  stub's reality.
- **Benefit:** Converts a silent no-op success into an honest failure for dependent drivers.
- **Effort:** Trivial (if error return chosen). **Risk:** Low — a caller that depended on the fake success was
  already not getting the feature. **Confidence:** READ.

### CAND-W6-15 — SMSMAP TX gather bounded by the FIFO budget, not by the gather buffer (INFERRED)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSMAP/smap.c:886-921` (`SMap_Send`); buffer `iop/SMSMAP/smap.c:888`, sizes `iop/SMSMAP/smap.c:17-18` (`SMAP_TXMAXSIZE 1514`, tail pad 4); budget `iop/SMSMAP/main.c:165` / `smap.c:708` (`m_TXFree = 4096`)
- **Problem:** For a chained pbuf, `SMap_Send` gathers the whole chain into the static `sl_TXBuf` (1,518 bytes)
  with `mips_memcpy` per link (`smap.c:914-919`). The only length gate is `lTXLen > m_TXFree` (`smap.c:908`), where
  `m_TXFree` is the 4,096-byte TX-FIFO budget — so a chain with `tot_len` between 1,519 and 4,096 passes the gate
  and overruns the static buffer. Reachability depends on whether the SMSTCPIP stack (out of scope here) can hand
  linkoutput a chain longer than the 1,500 MTU — normal traffic cannot, which is presumably why this has never
  fired; the bound is nonetheless the wrong constant.
- **Fix:** Gate the gather on `sizeof(sl_TXBuf)` (return `SMap_TX` above it), keeping the FIFO check for the
  FIFO. One constant change.
- **Benefit:** Makes the gather buffer's bound self-evidently correct instead of incidentally safe.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** INFERRED (code READ; reachability depends on out-of-scope stack behaviour).

### CAND-W6-16 — `smb_GetShareList` spends one full NetShareEnum RPC per share (smbman-sms)
- **Category:** 5 (performance, structural)
- **Locations:** `iop/smbman-sms/src/smb_fio.c:1015-1035`
- **Problem:** Share listing does one NetShareEnum to count shares, then **one additional full NetShareEnum round
  trip per share** (`smb_NetShareEnum(..., i, 1)` in the loop) to fetch entries one at a time into a single
  `ShareEntry_t`. Enumeration cost is O(n) network round trips on the IOP↔server path for what the first call's
  response format already pages through a caller buffer.
- **Fix:** Fetch into a bounded local array in as few enum calls as the existing `smb_NetShareEnum` buffering
  allows (it already accepts an index/count window), then DMA the entries out. No protocol change.
- **Benefit:** Share browsing on large servers drops from n+1 round trips to a small constant.
- **Effort:** Medium. **Risk:** Medium (touches the EE DMA contract for the list) — worth doing only if share
  browsing latency is a felt problem. **Confidence:** READ.

### CAND-W6-17 — UDPFS SMAP PHY-reset retry bound fires on the first poll (INFERRED)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSUdpfs/smap/src/smap.c:137-147` (`InitPHY`)
- **Problem:** The reset-wait loop is `for (i = 0; read & RST; i++) { if (i <= 0) return -1; ... DelayThread(1000); }`
  — the guard `i <= 0` is true on the very first iteration, so the loop can only succeed if the reset bit has
  *already* cleared before the first read; it can never actually wait. Either the bit reliably clears that fast
  (making the check dead weight) or PHY reset spuriously fails on slower hardware. A retry cap was clearly
  intended (`i` is counted and a delay exists) but written as a bound that cannot function.
- **Fix:** Change the guard to a real cap (e.g. `i > 1000` with the existing 1 ms delay ≈ 1 s), matching the
  loop's evident intent.
- **Benefit:** PHY init either works as intended on slow-to-reset hardware or the dead check is honest.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** INFERRED (code READ; hardware timing not verifiable in this environment).

### CAND-W6-18 — `FATFS_Init` leaks its filesystem allocation on two early returns (SMSUMS)
- **Category:** 4 (size/memory)
- **Locations:** `iop/SMSUMS/src/ums_fat.c:1002` (allocation), `:1043` and `:1044-1046` (early returns), `:1131` (the free the early returns bypass)
- **Problem:** `lpFS` (`sizeof(FATFS) + lSecSize`) is allocated at :1002. The bad-signature return (:1043) and the
  zero-total-sectors return (:1044-1046) `return retVal` directly, skipping the `if (!retVal) FreeSysMemory(lpFS)`
  cleanup at :1131 that the other failure paths (BPB whitelist mismatch) do reach. Every mount attempt against a
  non-FAT or corrupt stick leaks the allocation; media is removable, so attempts repeat.
- **Fix:** Route both returns through the existing cleanup (drop the early `return`s, let control fall to :1131).
- **Benefit:** Removes a per-mount-attempt IOP memory leak.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### CAND-W6-19 — `dev9.h` duplicated verbatim in two IOP trees
- **Category:** 2 (duplication)
- **Locations:** `iop/SMSDev9/include/dev9.h` and `iop/SMSUdpfs/smap/include/dev9.h` (53 lines each; `diff` — identical)
- **Problem:** The DEV9 ABI header exists twice with no note in either copy that the other exists; an ABI change
  made in one tree silently desynchronises the UDPFS SMAP driver from the DEV9 driver it registers callbacks with.
  Caveat: the duplication is partly load-bearing — the UDPFS build deliberately shadows the SDK's dev9 header with
  the v1.1 ABI (see `tools/build_udpfs.sh` / `tools/build_ata_bd_dev9v11.sh`), so a naive "delete one copy" breaks
  the include-order trick.
- **Fix:** Keep both files (the shadowing is intentional) but make the pair self-maintaining: a one-line comment in
  each copy naming the other and requiring both to change together. LESS than any build-system consolidation.
- **Benefit:** Prevents a silent ABI skew between two drivers that share hardware callbacks.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** READ.

### CAND-W6-20 — SMSDev9 exported callback registration indexes fixed arrays unchecked (INFERRED)
- **Category:** 6 (bugs/security)
- **Locations:** `iop/SMSDev9/src/ps2dev9.c:170-186` (`dev9RegisterIntrCb` → `dev9_intr_cbs[7]`, `dev9RegisterPreDmaCb`/`PostDmaCb` → arrays of 2), `:301,309` (`dev9DmaTransfer` indexes the DMA callback arrays with its `ctrl` argument)
- **Problem:** These are library exports callable by any loaded IOP module. `intr`, `aDev`, and `ctrl` are used as
  raw array indices with no range check, so an out-of-range argument writes a function pointer outside the tables
  (registration) or calls through an out-of-bounds slot (DMA). All in-tree callers pass constants (SMAP registers
  2–6 / channel 1), so this is a robustness gap in an exported interface rather than a live in-tree bug.
- **Fix:** Range-check the three registration functions and `ctrl` in `dev9DmaTransfer` (ignore/return on
  out-of-range) — four comparisons total.
- **Benefit:** Hardens the DEV9 export surface every network/HDD driver depends on.
- **Effort:** Trivial. **Risk:** Low. **Confidence:** INFERRED (code READ; no in-tree caller misuses it).

---

## (c) Checked-clean notes

- **smbman-sms is largely hardened already** — verified, do not re-flag: `GetSMBServerReply` received-size checks
  (`smb.c:296-306`); `setStringField` capacity logic (`smb.c:313-337`); `smb_NetShareEnum` full offset/count
  validation (`smb.c:739-795`); `smb_FindFirstNext2` parameter/data validation (`smb.c:1540-1581`);
  `smb_ReadAndX` wire-length validation (`smb.c:1127-1151`); `sms_join_path` bounded with no silent truncation
  (`sms_path.h`); `smb_dread` −EILSEQ skip-advance handling (`smb_fio.c:713-727`). CAND-W6-7/-9 are the remainder,
  not the pattern.
- **SMSUdpfs is SMS-hardened by design** — the in-code comments document fixes that must not be re-flagged as
  missing: `_recv_with_result` zero-initialised header + result clamp (`udpfs_core.c:89-136`); UDPRDMA wire
  `hdr_size` clamp-and-drain (`udprdma.c:186-215`); RX mutex serialisation and discover-time RX-state reset
  (`udprdma.c:488-530`); bounded TX wait replacing an infinite loop (`smap/src/xfer.c:254-287`);
  `smap_register_rx_callback` header-size clamp (`xfer.c:94-99`) and `HandleRxIntr` `hdr_len` clamp
  (`xfer.c:177-185`); single-socket-for-life is a documented consequence of ministack having 4 UDP slots and no
  unbind (`udpfs_core.c:267-303`, `udprdma.c:471`).
- **SMSUdpfs ministack `udptty.c` is deliberately unbuilt**, with the reason documented at
  `ministack/src/main.c:59-65` (it would `close(0)/close(1)` and hang if the tty reopen missed fds 0/1; kept
  in-tree for upstream re-sync). Intentional — not a dead-code finding.
- **SMSCDVD checks that do exist:** `CDVD_Thread`'s RPC buffer allocation is NULL-checked with a sleep fallback
  (`SMSCDVD.c:975-977`), and `_start` checks its `CreateThread` result (`SMSCDVD.c:1224-1234`) — the unchecked
  cache allocation at `:1186` (CAND-W6-6) is the exception, not the norm, in this file.
- **USBD `initHubDriver` checks its own allocation** and returns −1 (`hub.c:44-48`); the defect is in its caller
  (CAND-W6-6), not here.
- **SMSUMS `_connect_thread` checks its 128 KiB cache allocation** (`ums.c`) — contrast `_ums_connect`
  (CAND-W6-6). `FATFS_Init`'s BPB sector/cluster whitelist (`ums_fat.c:1048-1061`) and its fall-through
  `FreeSysMemory` (`:1131`) both exist; CAND-W6-5/-18 are about the gaps around them, not their absence.
- **SMSSMB `_smb_file_info_dos2ps2` name copy is NOT a bug:** `m_NameLen` is an `unsigned char`
  (`SMSMB.c:105`), so `strncpy` + `name[lLen] = 0` at `SMSMB.c:673-674` stays inside `io_dirent_t.name[256]`.
  Checked and cleared.
- **SMSMAP RX path is allocation-guarded:** the BD length flows into `pbuf_alloc` and failure is handled
  (`smap.c:1014-1022`); there is no raw copy of a wire length. (The TX gather is the asymmetric case, CAND-W6-15.)
- **SMSMAP and SMSUdpfs SMAP are different drivers** (~1,691 diff lines between the two `smap.c` files) — not
  duplication, do not consolidate.
- **SMSDev9** is otherwise register-level boilerplate; `dev9_ioctl` returning 0 for unknown commands is its
  normal default, and the PCMCIA tuple walk (`ps2dev9.c:613-663`) is end-checked.
- **Licences** are settled per instructions and were not flagged anywhere in scope.
