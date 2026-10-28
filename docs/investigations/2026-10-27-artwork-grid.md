# Optional browser artwork grid (#26)

The first implementation uses the existing browser's filtered media list and
actions. R2 switches between its default list and a three-column, two-row grid.
Mode is kept only for the current application session. Opening folders and
returning to a parent retains the selected filename through the existing level
stack; grid rendering recomputes the page containing that selection. Opening a
file and the existing Play-all menus use the same media paths as the list.

## Lookup and budgets

- Sidecars are in the media file's own directory: `Movie.avi` → `Movie.jpg`,
  then `Movie.jpeg`. Lookup uses the unfiltered directory listing, compares
  case-insensitively, and opens that listing's actual filename. `.jpg` takes
  precedence. No speculative remote path probes or recursive artwork scans.
- Video, audio and playlist tiles can use sidecars. Folders, shares, partitions
  and other files retain their ordinary icons. Separate `ART` directory lookup
  and PNG are deferred.
- Baseline three-component JPEG only, at most 256 KiB encoded, with each source
  dimension at most 512. A bounded marker preflight rejects oversized dimensions
  before invoking the complete JPEG decoder. It does not replace decoder
  validation. Decode/read/allocation failures leave a normal icon.
- One source is read and decoded at a time. The decoded bitmap and JPEG context
  are destroyed after producing a thumbnail. At most six 64×64 RGBA thumbnails
  survive: 96 KiB total, plus cache keys. Failed lookups/decodes are memoized for
  the visible page, avoiding repeated storage access while moving its selection.
- JPEG writes its packed RGB bitmap through an uncached alias. The CPU
  thumbnail sampler reads the same alias, avoiding stale cached heap contents;
  the completed RGBA thumbnail is then synchronized before its DMA upload.
- Thumbnails use aspect-preserving nearest-neighbor fit into a square with
  opaque black padding. Captions are clipped to their tile width. The original
  source filename remains unchanged for opening media.
- Page change, folder/device change, browser refill, switching back to the list,
  and player/GUI cleanup drain DMA and release cached pixels. A full GUI repaint
  also clears the cache through the browser's cleanup callback.

## GS ownership

No permanent VRAM atlas is allocated. Each thumbnail uses the existing GUI icon
scratch range, GS base `0x3FC0`, PSMCT32, buffer width 64. The 64×64 transfer
occupies exactly the last 16 KiB of VRAM, ending at `0x4000`; it does not extend
beyond that range. Each upload, TEXFLUSH and sprite draw is ordered in one VIF1
display chain before the next tile's upload. Existing fallback icons use the
same scratch convention.

Cached pixels remain alive while their upload packets can refer to them. Packet
cleanup and cache eviction first wait for VIF1 and GIF DMA. The JPEG adapter
suspends the clock while using the decoder's VU/DMA path, drains it before
destroying the temporary context, restores VU0 `vf31` from the current display's
`GS_Params()->m_AspectRatio` (the scale consumed by `GS_XYZ`/`GS_XYZv`), and resumes
the clock afterward. GUI/icon/font display chains use VIF DIRECT rather than
VU microprogram calls, so no prior VU1 microprogram is required for the browser.
All six JPEG loads complete before the grid's coordinates/display packets are
constructed. Those packets are heap allocations, separate from the decoder's
scratchpad (`0x70000000..0x70001fff`) and VU memory. The existing
partial browser redraw still suspends the clock around its actual display DMA.

This feature includes the JPEG parser/allocation fixes merged in `26c3d2e`.
It does not make unbounded or malformed JPEGs safe by filename or extension alone.

## Playback integration (2026-10-28)

The candidate integrates playback PR #35 at
`47b7df63abc0a05eb4f1c6c5c0b08851a03ae886`. This includes confirmed-USB refill
selection/allocation cleanup and the three measured MPEG chroma fixes. The
decoder evidence and remaining reporter gates are in the
[playback report](2026-10-28-playback-followup.md). This integration does not
extend the isolated decoder evidence to the browser's full display path.

The known-issues article combines current playback status with artwork setup,
then regenerates the derived Pages/search/wiki outputs. The combined tree
validates 30 pages, 380 search entries and 32 wiki files. All 31 host regression
scripts pass, including both artwork controls, the new vector/skip controls and
the existing playback/I/O checks. Pinned GCC 3.2.3 release, packed, diagnostic
and BDM=0 players and the native motion oracle compile successfully.

PR #35 merged as `83c94b823b474d19978e5a5e4c150d3ec10f1853`, and PR #34 now
targets master. Keep the artwork PR draft for console acceptance. Preserve a
separate artwork tester bundle with its own source/head manifest identity.

## Host verification

Run `python3 tools/test_browser_artwork.py`. It compiles the production artwork
helper and extracts the actual browser loader, navigation and upload-packet
builder under ASan/UBSan. Cases cover filename/path bounds, case resolution,
extension priority and fallback, missing artwork, short reads, oversized source
files/dimensions, decoder/allocation failures, aspect padding, failed-entry
memoization, six-entry page eviction, page boundaries, current-item positioning,
null-selection fallback,
and image-transfer/TEXFLUSH/sprite ordering. File I/O, JPEG decoding and GS
execution are stubbed. These controls establish host-side behavior and packet
structure, not physical rendering or complete decoder acceptance.

Run `python3 tools/test_browser_artwork_lifecycle.py` for the production GUI
cleanup dispatch, browser packet cleanup, destruction callback and reset path.
It verifies that ordinary/full GUI redraws reuse decoded thumbnails, destruction
releases them, and reset clears the filtered list before the grid can select a
stale entry. Both redraw and reset controls failed against the original PR head
and pass with these fixes under ASan/UBSan/LSan.

The lifecycle control also executes the production folder action, level-stack
push/pop, refill and event handler in both list and grid modes. It covers nested
folder entry, Back with directory names longer than a pointer, restoration of
the selected filename/page, refresh, media removal and oversized-path rejection.
The refill now copies its input before reset can release the selected node.
The shared CWD workspace is explicitly bounded to its 1,024-byte region; Back
uses that capacity rather than `sizeof` a pointer. Folder length checks happen
before saving a navigation level, so rejecting an oversized name preserves the
current listing and level stack. Independent negative controls detect the old
folder-name lifetime error and the small Back buffer. Directory/list storage
and GS/DMA calls use host adapters; only EE pointer transport is widened for the
host. Console folder-return and media-removal acceptance remains below.

## Console acceptance still needed

1. Test NTSC, PAL, 480p and an HD mode. Compare tile bounds, caption clipping,
   selected borders, cover aspect, fallback icons, status text and clock.
2. Test empty, one-item, five/six/seven-item and large folders. Exercise every
   direction, page buttons, R2 in both directions, first-tile Left, folder/back,
   device switches, directory refresh and media removal.
3. Mix valid portrait/landscape JPEGs, absent covers, progressive/PNG content
   named `.jpg`, truncated/corrupt JPEGs and sources above both limits. Ensure
   fallback icons and navigation remain usable.
4. Test USB, MX4SIO and a case-sensitive SMB/HOST share; measure first-page
   loading time. Synchronous device calls can wait for their driver's timeout
   on disconnected storage. No asynchronous reader/cancellation is introduced.
5. Start/stop individual video/audio/picture files and Play-all folders
   repeatedly. Confirm the same filenames open, parent selection survives,
   artwork is reloaded on return, and memory does not grow across pages.

No console, screenshot, performance, or reporter acceptance is claimed here.
