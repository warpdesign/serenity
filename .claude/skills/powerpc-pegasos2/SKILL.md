---
name: powerpc-pegasos2
description: Build, run, test and debug the SerenityOS 32-bit PowerPC port for the Pegasos 2 (QEMU pegasos2 and the physical machine). Use when working on the powerpc32-pegasos2-bringup branch, its loader, drivers (VirtIO, IDE, PS/2, UHCI, Radeon, VIA AC'97), QEMU checks, or SmartFirmware boot work.
---

# SerenityOS on PowerPC Pegasos 2

Branch `powerpc32-pegasos2-bringup` (pushed to `github.com:warpdesign/serenity`). The port boots to
the desktop in QEMU's `pegasos2` machine. Current state and history:

- `Documentation/PowerPCPegasos2Status.md`: what works, every QEMU check target.
- `Documentation/PowerPCSmartFirmwareBootPlan.md`: the plan for the physical machine (next work).
- `Documentation/PowerPCPegasos2.md`: early bring-up record (historical).
- `Documentation/UpstreamBugBacklog.md`: confirmed architecture-independent
  bugs also present in upstream SerenityOS, for separate upstream fixes. Add
  future findings there after checking current upstream source.

Commits are authored "Nicolas Ramz <nicolas.ramz@gmail.com>" with no co-author trailer.

## Setting up a new machine (macOS host)

```sh
brew install qemu e2fsprogs gzip           # QEMU 11.x; e2fsprogs gives mke2fs/debugfs/e2fsck
env ARCH=powerpc MAKEJOBS=16 ./Toolchain/BuildGNU.sh      # ~40 minutes
export PATH=/opt/homebrew/bin:$PATH         # macOS /usr/bin/python3 (3.9) is too old for the tests
cmake -S Meta/CMake/Superbuild -B Build/superbuild-powerpc -GNinja -DSERENITY_ARCH=powerpc
cmake --build Build/superbuild-powerpc --target serenity-configure
cmake --build Build/powerpc --target PowerPCDesktopCheckQuick     # kernel, loader, desktop userland
```

The PATH export matters for every build: the QEMU check targets run Python scripts that use
`str | None`. An x86_64 toolchain (`ARCH=x86_64`) is only needed to check that shared kernel code
still compiles: `cmake --build Build/x86_64 --target Kernel`.

## Images and running

The loader `Build/powerpc/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf` contains the
gzip-compressed kernel (6.1 MB).

Debug and release images: development uses the default debug configuration, with the kernel's
undefined behaviour sanitizer (KUBSAN) on, accepting the 6.1 MB image. KUBSAN accounts for nearly
half the kernel code and 90% of its relocations; a release build in a separate build directory
configured with `-DENABLE_KERNEL_UNDEFINED_SANITIZER=OFF` gives a 2.4 MB `serenity.elf`. Both are
to be produced for a first public release; don't turn KUBSAN off in `Build/powerpc`.

Disk images are made by the test scripts in `Tests/Kernel/PowerPC/`;
pass `--output-dir DIR` to `test_desktop.py` or `test_pixel_paint.py` to keep `root.ext2`:

```sh
B=Build/powerpc
python3 Tests/Kernel/PowerPC/test_desktop.py --qemu qemu-system-ppc \
    --loader $B/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf \
    --user-loader $B/Userland/DynamicLoader/Loader.so --build-dir $B --output-dir $B/PowerPCBoot
```

Always boot a copy (`cp -c root.ext2 /tmp/x/root.ext2`); the guest writes to the disk. Interactive run:

```sh
qemu-system-ppc -M pegasos2 -cpu 750cxe -m 512 \
    -vga none -device virtio-gpu-pci -device virtio-keyboard-pci -device virtio-mouse-pci \
    -kernel $B/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf -append root=lun3:0:0 \
    -drive "if=none,format=raw,id=d,file=/tmp/x/root.ext2" -device virtio-blk-pci,drive=d \
    -object rng-random,id=rng0,filename=/dev/urandom -device virtio-rng-pci,rng=rng0 \
    -display cocoa,zoom-to-fit=on -serial file:/tmp/x/serial.log -monitor none -no-reboot
```

Variants:
- IDE root instead of VirtIO: replace the two disk options with `-drive "if=ide,format=raw,index=0,file=..."`.
- Radeon instead of VirtIO GPU: `-vga none -device ati-vga,model=rv100`.
- PS/2 input: drop the `virtio-keyboard-pci`/`virtio-mouse-pci` devices.
- USB input: `-device usb-kbd -device usb-tablet` (VT8231 UHCI; the Pegasos 2 has USB 1.1 only).
- Audio to the speakers: `-M pegasos2,audiodev=audio0 -audiodev coreaudio,id=audio0`.

## Checks

Each target has a `Quick` variant (750 CPU) and a full one (750cxe):
`PowerPCFirmwareLoaderCheck`, `PowerPCWindowServerCheck`, `PowerPCDesktopCheck`,
`PowerPCDesktopProgramsCheck`, `PowerPCPixelPaintCheck`, `PowerPCBrowserCheck`, `PowerPCVIAAudioCheck`,
`PowerPCIDEBootCheck`, `PowerPCRDBCardBootCheck`, `PowerPCPS2InputCheck`, `PowerPCUSBInputCheck`, `PowerPCRadeonDesktopCheck`,
`PowerPCKmallocHPTCheck`. Run with `cmake --build Build/powerpc --target <name> -- -j18`.

## Debugging techniques

- Kernel log: `-serial file:serial.log`; the kernel always appends `serial_debug`.
- Loader output goes through the firmware: add `-d trace:vof_write,trace:vof_claim -D vof.log`.
  `-append halt` stops after the boot handoff is validated.
- Silent hang: `info registers` on the monitor (`-monitor unix:mon.sock,server=on,wait=off`,
  `printf 'info registers\n' | nc -U mon.sock`). `NIP fff00700` means a Program exception with no
  handler; the loop overwrites SRR0, so use `-d int -D int.log` to see the first exception
  ("Raise exception at 00000000" = a return through a corrupted LR, usually a stack overwrite).
- Kernel backtrace addresses: the kernel runs at `0xc1c00000` in QEMU, so
  `powerpc-serenity-addr2line -f -C -i -e Build/powerpc/Kernel/Kernel $((addr - 0xc1c00000))`.
- Drive the guest from the QEMU monitor: `sendkey`, `mouse_move`, `mouse_button`, `screendump`
  (PPM). Put a shell script on the disk with `debugfs -w -R "write local.sh /home/anon/x.sh" disk`,
  type `Shell /home/anon/x.sh` into Terminal, then `debugfs -R "cat /home/anon/out.txt" disk`.
  The guest syncs about every 10 seconds; wait before reading files back.
- Profiling: see `Meta/PowerPC/qemu-tbprof.c` and `symbolize-tbprof.py`. Monitor NIP sampling is
  biased towards `mtmsr`/`tlbie`/MMIO instructions; the plugin is exact.
- QEMU device traces, e.g. `-trace ide_sector_write -trace ide_data_writew`, show what a driver
  really sent.

## Tools in Meta/PowerPC

| Tool | Use |
| --- | --- |
| `qemu-tbprof.c` | TCG plugin: exact per-block instruction counts with reset/dump control files. |
| `symbolize-tbprof.py` | Turns a profile into per-library and per-function costs (kernel and userland). |
| `pixelpaint-latency.py` | Boots a PixelPaint image headless and times cursor, paint and burst latency. |
| `amiga-disk.py` | Read-only RDB partition listing and OFS/FFS (incl. DOS\6/\7) listing and extraction. |
| `make-pegasos-card.py` | Builds the SD card image (RDB, FFS boot partition with `serenity.elf`, ext2 root), or rewrites only the boot partition. Boot it in QEMU with `-drive if=ide,...` and `root=lun3:0:0;part1`; copy it to the card with `dd`. amitools (`pip install amitools`: `rdbtool`, `xdftool`) independently checks the result. |
| `make-pegasos-root.py` | Builds the SD card's ext2 desktop root from the desktop test recipe, with `membench` installed for manual use and no benchmark boot service. |
| `test-loader-inflate.cpp` | Runs the loader's gzip decoder on the host against the real kernel. |

## Gotchas found so far

- `root=lun3:0:0` depends on storage controller numbering; adding a PCI device such as
  `qemu-xhci` renumbers it and the kernel panics with "Couldn't find a suitable device to boot from".
- QEMU's VOF claims only the sum of the loader's segment sizes, which ends inside the loader's bss
  and stack. Never use an address-unspecified `claim` for memory right after the loader.
- Big-endian bugs come in three forms: native integers in hardware structures (use
  `LittleEndian<T>`), C bit-fields over wire formats (GCC allocates them MSB-first on big-endian;
  decode with shifts), and pixel byte order. GCC's `scalar_storage_order` pragma is C-only.
- QEMU's `ati-vga` ignores `SURFACE_CNTL` byte swapping and only implements the Rage 128
  `CONFIG_CNTL` aperture swap; the Radeon driver uses the latter for QEMU's device (PCI subsystem
  vendor `1af4`) only.
