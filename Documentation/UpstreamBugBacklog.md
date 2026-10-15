# Upstream bug backlog

Record architecture-independent bugs found during the Pegasos 2 port here when
the same behavior is confirmed in [SerenityOS upstream](https://github.com/SerenityOS/serenity).
Keep platform-specific work in [PowerPCPegasos2Status.md](PowerPCPegasos2Status.md).
These entries are candidates for separate upstream fixes; none has been
submitted upstream as of 2026-10-15.

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
