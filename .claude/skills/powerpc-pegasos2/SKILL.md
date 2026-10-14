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
  `-append serenity-loader-stop-before-kernel` stops after the boot handoff is validated.
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
  serenity-loader-real-mode-calls serenity-loader-stop-before-kernel" -d trace:vof_write -D vof.log`.
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

Next: write only the single current `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img`
to the SD card, boot with `boot ide:0 serenity.elf`, and capture the serial
output from `native allocation preparation starting` onward. The last
checkpoint will identify a claim, UART, `quiesce`, MMU, kernel, or userland
failure. Do not build separate cards for each checkpoint.
The physical machine is a Pegasos II with a G3 750CXe, a Radeon 9200 SE over DVI, SmartFirmware from
about 2005, an IDE-to-SD adapter, and a serial console on the host; there is no network boot.