- Files copied from `Build/caches` are mode 0600 on the host and `mke2fs -d` keeps that; make data
  files world-readable in image recipes.
- Apps that `unveil` a missing executable (Calendar → CalendarSettings, System Monitor → Escalator,
  NetworkSettings) exit at startup; image recipes must include those executables.
- Upstream `EBRPartitionTable` used to accept any readable sector 0, which hid every later partition
  table format; it now requires an MBR signature and an extended partition. `GUIDPartitionTable`
  still reads its fields as native integers, so GPT disks are misread on PowerPC.
- xHCI does not work on big-endian (about 600 bit-fields in its register and DMA structures); it
  declines the controller instead of crashing. EHCI is a stub upstream.

## Progress log

- 2026-10-13: full build from scratch; drawing lag fixed (VirtIO GPU partial flushes, PixelPaint
  thumbnail coalescing); i8042 PS/2 on the VT8231; new PCI native mode IDE driver (polled PIO,
  LBA48); upstream Ext2 superblock backup bug fixed; R100/R200 Radeon driver (QEMU rv100);
  hardware tools and System Monitor in the image; UHCI/USB HID big-endian fixes.
- 2026-10-14: Radeon internal DVI output; SmartFirmware boot plan; kernel embedded gzip-compressed
  in the loader (26.8 MB to 6.1 MB, CRC-checked). Measured KUBSAN's cost (2.4 MB image without it)
  and decided to keep it on until a first release, which will ship debug and release images; fixed
  the kernel build with KUBSAN disabled. RDB partition tables in LibPartition, the SD card image
  tool, and a QEMU check booting the card layout; fixed EBR detection hiding other table formats.

