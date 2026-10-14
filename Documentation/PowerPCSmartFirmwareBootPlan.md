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
| Image size | 6.1 MB with the kernel gzip-compressed (26.8 MB uncompressed: `.ksyms` 8 MB mostly padding, `.text` 7 MB, `.rela.dyn` 5.4 MB). | SmartFirmware disk reads are slow (Haiku saw pauses of one to two minutes). |

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

1. **Card layout:** an RDB table with partition 0, a small Amiga FFS
   partition holding `serenity.elf`, followed by an ext2 root partition. A
   host-side recipe builds the whole card image (for example with amitools'
   `rdbtool`/`xdftool` for the RDB and FFS parts, and `mke2fs` for the root),
   and a second command replaces only `serenity.elf` for quick iterations.
2. **Loader:** boot it with `boot ide:0 serenity.elf`. This is enough for
   phases 1 to 4.
3. **Root filesystem:** the adapter holds a single card, so the kernel must
   find the ext2 partition through the RDB. Teach `LibPartition` to read RDB
   (it reads MBR, EBR and GPT today), and test it in QEMU with the same card
   image on the VT8231 IDE controller before using the physical card.
3. **Load time:** `serenity.elf` is the loader plus the kernel ELF, which is
   embedded gzip-compressed and inflated by the loader (with a CRC-32 check)
   before it is loaded. That brings the file from 26.8 MB to 6.1 MB, close to
   MorphOS's self-decompressing 1.7 MB `boot.img`; the userland lives on the
   ext2 root partition. xz would save another 3 MB if disk reads are still too
   slow, at the cost of a larger decoder and slower inflation on the G3.

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

### 2. Capture the firmware contract, then quiesce

- Choose memory from `/memory` `available` and `claim` exact addresses; never
  use address-unspecified claims.
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
