# Upstream bug backlog

Record architecture-independent bugs found during the Pegasos 2 port here when
the same behavior is confirmed in [SerenityOS upstream](https://github.com/SerenityOS/serenity).
Keep platform-specific work in [PowerPCPegasos2Status.md](PowerPCPegasos2Status.md).
These entries are candidates for separate upstream fixes; none has been
submitted upstream as of 2026-10-16.

## Block filesystem cache lock spans all slow device writes

- **Observed risk:** On the physical Pegasos 2, a VFS sync of 16 dirty ext2
  blocks can take about seven seconds. The Shell's library-loading log stops
  during that sync and resumes immediately afterward. Device write latency is
  a separate problem, but the filesystem cache lock can make readers wait for
  the entire batch rather than one block.
- **Upstream source:** [BlockBasedFileSystem.cpp](https://github.com/SerenityOS/serenity/blob/master/Kernel/FileSystem/BlockBasedFileSystem.cpp)
  holds `m_cache.with_exclusive` around the loop that calls
  `file_description().write()` for every dirty block. Verified against
  upstream `master` on 2026-10-16.
- **Local status:** A Pegasos IDE experiment batched ATA cache flushes at
  filesystem sync. The physical PIO log measured the batched drive-cache
  flush at 0 ms, while the slow VFS writeback and Shell wait remained. The
  experiment was reverted; it did not measure each flush in the later UDMA5
  path. The shared filesystem locking remains unchanged.
- **Upstream scope:** Let reads interleave with slow writeback without losing
  concurrent dirty updates, and propagate writeback errors before marking
  blocks clean. This needs a separate concurrency regression.

## Ext2 direct I/O splits large requests into filesystem blocks

- **Observed:** On a real Pegasos 2 using UDMA5, `disk_benchmark` wrote a
  1 MiB file at about 65-66 KB/s whether userland called `write()` with 8 KiB
  or 64 KiB buffers. Reads stayed near 2 MB/s. Changing the userland buffer
  size did not exercise larger IDE DMA descriptors.
- **Upstream source:** [Ext2FSInode::write_bytes_locked()](https://github.com/SerenityOS/serenity/blob/master/Kernel/FileSystem/Ext2FS/Inode.cpp)
  iterates one logical block at a time even with `O_DIRECT`, and
  [BlockBasedFileSystem::write_block()](https://github.com/SerenityOS/serenity/blob/master/Kernel/FileSystem/BlockBasedFileSystem.cpp)
  sends each uncached block as a separate device write. Confirmed on upstream
  `master` on 2026-10-16.
- **Local impact:** The Pegasos image uses 4 KiB ext2 blocks, so a 1 MiB
  direct write causes about 256 block-device requests. The local IDE driver
  then sends ATA FLUSH CACHE after each request. Raw DMA throughput has not
  yet been measured; the adapter's internal write latency may also matter.
- **Upstream scope:** Coalesce physically adjacent blocks without changing
  partial-block, allocation, cache-coherency, or error semantics. Add a
  regression that checks fewer device requests for a large direct write.

## Common storage layer limits every raw request to one page

- **Observed:** A read-only `membench` test of `/dev/hda` could not be run as
  the unprivileged desktop user. Source review shows the test would have been
  limited to 4 KiB device requests despite asking for 64 KiB reads.
- **Upstream source:** [StorageDevice.cpp](https://github.com/SerenityOS/serenity/blob/master/Kernel/Devices/Storage/StorageDevice.cpp)
  caps both read and write requests at `PAGE_SIZE` because an older PATA
  channel has a one-page DMA buffer. Confirmed on upstream `master` on
  2026-10-16.
- **Local fix:** The common storage device retains a one-page default and
  lets the Pegasos IDE device advertise its existing 64 KiB contiguous DMA
  buffer. A host regression checks that a 1 MiB raw request now plans sixteen
  64 KiB IDE requests instead of 256 one-page requests. The PowerPC kernel
  builds; physical validation is pending, and the current SD image is unchanged.
- **Upstream scope:** Let each storage driver report its safe maximum transfer
  size while preserving the one-page limit for drivers that need it.

## USB HID keyboards have no held-key repeat

- **Observed:** On the Pegasos 2, a USB keyboard now registers separate
  presses after the UHCI DATA toggle fix, but holding a key sends only one
  key-down event. Backspace, letters, and Space do not repeat.
- **Upstream source:** [HID KeyboardDriver.cpp](https://github.com/SerenityOS/serenity/blob/master/Kernel/Devices/Input/HID/KeyboardDriver.cpp)
  generates key events only where the old and new report bitmaps differ.
  Unchanged held-key reports cannot produce a repeat, and the driver has no
  timer. Verified against upstream `master` on 2026-10-16.
- **Local fix:** The HID keyboard now starts a 500 ms repeat delay on a
  repeatable key press and emits further presses every 40 ms while held.
  Release, switching keys, and disconnect invalidate pending timers. A host
  regression checks those transitions; physical confirmation is pending.
- **Upstream scope:** Add typematic timing to the generic HID keyboard path
  independently of the UHCI toggle correction. The previous UHCI fix stopped
  processing NAKs and empty packets as reports, exposing the existing absence
  of repeat handling; the HID state-diff rule itself predates that fix.

## One-page DMA buffers are zeroed through a mismatched cacheable mapping

- **Observed risk:** A driver can request an uncached DMA page, but the page
  is first zeroed through the default cacheable quickmap. On a noncoherent
  machine, a delayed dirty-cache writeback can overwrite data later written
  through the uncached DMA mapping. This is a plausible contributor to an IDE
  PRD-table stall on the Pegasos 2, not yet isolated as its sole cause.
- **Upstream source:** [MemoryManager.cpp](https://github.com/SerenityOS/serenity/blob/master/Kernel/Memory/MemoryManager.cpp)
  calls `allocate_physical_page()` without passing the requested memory type
  in `allocate_dma_buffer_page()`. The multi-page allocator already passes the
  memory type for zeroing. Verified against upstream `master` on 2026-10-16.
- **Local fix:** Pass the DMA mapping's memory type to
  `allocate_physical_page()` before the final mapping. Physical validation is
  pending; the Pegasos IDE DMA boot regression checks whether disk reads
  continue into userland.
- **Upstream scope:** Submit this generic allocator correction separately from
  the Pegasos IDE mode and recovery changes, with a noncoherent DMA test.

## UHCI interrupt transfers reuse the same DATA toggle

- **Observed:** A hot-plugged USB keyboard enumerated, but separate Backspace,
  Space, and Return presses were often ignored. A lost key-release report can
  leave HID's previous-key state set, suppressing the next press.
- **Upstream source:** [UHCIController.cpp](https://github.com/SerenityOS/serenity/blob/master/Kernel/Bus/USB/UHCI/UHCIController.cpp)
  invokes the callback and rearms the same queue head; [UHCIDescriptorTypes.h](https://github.com/SerenityOS/serenity/blob/master/Kernel/Bus/USB/UHCI/UHCIDescriptorTypes.h)
  only marks the TD active again. Its `set_data_toggle(false)` cannot clear an
  existing toggle bit. Verified against upstream `master` on 2026-10-15.
- **Local fix:** `3ee3673b02` advances DATA0/DATA1 after a received report,
  preserves it on NAK/error, and resets stale TD status. The
  `PowerPCUHCIInterruptTransferCheckQuick` and normal targets check the toggle
  sequence. Physical confirmation of the new image is pending.
- **Upstream scope:** Implement endpoint toggle progression and successful
  completion handling in the generic UHCI driver, with a repeated-key input
  regression independent of PowerPC.

## UHCI descriptors and control-transfer completion

- **Observed:** The first physical USB keyboard descriptor read returned type
  zero and enumeration failed with `EIO` on a noncoherent PowerPC host.
- **Upstream source:** [UHCIDescriptorPool.h](https://github.com/SerenityOS/serenity/blob/master/Kernel/Bus/USB/UHCI/UHCIDescriptorPool.h)
  allocates ordinary kernel pages for DMA-visible queue heads and TDs.
  [UHCIController.cpp](https://github.com/SerenityOS/serenity/blob/master/Kernel/Bus/USB/UHCI/UHCIController.cpp)
  also treats a NAK as transfer completion and returns a byte count including
  the control setup stage. Verified against upstream `master` on 2026-10-15.
- **Local fix:** `86087d3124` uses DMA-safe descriptor pages, waits through
  NAKs with a timeout, returns only data-stage bytes, and prevents copying
  unread descriptor bytes. The next physical boot recognized the keyboard;
  the trace does not isolate which change was necessary.
- **Upstream scope:** Split the DMA-memory and transfer-completion changes into
  focused patches with host-controller regressions suitable for upstream.