- 2026-10-14: SmartFirmware probe loader (real-mode client-call trampoline, banner, full device tree
  dump, `exit` on non-QEMU firmware). QEMU test: `-append "ofdump
  realmode halt" -d trace:vof_write -D vof.log`.
  The survey card was retired. The one current SD card image is
  `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img`.

- 2026-10-14: The physical G3 Pegasos 2 completed the firmware tree dump after
  bounding a large Radeon `rom` property to 128 displayed bytes. SmartFirmware
  printed `Program terminated!` and restarted after the loader's `exit` call;
  this is its observed normal termination behavior. The next image,
  `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img`,
  claims and releases one 4 KiB page selected from `/memory@0/available`,
  then stops before changing the MMU. QEMU's forced real-mode check passes.
  The next submitted physical log had no claim markers between tree end and
  the stop message, which matches the earlier survey-only loader. The exact
  card image used is unconfirmed. Obsolete generated card images were removed;
  the claim image printed `exact RAM claim probe starting` and made the tree
  dump opt-in via `ofdump`. The current image is the combined boot path below.

- 2026-10-15: The physical 4 KiB claim at `0x01800000` succeeded and was released.
  To avoid more SD card writes for one-purpose probes, the current loader now
  claims kernel staging, loaded kernel and HPT from `/memory@0/available`,
  captures firmware settings, calls `quiesce`, keeps serial output through the
  native UART, switches the MMU and enters the kernel. QEMU's forced native
  path reaches the desktop from the RDB IDE card and writes a 108,894-byte
  file with a clean ext2 result on both 750 and 750CXe. The physical full
  boot has not run yet. `stop` in `bootargs` stops after allocations on the
  same card; `ofdump` explicitly enables the tree dump.

- 2026-10-15: The next physical boot reached the kernel and `init_stage2`:
  kernel loading, HPT, `quiesce`, native serial, scheduler, and RTC all worked.
  Its first error was `PCI: Failed to assign Pegasos 2 device resources`;
  PCI registration stopped, so storage detected zero devices and root mount
  panicked. The local Haiku `pegasos2-support` source uses RTAS PCI config on
  physical SmartFirmware and preserves firmware BARs. Serenity now instantiates
  RTAS before `quiesce`, passes its runtime/tokens to the kernel, selects RTAS
  PCI config on physical firmware, and preserves those BARs. QEMU's direct
  config/resource assignment path remains selected when PCI RTAS tokens are
  absent. The host RTAS PCI contract target passes and kernel/loader compile.
  The user explicitly requested no further QEMU verification runs. Physical
  RTAS enumeration and boot are still unverified.

- 2026-10-15: The first RTAS image reserved runtime memory at
  `0x0318a000`–`0x0318dfff`, entered the kernel, and faulted at
  `boot_rtas_info()` before any RTAS call. The access was to `0x007c3268`,
  the loader's low-memory reservation array after its mapping had gone away.
  The loader now fully validates RTAS against that array before handoff; PCI
  and power services use a late lookup that only reads copied RTAS fields.
  `TestBootInfo.cpp` covers late lookup with the reservation pointer absent.
  Physical PCI identification remains the next checkpoint. Do not run QEMU
  verification unless the user asks to resume it.

