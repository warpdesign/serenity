# Booting the PowerPC port from Pegasos 2 SmartFirmware

## Goal

Boot the physical Pegasos 2 (512 MiB, G3 750CXe module, Radeon 9200 SE over
DVI, SmartFirmware from around 2005) from SmartFirmware to the SerenityOS
desktop, with the root filesystem on its IDE disk. QEMU's Virtual Open Firmware
(VOF) path must keep working unchanged.

The Pegasos II is debugged over a serial cable connected to a separate host
computer running a terminal, as during the Haiku port. Its disk is an SD card
in an IDE-to-SD adapter, the same card Haiku booted from. Network (TFTP) boot
is not assumed to work, so every test boots from that card. The card's whole
content may be replaced; it can be rewritten for Haiku afterwards.

The Haiku PowerPC port has already booted this machine
([Haiku-PowerPC](https://github.com/warpdesign/Haiku-PowerPC), branch
`pegasos2-support`, `docs/ppc/smartfirmware-handoff-plan.md`). Its findings
are the starting point here; each one still has to be confirmed for our
loader.

## What the current loader assumes

`Kernel/Arch/powerpc/Loader/` is one ELF containing the kernel
(`_binary_kernel_elf_*`). It is written for VOF:

| Area | Current behaviour | Problem on SmartFirmware |
| --- | --- | --- |
| Client calls | Calls the entry in `r5` with whatever MSR firmware entered with. | SmartFirmware reports `/options/real-mode? = true` but enters with `MSR = 0x3030` (IR/DR on). Calls must be made with IR/DR off. |
| Memory | `claim(0, size, align)` for the kernel and HPT. | An address-unspecified `claim` returned 0 on the real machine. |
| MMU | Loads SDR1 and turns translation on, leaving BATs alone. | Firmware leaves identity DBATs at `0x00000000`, `0x80000000`, `0xc0000000` and `0xf0000000`. BATs win over the HPT, and `0xc0000000` is our kernel base. |
| Firmware lifetime | Never calls `quiesce`; passes the entry point to the kernel. | Firmware must be stopped before the OS owns the MMU. |
| Console | Firmware `stdout` until kernel entry. | `stdout` may be the screen; the serial port must be found and used natively. |
| Device addresses | Kernel uses QEMU constants: PCI I/O base `0xfe000000`, COM2 at `0x2f8` (`Pegasos2/HardwareAddresses.h`) for debug output, PIC, RTC and i8042. | Must come from the firmware tree. |
| RTAS | Not passed to the kernel, so RTAS shutdown/reboot is unused. | Instantiate RTAS before `quiesce` and pass it on. |
| Image size | 6.1 MB with KUBSAN and the kernel gzip-compressed (2.4 MB without KUBSAN). | SmartFirmware disk reads are slow (Haiku saw pauses of one to two minutes). |

The kernel itself does not call Open Firmware after entry. PCI bridge windows,
the MV64361 base, memory ranges and the timebase are already read from the
firmware tree.

## Boot media

SmartFirmware boots with `boot <device> <file>` and understands Amiga RDB
partition tables (zero-based partition numbers, for example
`boot ide:0 serenity.elf`). Haiku booted from a disposable SD card in an
SD-to-IDE adapter: a 32 MiB Amiga FFS partition holds the loader and a second
partition holds the OS.

Plan:

1. **Card layout (done):** an RDB table with partition 0, a 32 MiB Amiga FFS
   (`DOS\3`) partition holding `serenity.elf`, followed by an ext2 root
   partition (`LNX\0`, not mounted by AmigaOS). `Meta/PowerPC/make-pegasos-card.py
   card` builds the whole image from the loader and an ext2 root image, and
   `boot-only` replaces only `serenity.elf` on an existing card. amitools'
   `rdbtool` and `xdftool` accept the result.
2. **Loader:** boot it with `boot ide:0 serenity.elf`. This is enough for
   phases 1 to 4.
3. **Root filesystem (done):** the adapter holds a single card, so the kernel
   finds the ext2 partition through the RDB. `LibPartition` reads RDB, and
   `PowerPCRDBCardBootCheck` boots the card image on the VT8231 IDE controller
   with `root=lun3:0:0;part1`.
3. **Load time:** `serenity.elf` is the loader plus the kernel ELF, which is
   embedded gzip-compressed and inflated by the loader (with a CRC-32 check)
   before it is loaded; the userland lives on the ext2 root partition. Its size
   depends on the kernel's undefined behaviour sanitizer (KUBSAN), which adds
   checks to most code and a relocated descriptor for each check:

   | Image | KUBSAN | Kernel ELF | `serenity.elf` |
   | --- | --- | --- | --- |
   | Debug (used during the port) | on | 26.8 MB | 6.1 MB |
   | Release | off | 14.8 MB | 2.4 MB |

   KUBSAN stays on while the port is being developed, accepting the larger and
   slower-to-load image. A first public release will build both a debug image
   and a release image, the latter configured with
   `-DENABLE_KERNEL_UNDEFINED_SANITIZER=OFF` in a separate build directory.
   xz would bring the debug image to about 3 MB if loading it becomes the
   bottleneck, at the cost of a larger decoder.

Keep an image of the card as it was before the first write, so Haiku can be
restored.

## Serial console

With no keyboard attached, SmartFirmware's console is the serial port, so the
firmware banner and everything the loader writes through firmware `stdout`
reach the host terminal. That stops at `quiesce`. From then on the loader and kernel
write to the UART directly:

- Find the UART the firmware console uses before `quiesce`, and keep its
  configuration (speed, format, FIFO). Only poll the line status register and
  write the transmit register, so the host terminal settings never have to change.
- Switch the loader's own output to the native UART before `quiesce` and print
  a marker on both paths, so a silent board can be told apart from a lost
  serial setup.
- Pass the UART address to the kernel, whose `serial_debug` output must use the
  same port from its first message.

## Phases

Each phase ends at a point that can be checked over serial. The loader's
`serenity-loader-stop-before-kernel` boot argument already provides one stop
point; add more as needed.

### 1. Loader runs and talks

- Select a SmartFirmware backend when the firmware is not VOF (for example by
  `real-mode?` and the firmware's identity properties); keep the VOF path as is.
- Wrap every client call: switch to real mode (IR/DR off) through a trampoline
  at an identity-mapped address, call, restore MSR. Pass only real addresses.
- Print a banner through firmware `stdout`, and verify it on the screen and on
  serial.

Checkpoint: banner, firmware identity, CPU (PVR) and memory ranges printed.

Status (2026-10-14): implemented and the initial physical firmware survey
started. The loader reads
`/options` `real-mode?` and, when it is true, makes every later client call
through `powerpc_loader_call_firmware_real_mode` in `boot.S`, which clears IR
and DR around the call. It prints the firmware kind, the call mode, the entry
MSR, the PVR, the console path, `bootpath` and `bootargs`. The complete device
tree is printed only when `serenity-loader-firmware-dump` is supplied in
`bootargs`; the physical survey already captured it. On firmware without
`/chosen` `qemu,boot-kernel` it calls `exit` before kernel allocation or MMU
change. On the physical Pegasos 2, SmartFirmware restarts after that call;
it does not return directly to the existing prompt. In QEMU, the
`serenity-loader-firmware-dump` and `serenity-loader-real-mode-calls` boot
arguments exercise the dump and the trampoline; the boot then continues
normally. The earlier survey image has been retired; the one current SD card
image is `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img`.

The first Pegasos 2 serial capture reached the ATI display node's large `rom`
property. The loader kept printing numbers there, so the machine was reset
before the tree-end or `exit` marker. The printer had trusted the length
returned by `getprop` even though it supplied a 128-byte buffer. Its output
is now bounded by the supplied capacity and still reports the full property
length. A host regression reproduced the excessive output before the fix and
passed afterward; both QEMU loader targets pass. The card image above has
been updated with the fixed loader. The second physical run printed a bounded
`rom` property (`0x00020000` bytes total), continued through the Radeon
`assigned-addresses`, printed `firmware tree end` and the stop message, then
SmartFirmware printed `Program terminated!` and restarted. This completes the
read-only survey checkpoint. The firmware restart is the observed behavior of
the `exit` client service here; it did not indicate a kernel crash.

The first capture established SmartFirmware 1.2 (`Pegasos2,1.2`), entry MSR
`0x00003030`, CPU PVR `0x00083311`, console path `/failsafe`, and boot path
`/pci@80000000/ide@C,1/disk@0,0`. `/memory@0/reg` is 512 MiB;
`available` contains `0x00000000`–`0x01000000` and
`0x01800000`–`0x0fc00000`. The south PCI bridge reports I/O window
`0xfe000000` and interrupt acknowledge `0xf1000cb4`; the other PCI bridge
reports I/O window `0xf8000000`. An ISA serial node exists at I/O port
`0x2f8`, and the Radeon has PCI ID `1002:5964`. Its firmware-assigned BARs
are framebuffer `0xc0000000` (256 MiB), I/O `0xf8001000` (256 bytes),
register aperture `0xd0000000` (64 KiB), and ROM `0xd0020000` (128 KiB).
These are observations for phase 2, not yet passed to the kernel.

### 2. Capture the firmware contract, then quiesce

- Choose memory from `/memory` `available` and `claim` exact addresses; never
  use address-unspecified claims.
- Before claiming space for the kernel, run a small exact-address probe. It
  selects a 4 MiB-aligned page from the largest available range above the
  loader, claims 4 KiB at that address, then releases it. The probe card is
  `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img`.
  QEMU's forced real-mode path selected `0x00800000` and reported successful
  claim and release. On the physical memory map captured above, the expected
  candidate is `0x01800000`; only the hardware run can confirm that claim.
- Record everything the kernel needs into `BootInfo`: memory and reserved
  ranges, timebase, PCI bridges (existing), interrupt acknowledge (existing),
  PCI I/O window base, serial port (from the chosen console path or the VT8231
  node, with its clock), display node properties (read without opening the
  `screen` package; opening it re-runs the Radeon BIOS), boot path and
  arguments.
- Instantiate RTAS and pass its entry, base and size; the kernel's
  `RTASCall.S` and `PowerState.cpp` already know how to call it.
- Initialise the native UART, print a marker, call `quiesce`, print a second
  marker. No firmware call after this point.

Checkpoint: both UART markers, and a dump of the captured `BootInfo`.

The exact-address claim probe is the next physical checkpoint. The latest
physical log went directly from `firmware tree end` to the stop message, with
no claim-probe marker or error. That matches the earlier survey-only loader;
the card image used for that boot has not been identified. The one current
image skips the dump by default and prints `exact RAM claim probe starting`
before reading `available`. Its SHA-256 is
`d0eaa7c1ab82018df04a82ced1cc1d04282f83bde4c0a6036a98f12d0f682bbf`.
The FFS `serenity.elf` was extracted and run in QEMU: it printed the start,
candidate, success and release markers without a tree dump. It still calls
`exit` before any MMU change or kernel entry, so the expected final firmware
behavior is the same restart observed during the survey. The full memory,
UART, RTAS and `quiesce` handoff in this phase remains to be implemented.

### 3. Take over the MMU

- Invalidate the four IBATs and four DBATs left by firmware before loading
  SDR1 and enabling translation. (The 750CXe has no extended BATs; 74xx G4
  modules would also need BATs 4-7, but none is available for testing.)
- Keep the existing high-alias and HPT probes; they now prove the firmware BATs
  are gone.
- Reserve firmware memory that stays in use until the kernel's own exception
  vectors are installed.

Checkpoint: `high kernel alias passed` and `boot handoff validated` on the real
machine.

### 4. Kernel without QEMU constants

- Replace `PEGASOS2_PCI_IO_PHYSICAL_BASE` and `PEGASOS2_UART_PHYSICAL_BASE`
  uses (debug output, legacy PIC, RTC, i8042) with `BootInfo` values. Keep the
  QEMU values only as an explicit VOF fallback.
- Confirm firmware BAR assignments are preserved and the VT8231 PIRQ routing
  matches what SmartFirmware programs.
- Use RTAS for shutdown and reboot.

Checkpoint: early kernel log over serial, PCI enumeration, timer and interrupts.

### 5. Devices on real hardware

In this order, each with its existing QEMU check as a reference:

1. IDE: find the ext2 partition through the RDB, mount it read-only, run
   checksum reads, then enable writes (`PowerPCIDEBootCheck`).
2. Input: PS/2 keyboard and mouse (`PowerPCPS2InputCheck`), UHCI USB
   (`PowerPCUSBInputCheck`).
3. Display: Radeon mode set on the 9200 SE over DVI. Collect the
   `Radeon: Output registers` log lines (`PowerPCRadeonDesktopCheck`).
4. Audio (VIA AC'97) and networking.

Checkpoint: desktop on the monitor, typing and mouse work, files survive a
reboot.

### 6. Reliability

Cold and warm boot, shutdown, repeated filesystem writes, and a documented
procedure for preparing the SD card.

## Open questions

- The exact SmartFirmware version; record it from the firmware banner in the
  first serial log.
- Which serial port SmartFirmware uses for its console, and at what speed; the
  chosen `stdout` path in the first serial log should answer it.
- The SD card's capacity, which bounds the root partition size.