- 2026-10-15: The next physical boot passed the stale-pointer fix and reached
  the RTAS PCI read, but got south bridge ID `ffff:ffff`; no PCI devices were
  registered and storage remained at zero. The next build records the RTAS
  service status/value and probes native MV64361 config as a fallback. It
  proceeds through that path only if the VT8231 and VIA IDE IDs match
  `1106:8231` and `1106:0571`. Firmware BARs are preserved on both native
  paths. Physical outcome is pending; no QEMU boot was run.

- 2026-10-15: Compared RTAS result handling in the local Haiku Pegasos port
  and Linux CHRP PCI code. Both, like Serenity, read status from return cell
  zero and the PCI value from return cell one. Haiku uses a regular RAM
  mapping for its RTAS argument page; Linux uses a static RAM block. Serenity
  had used the generic MMIO helper, which mapped this RAM page with I/O cache
  attributes. The next build maps it as normal cacheable RAM for PCI and
  power calls. Added native quick/normal targets that specifically require
  RTAS PCI success, separate from the fallback-tolerant native PCI check.
  No QEMU run was made; physical confirmation is pending.

- 2026-10-15: The user booted the earlier fallback image, not the cacheable
  RTAS image. RTAS status/value both remained `ffffffff`. Direct PCI fallback
  identified the VT8231 and IDE, routed two PCI lines, and enumerated all
  south-bridge functions. The direct AGP display probe returned `ffff:ffff`.
  Serial output stopped after UHCI `Interrupt line: 9`; the user confirmed a
  hang. UHCI did not enable PCI I/O decoding and used unbounded hardware
  waits. The next build enables decode and bus mastering, and adds bounded stop/reset/start
  polls, allowing storage discovery after a USB failure. The native boot-log
  regression self-test now rejects a log stopped at UHCI. Neither this fix nor
  the cacheable RTAS mapping has been tested on physical hardware. No QEMU
  run was made.

- 2026-10-15: The latest physical boot passed RTAS PCI (status zero),
  enumerated the Radeon 9200 SE and both host bridges, initialized both
  UHCI controllers, drove DVI at 1024×768/60, found the IDE-to-SD adapter
  and two RDB partitions, mounted ext2, and started `/init`. It then logged
  `Entropy starvation...`; the screen had a static white mark on black.
  There is no physical RNG source registered. The next image offers an
  opt-in `weakrng` kernel boot argument, mixing interrupt timing
  for development only. PowerPC external IRQ timing is collected by default,
  as on x86. `KernelRng::wait_for_entropy()` now uses the predicate-based
  wait queue, releasing the RNG lock while sleeping without losing wakeups.
  The board has
  a PS/2 mouse but no PS/2 keyboard. i8042 previously sent another command
  to the absent keyboard in its failure path and exited before probing the
  mouse; it now disables that port and continues. The USB keyboard is
  plugged in after firmware boot. The Radeon safe mode and synthetic EDID
  prefer 1024×768; the monitor's native 1920×1080 remains open. Physical
  Ethernet `1106:3065` is unsupported and AC'97 reported 1040 Hz. No QEMU
  boot was run. The new entropy and mouse changes await physical validation.

- 2026-10-15: Investigated the VIA AC'97 1040 Hz log. `AudioChannel::create`
  requested 44,100 Hz; the physical codec readback was 1040 Hz. Linux
  `via82xx` and Haiku `auvia` set bit 25 (primary codec valid) on AC'97
  commands, while Serenity omitted it. The compiled VIA driver now sets that
  bit for read/write, waits for read completion, and rejects invalid DAC
  rates after three attempts. The host VIA codec-command quick/normal
  contract targets pass; native quick/normal audio log targets reject 1040 Hz.
  This audio change was later included in the updated SD image. No QEMU run
  was made; physical audio behavior is pending.

- 2026-10-15: The next physical boot confirmed the interrupt-timing entropy option became
  ready and the attached PS/2 mouse initialized. RTAS, both UHCI controllers,
  Radeon DVI at 1024×768, IDE/RDB/ext2, and `/init` worked. The dynamic
  loader then failed with `MM: Unable to commit 3 pages, have only 1` and
  `mmap writable: Out of memory`. The physical usable-memory regions total
  about 183 MiB. Radeon BAR0 exposes 128 MiB; the shared framebuffer commits
  one RAM page per mapped framebuffer page for fake console writes. The
  compiled Radeon change caps the mapping at 32 MiB (enough for 1080p) and
  logs uncommitted pages before/after allocation. Host and native quick/normal
  regressions cover the cap and the observed OOM. `/memory/reg` reports
  512 MiB installed but `/memory/available` reports only 244 MiB; the loader
  uses the latter for the kernel by default. `fullram`
  selects the full `reg` range after firmware quiesce while keeping loader
  claims inside `available`. Linux and Haiku use `reg` for physical RAM.
  The user authorized updating the existing card image. Its FFS
  `serenity.elf` now contains the Radeon, audio, optional full-RAM, and short
  boot-argument changes;
  extraction matched the rebuilt loader. Root ext2 was not rewritten. The
  image SHA-256 is
  `54dfbe2d4a073b00c00dede91b0a23d4bfdfc0e9c02a379c5bfd081db3e167d6`.
  No QEMU run was made.

- 2026-10-15: Physical boot with `weakrng` reached the desktop. Serial timing:
  `/init` 10.126 s, WindowServer main loop 24.275 s, user login 34.330 s,
  Taskbar and Terminal began loading around 44 s. The display was bluish,
  and repainting was slow. The photo shows the configured black Terminal
  background as saturated blue, consistent with unswapped big-endian opaque
  pixels putting alpha `0xff` in the blue channel. The physical RV280 path
  set `SURF_TRANSLATION_DIS`
  along with the 32-bit aperture byte-swap bit. Linux radeonfb notes that
  translation disable also disables the swapper on later chips. The rebuilt
  driver clears stale surface descriptors, enables both aperture swap bits
  without translation disable on big-endian native Radeon, and logs the
  resulting `SURFACE_CNTL`. Host and native-log regression checks were added;
  physical color correction was later confirmed. IDE uses polled PIO, but the time
  spent on disk versus painting has not yet been measured. The sole current
  SD image is `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img`; its SHA-256 is
  `54dfbe2d4a073b00c00dede91b0a23d4bfdfc0e9c02a379c5bfd081db3e167d6`.
  The FFS loader was replaced; ext2 root was unchanged. No QEMU run was made.

- 2026-10-15: User confirmed Radeon colors are fixed. After hot-plugging the
  USB keyboard, UHCI root port 2 detected it, but the first physical device
  descriptor returned type zero and device creation failed with `EIO`.
  Queue-head and transfer-descriptor pools were cacheable kernel memory even
  though UHCI uses DMA to read and update them. They now use cache-inhibited
  DMA pages. Control transfers no longer treat a temporary NAK as success,
  report only data-stage bytes, and have a bounded wait. `USB HID: Initialized
  interface` is logged after successful HID setup. The native keyboard test
  rejects the captured descriptor failure. The rebuilt loader was extracted
  from the card image and matched byte for byte; root ext2 was unchanged.
  The single current SD image SHA-256 is
  `54dfbe2d4a073b00c00dede91b0a23d4bfdfc0e9c02a379c5bfd081db3e167d6`.
  Physical keyboard enumeration was later confirmed; typing remained erratic.
  No QEMU run was made.
  The user also reported Terminal's empty frame lasting at least five seconds
  before its menu/content, followed by wallpaper and taskbar. In the serial
  log Terminal starts at 44.085 s, Shell at 49.668 s, and Shell libraries
  continue loading until 58.995 s. IDE uses polled PIO and Radeon framebuffer
  CPU access is uncached; neither has been timed separately yet. Do not treat
  the staged startup as one graphics stall without measurements.

- 2026-10-15: SystemMonitor launched under the conservative map and requested
  1024 pages (4 MiB) when only 508 uncommitted pages remained. It aborted at
  `AK/Error.h:252`; the user observed the whole desktop freeze. This is a RAM
  capacity failure, not evidence that the board's reported slow memory-transfer
  speed caused the allocation failure. The current SD image already has the
  optional `fullram` boot flag. The user later confirmed that SystemMonitor
  runs with `weakrng fullram` on the same card. The default path without
  `fullram` remains limited to the smaller firmware-available map.

- 2026-10-15: The USB keyboard enumerated, but keypresses were erratic.
  Backspace worked once, then later presses were ignored; Space and Return
  were often missed. Successful keys appeared promptly. The UHCI async poll
  path rearmed the same interrupt TD without alternating its DATA0/DATA1
  toggle, so the keyboard's release reports could be lost and HID's state
  comparison would suppress later presses. The TD now advances its toggle
  only after a successful nonempty report; NAK/error retries preserve it and
  clear stale status. Host quick/normal toggle checks and the kernel build
  pass. The existing card's FFS loader was updated once and extracted byte
  for byte; ext2 was unchanged. Current image SHA-256:
  `54dfbe2d4a073b00c00dede91b0a23d4bfdfc0e9c02a379c5bfd081db3e167d6`.
  Physical confirmation remains pending. No QEMU run was made.
  Upstream SerenityOS master still rearms the interrupt TD without advancing
  its toggle and its setter only ORs the toggle bit. The fix is committed
  locally as `3ee3673b02`; it has not been submitted upstream.

- 2026-10-15: Performance work. `memcpy`, `memmove` and `memset` were byte loops on PowerPC in the
  kernel, LibC and the dynamic loader, so each WindowServer flush to the uncached Radeon framebuffer
  was four bus writes per pixel. They are now shared assembly in
  `Userland/Libraries/LibC/arch/powerpc/memory.S` (aligned words in 32-byte blocks, no `dcbz`,
  because it faults on cache-inhibited memory). New `membench` utility: RAM, framebuffer (store
  widths, read back) and raw disk rates, mirrored to the kernel log; `membench --verify` runs in
  `PowerPCDesktopProgramsCheckQuick`. IDE now uses polled bus-master Ultra DMA, with a cacheable
  bounce buffer flushed by `dcbf` around transfers. VIA timing is programmed, and the mode is capped
  at UDMA2 unless the firmware set the 80-wire cable bit. A failed transfer falls back to PIO, and
  `nodma` disables DMA. QEMU raw reads: 4.6 MB/s PIO, 13-14.5 MB/s DMA. QEMU memory numbers say
  nothing about real hardware. The card was rebuilt here with both changes and the boot benchmark:
  `make-pegasos-root.py --boot-benchmark`, then `make-pegasos-card.py card`. Physical results are
  pending.

- 2026-10-15: Phase 4 legacy I/O. The loader derives the PCI I/O window from the south bridge's I/O
  range, maps DBAT2 on its 16 MiB block (`powerpc_loader_enable_translation` second argument; 0
  skips DBAT2), and passes `legacy_io_physical_base` in `BootInfo`. The kernel debug UART, PIC, RTC
  and i8042 go through `Pegasos2/LegacyIO.h`; `fixedio` restores the constants in loader and kernel.
  Kernel, loader and boot-probe harness build; no QEMU run (not requested). Only the card's FFS
  loader was replaced; SHA-256
  `8237052ebf8ed71361b69264e809e1e64e381be6d998c587d7f1e8a28163cf56`.

- 2026-10-16: The other computer pushed DMA IDE, `membench`, and faster
  PowerPC memory routines. The physical boot with `fullram weakrng` mounted
  ext2 but stalled a DMA read of eight sectors at LBA 68136. Drive status
  `0x58` kept DRQ asserted; bus-master status `0x21` kept Active set, with
  neither Error nor Interrupt. Switching to PIO without resetting the drive
  failed the same request again, `/init` exited, and the SD LED stayed lit.
  The user tested `boot ide:0 serenity.elf fullram weakrng nodma` on that
  image and reached the desktop, isolating this boot failure to IDE DMA.
  Source fixes staged: default VIA UDMA2 when IDENTIFY word 88 has no selected
  mode (instead of immediately picking UDMA5); `udma=0` or `udma=5` selects an
  explicit supported mode on a future image; ATA software reset before PIO
  retry after a DMA error; one-page DMA allocations zero with the requested
  memory type to avoid cacheable/uncached aliases. The generic allocator bug
  is also in upstream SerenityOS and is recorded in
  `Documentation/UpstreamBugBacklog.md`. New logs show PRD table/bounce-buffer
  physical addresses and PRD contents on failure. Host UDMA policy and native
  boot-log self-tests and the PowerPC kernel/loader build passed. Physical
  verification is still needed. No QEMU run was made.

- 2026-10-16: Rebuilt the 929 MiB canonical card image from the current kernel,
  loader and ext2 desktop root. The local previous image was stale and had no
  `membench`, unlike the user's other-computer image. The new root installs
  `/bin/membench` but has no auto-start service; the root-image regression
  checks this. From Terminal after startup, run `membench` for RAM rates or
  `membench --verify` for memory-routine checks. With device access,
  `membench -f /dev/gpu/connector0 -d /dev/hda` adds framebuffer and disk.
  The loader now accepts `boot ide:0 serenity.elf /?`, prints options, and
  prompts at `boot>`; a typed line replaces the original arguments and Enter
  continues booting without reloading the 6 MiB ELF. `exit` returns to
  SmartFirmware through the saved entry context, without calling the firmware
  `exit` service. The prompt, return, and DMA changes still need a physical
  test. Host tests and the kernel/loader build pass; no QEMU run. Card image
  SHA-256:
  `44ae3b8869c28135f33d0e81beb798ab32cb766a6587283508f9ef8a67a31d92`.

- 2026-10-16: USB held-key repeat investigation while the user writes the
  card above. `3ee3673b02` fixed UHCI DATA toggling and ignored NAK/empty
  transfers, but HID's report-state diff only sends presses on transitions;
  unchanged held keys had no typematic timer before that commit either.
  Upstream SerenityOS has the same generic HID behavior; it is logged in
  `Documentation/UpstreamBugBacklog.md`. Source now synthesizes HID key-down
  events after 500 ms and every 40 ms until release, another key takes over,
  or device detach; modifiers and lock keys do not repeat. Host regression
  and PowerPC kernel build pass. The first image written by the user did not
  include this USB change; a later image below now does.

- 2026-10-16: First physical boot of the rebuilt card without `nodma` reached
  the desktop, but **DMA itself failed**. `fullram weakrng` selected UDMA2
  (`0x17 -> 0xf4`); at uptime 14.154 a read of eight sectors at LBA 198360
  failed with ATA status `0x23` and bus-master status `0x21`. PRD pointed to
  `0x05d77000`, 4096 bytes, EOT `0x8000`. ATA soft reset returned `0x50`;
  the driver then used PIO and WindowServer started at 32.811 s. Full capture:
  `/Users/leo/.t3/userdata/attachments/046af1a9-eae0-4cea-8013-ceea9167016f-637a902f-e672-4349-a2c1-71820db91d58-txt.txt`.
  `PowerPCNativeIDEDMABootCheck` quick and normal pass on this capture, which
  verifies recovery, not continuing DMA. The user then booted the same card
  directly with `boot ide:0 serenity.elf fullram weakrng udma=0` (no `/?`
  required). UDMA0 (`0x17 -> 0xf5`) also failed an eight-sector read, now at
  LBA 353536 at 20.842 s; ATA status was `0x73`, bus-master status `0x21`.
  Reset returned `0x50`, and PIO fallback again reached the desktop. Full
  second capture:
  `/Users/leo/.t3/userdata/attachments/046af1a9-eae0-4cea-8013-ceea9167016f-dbef3f0f-8325-4dd0-9203-61f8af166dcd-txt.txt`.
  `test_native_ide_dma_boot.py --normal --mode 0` passes on the second log.
  UDMA speed alone does not explain the failure. The drive selected mask was
  `0x04` before the UDMA0 request, versus `0x00` before the prior UDMA2 run.
  The local 929 MiB card image was then rebuilt with the committed HID
  held-key repeat fix and an additional ATA error-register diagnostic on DMA
  failure. The ext2 root is unchanged. New SHA-256:
  `ef9c13eb88454b5f468757f17fabe5c7f40b452840d554d4f0c737f73a23f4cb`.
  The new image needs physical repeat and DMA tests; do not claim DMA works
  from the recovered desktop boot.

- 2026-10-16: The user confirmed USB key repeat works in the physical image.
  Full serial capture:
  `/Users/leo/.t3/userdata/attachments/046af1a9-eae0-4cea-8013-ceea9167016f-f89554b6-ef4b-4683-b1c9-5b934ab8d2b4-txt.txt`.
  UDMA2 still stalls after an eight-sector read at LBA 184032; ATA status
  `0x58` has DRQ without ERR, ATA error `0x00`, bus-master status `0x21` has
  Active. Reset to `0x50` and PIO fallback boot the desktop. The user's Shell
  waits 7–8 s in gaps aligned with VFS syncs of 16–19 ext2 blocks; the Shell
  itself is loading shared libraries, not writing that data. On the card,
  `disk_benchmark -t 1 -f 131072 -b 8192 -d /home/anon` reported two runs in
  8815 ms, write_bps 102500, read_bps 1311000. Source inspection found that
  the IDE driver sent ATA FLUSH CACHE after every small write, while the
  generic block filesystem cache lock spans the whole writeback loop. The
  latter is confirmed in upstream and recorded in
  `Documentation/UpstreamBugBacklog.md` for a separate fix.
  IDE writes now defer their ATA flush until filesystem sync/fsync, with a
  pending-write counter and flush-duration diagnostic. Generic partition
  sync forwards to the device; unsupported devices retain their prior behavior.
  The PowerPC ELF `DT_PPC_GOT` tag was already used by the PLT resolver but
  logged as unhandled `DT_LOPROC` by the generic scanner; architecture-specific
  recognition now suppresses that warning. Host tests, kernel, loader,
  dynamic loader, LibELF, ext2 fsck and card content comparison pass. No QEMU
  run. **Physical performance of this newest image is unverified.** Current
  929 MiB card SHA-256:
  `050dade55256bc5f42c17f8cc13d929910c7313afe7e98c4eab7d5c04da82ed4`.

- 2026-10-16: The user's next physical test of that image crashed: DMA read
  LBA 151776 failed (`ATA status 0x55`, ATA error `0x00`, BM status `0x21`),
  SRST then timed out with status `0x58`, further reads failed, and `/init`
  faulted. The PIO retry is after `TRY(reset_after_dma_error())`, so the reset
  timeout returned before the retry. Earlier images reached status `0x50`
  and could continue with PIO. The failed boot cannot validate the newest
  writeback performance change. Explicit `nodma` is the safe way to boot the
  current card.
  Source review against the PCI IDE bus-master specification identified a
  strong candidate: our IDE channel left ATA `nIEN` set during DMA, hiding
  the drive's INTRQ completion signal from the bus-master engine. The engine
  status `0x21` is Active=1, Interrupt=0. New source masks the PCI function's
  interrupt output to the CPU (falls back to PIO if that mask cannot be set),
  clears device `nIEN` only for DMA, acknowledges any prior INTRQ, and accepts
  DMA only when BM Interrupt is latched and the drive is ready. A failed DMA
  command now gets one additional, longer bounded SRST attempt; this remains
  best-effort and cannot guarantee recovery from a wedged adapter. On physical
  Pegasos, the loader now appends `nodma` by default; `dma` or `udma=N` opts
  into experimental DMA, while QEMU defaults remain unchanged. Host tests
  and the PowerPC kernel/loader build pass; no QEMU or physical run of this
  source yet. The prior card then booted with explicit `nodma`, but still
  took around 81 s to finish drawing the Shell window. VFS syncs of 16–23
  blocks took around 8 s; `IDE: cache flush after 18/25/22 writes took 0 ms`
  shows the ATA flush itself was not the bottleneck. The flush-batching
  experiment was reverted; per-write cache flushes are restored. The user
  requested one image only: the canonical `pegasos2-sdcard.img` was replaced
  with the revised loader and the temporary candidate removed. New 929 MiB
  SHA-256:
  `a2ca58ac738634188fa95f8ebe74fe075f4a24d066cea5485492d2bcd925a3bf`.
  The kernel now logs up to eight slow PIO writes broken into command setup,
  device wait, 16-bit data-port transfer, and completion times. The earlier
  `Flushed 16 blocks` count covers cached blocks only; other metadata writes
  may add to the total. The user recalls MorphOS uses UDMA on this adapter,
  so do not attribute Serenity's slow PIO path to a proven adapter limit.
  Capture `IDE: PIO write ...` lines and test explicit `dma` separately.
  FFS loader and ext2 root match the built inputs; this image needs a
  physical run. Spec:
  `https://read.seas.harvard.edu/~kohler/class/04f-aos/ref/hardware/IDE-BusMaster.pdf`.

Next: write the one canonical image with safe default PIO and opt-in `dma`.
Boot it with `fullram weakrng`
first, then `fullram weakrng dma` to test the revised DMA completion. Measure Shell
window delay and rerun the one-case disk benchmark to compare with the 102500
bytes/s write baseline. Check for any DMA failure/reset. USB repeat is already confirmed. The image accepts
`udma=0` or `nodma` without another rewrite. Do not assume a faster UDMA mode
will fix the observed UDMA0 failure. `boot ide:0 serenity.elf /?` opens the optional firmware
prompt; `exit` should return to SmartFirmware without restarting it.
Short options are `/?`, `weakrng`, `fullram`, `dma`, `halt`, `ofdump`, `realmode`,
`claimram`, `native`, `nodma`, `udma=<mode>`, and `fixedio`. Check the serial log for
`legacy I/O ports at 0xfe000000`; if the board goes silent after `entering kernel`, retry with `fixedio`.
After the desktop appears, plug the USB keyboard into the same root port 2
and test separate repeated letters, two Backspaces, Space, and Return in
Terminal; capture any UHCI transfer-error lines. SystemMonitor already runs
with `fullram`.
The default RAM map remains conservative if `fullram` is omitted.
Check that the loader prints `using installed RAM from /memory/reg` and the
kernel lists physical RAM ending at `0x1fffffff`. The monitor should still
report 1024×768/60 until native
mode selection is added.
Do not build separate cards for each checkpoint or run QEMU verification
until the user requests it.
The physical machine is a Pegasos II with a G3 750CXe, a Radeon 9200 SE over DVI, SmartFirmware from
about 2005, an IDE-to-SD adapter, and a serial console on the host; there is no network boot.
