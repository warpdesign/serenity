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

## Loader handoff and remaining hardware checks

`Kernel/Arch/powerpc/Loader/` is one ELF containing the kernel
(`_binary_kernel_elf_*`). It now has a native SmartFirmware handoff path:

| Area | Current behaviour | Remaining physical check |
| --- | --- | --- |
| Client calls | Uses the real-mode trampoline when `/options/real-mode?` is true. | Calls after the completed survey, especially `quiesce`. |
| Memory | Native path claims the ELF staging area, loaded kernel and HPT at exact addresses selected from `/memory@0/available`. | Confirmed through kernel memory initialization on the physical board. |
| MMU | Turns translation off after `quiesce`; the HPT setup replaces the firmware BATs with identity RAM and PCI I/O mappings before enabling translation. | Confirmed through kernel scheduler startup on the physical CPU. |
| Firmware lifetime | Captures the needed firmware properties, then calls `quiesce` before MMU takeover. | Confirmed by kernel execution after the physical `quiesce` call. |
| Console | Uses firmware `stdout` before `quiesce`, then polls the native UART without changing its line settings. | Physical kernel messages arrived on the native serial console. |
| Device addresses | Validates that the firmware's PCI I/O and MV64361 bases match the kernel's current Pegasos 2 constants. | Remove the remaining constants after the first native boot. |
| RTAS | The loader instantiates RTAS before `quiesce` and the kernel reads physical PCI IDs through it. | Power-service calls and long-running PCI use on physical hardware. |
| Image size | 6.1 MB with KUBSAN and the kernel gzip-compressed (2.4 MB without KUBSAN). | SmartFirmware disk reads are slow (Haiku saw pauses of one to two minutes). |

The kernel itself does not call Open Firmware after entry. PCI bridge windows,
the MV64361 base, memory ranges and the timebase are already read from the
firmware tree.

The 2026-10-15 physical boot reached `init_stage2`. PCI resource assignment
then failed and aborted PCI registration, leaving no IDE disk or graphics
device. Haiku's local physical Pegasos port uses SmartFirmware RTAS for PCI
configuration and preserves the BARs assigned by firmware. The next Serenity
image follows that approach. The change compiles but awaits a physical boot;
QEMU boot tests were skipped at the user's request.

The first RTAS image reached kernel PCI startup and then faulted before its
first RTAS PCI call. The kernel's `boot_rtas_info()` re-read the loader's
reservation array through physical address `0x007c3268`, which was no longer
mapped. The next image validates that reservation in the loader, then uses
only the copied RTAS fields after kernel memory initialization. The next
physical log should show the RTAS south-bridge and IDE PCI identifiers.

The next physical log reached the RTAS read but got `ffff:ffff` for the
VT8231, so PCI registration still stopped. The next build records the RTAS
service status and result, then checks direct MV64361 config access as a
fallback. It accepts that fallback only if both the south bridge and IDE PCI
IDs match the captured firmware tree. Neither native path reassigns the
firmware BARs.

Comparison with Haiku's physical Pegasos RTAS caller and Linux's CHRP RTAS
PCI caller showed the same return convention used by Serenity: the first
return cell is status and the second is the PCI value. Haiku maps the RTAS
argument buffer as regular RAM, and Linux places its argument block in kernel
RAM. Serenity was mapping its physical RAM argument page with cache-inhibited,
guarded I/O attributes. The next build uses normal RAM attributes for both
PCI and power-service RTAS calls. The physical outcome is still pending.

The user subsequently booted the preceding image. Its RTAS result cells
remained `ffffffff`, but direct PCI fallback identified and enumerated the
VT8231, IDE, USB, audio, and Ethernet functions. Direct AGP display access
still returned `ffff:ffff`. Serial output stopped during UHCI initialization
after `Interrupt line: 9`; the user confirmed it did not continue. The UHCI
driver now enables I/O decoding and bus mastering and times out its stop, reset, and start
polls, so an unresponsive controller cannot indefinitely block IDE probing.
The cacheable RTAS mapping and UHCI fix are both awaiting a physical boot.

That physical boot succeeded through RTAS PCI enumeration, both UHCI
controllers, Radeon DVI mode set at 1024×768/60, IDE adapter detection, RDB
partition discovery, ext2 root mount, and `/init`. It then logged `Entropy
starvation...` and displayed a static white mark on black. No physical RNG is
registered in the port. The next image offers an explicit
`weakrng` boot argument for development only; it mixes interrupt
timing into the kernel pool while the periodic fallback stays disabled by
default. External interrupt timing is collected as it is on x86. The RNG
wait uses a predicate-based queue, releasing its lock while sleeping without
losing wakeups. Use `boot ide:0 serenity.elf weakrng` for the next
physical test; the [Pegasos II firmware guide](https://www.nxp.com/docs/en/application-note/AN2738.pdf)
documents this argument syntax.
The Pegasos has a PS/2 mouse but no PS/2 keyboard. Failed-keyboard recovery
previously exited before the mouse probe; it now disables the empty keyboard
port and continues. The user plugs the USB keyboard in after firmware boot.
The Radeon driver currently uses a fixed safe 1024×768 mode and synthetic
EDID with 1024×768 preferred; the monitor's native 1920×1080 mode is still
an open display task.

The physical VIA AC'97 driver reported 1040 Hz after the generic audio
channel requested 44,100 Hz. Linux `via82xx` and Haiku `auvia` mark
primary-codec commands valid; Serenity omitted that bit. The compiled fix
sets it for reads and writes, waits for a settled read, and rejects an
invalid DAC-rate readback. The updated SD image now includes this fix;
physical audio behavior is pending.

The next physical boot proved the entropy pool became ready and the PS/2
mouse initialized, but `/init` then failed with `MM: Unable to commit 3
pages, have only 1`. The physical memory map supplies about 183 MiB of usable
RAM. The Radeon BAR advertises 128 MiB, and the shared framebuffer allocates
an equally sized RAM buffer for fake console writes. The compiled Radeon
change caps its mapping at 32 MiB and logs the remaining commitment budget.
`/memory/reg` reports 512 MiB installed, while `/memory/available` lists
244 MiB. The kernel's default map uses `available`. The loader now accepts
`fullram` to use `reg` for the kernel map
after quiesce, while keeping its firmware claims in `available`. Linux and
Haiku use `reg` for physical RAM. Both paths can be tested with one SD image;
the user subsequently confirmed that `fullram` lets SystemMonitor run on the
board.

The next physical boot reached the desktop with the conservative RAM map.
The display looked bluish and startup was slow: `/init` at 10.126 s,
WindowServer's main loop at 24.275 s, LoginServer's user session at
34.330 s, and Taskbar/Terminal loading at about 44 s. The native RV280
driver set `SURF_TRANSLATION_DIS` alongside the byte-swap bit. Linux
radeonfb documents that the former disables the swapper on later Radeon
chips. The next card image clears stale surface descriptors, keeps that bit
clear on native big-endian Radeon hardware, and logs `SURFACE_CNTL` for the
next serial capture. The IDE path uses polled PIO; its contribution to the
startup delay remains unmeasured.

The user confirmed the Radeon colors are fixed. Hot-plugging the USB
keyboard then reached root port 2, but its first device descriptor returned
type zero and enumeration failed with `EIO`. UHCI's queue-head and transfer-
descriptor pools used cacheable memory, unlike its frame list and data buffer.
The updated loader uses cache-inhibited DMA pages for both pools, waits through
temporary NAKs, returns only received control data bytes, and bounds the
control-transfer wait. The physical keyboard test should show
`USB HID: Initialized interface` with no device-descriptor error. No QEMU
verification was run.

The following physical boot recognized the USB keyboard, but typing was
erratic rather than delayed: separate Backspace presses, Space, and Return
were often ignored. UHCI's asynchronous interrupt queue reused the same
transfer descriptor without alternating its USB DATA0/DATA1 toggle. The
current image rearms the TD with the next toggle only after a successful
report and clears stale completion bits; retries keep the previous toggle.
The host toggle regression and kernel build pass. On the next physical boot,
test separate repeated keys and verify that release reports reach HID. The
The user confirmed SystemMonitor runs with `fullram`; the new keyboard toggle
fix still needs physical typing confirmation. The same card can test both.

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
`halt` boot argument already provides one stop
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
tree is printed only when `ofdump` is supplied in
`bootargs`; the physical survey already captured it. The original survey
loader called `exit` before kernel allocation or MMU changes. On the physical
Pegasos 2, SmartFirmware restarted after that call. In QEMU, the
`ofdump` and `realmode` boot
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
- A 4 KiB exact-address claim at `0x01800000` succeeded and was released on
  the physical machine. The current loader uses the same selection rules for
  the decompressed kernel ELF, loaded kernel span and 4 MiB HPT, excluding its
  previous claims. It keeps those allocations until kernel entry.
- Record everything the kernel needs into `BootInfo`: memory and reserved
  ranges, timebase, PCI bridges (existing), interrupt acknowledge (existing),
  PCI I/O window and UART. The current loader captures the timebase, MV64361
  base and PCI ranges before `quiesce`, and validates that the I/O and UART
  addresses match the kernel's current Pegasos 2 constants. Dynamic kernel
  addresses and display properties remain to be added.
- Instantiate RTAS and pass its entry, base and size; the kernel's
  `RTASCall.S` and `PowerState.cpp` already know how to call it.
- Print directly through the native UART, call `quiesce`, disable translation,
  and keep printing through that UART. No firmware call after this point.

Checkpoint: `native UART before quiesce`, `firmware quiesce complete`,
`high kernel alias passed`, `boot handoff validated`, and the first kernel
serial message on the physical machine.

The loader's forced native path boots the QEMU Pegasos 2 desktop from the RDB
IDE card, reaches WindowServer, writes a file and leaves ext2 clean. It also
works without an explicit `root=` argument: the loader supplies
`root=lun3:0:0;part1` when booting the native path. Quick 750 and normal
750CXe tests are `PowerPCNativeCardBootCheckQuick` and
`PowerPCNativeCardBootCheck`. QEMU does not reproduce SmartFirmware's entry
MSR or physical UART behavior, so the current card image attempts all of
these stages in one physical boot. Passing `stop` in `bootargs` stops after
the three allocations on the same card; `ofdump` remains an optional tree
dump. RTAS and removing the remaining fixed kernel device addresses can
follow the first native kernel boot.

The one current image is `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img`,
SHA-256 `198ef2096243943241acb2613e81e6493b5c4c52d894c043d6a7f1898f3709e8`.
The `serenity.elf` extracted from its FFS partition matches the rebuilt
loader. This is the only SD card image to write for the combined hardware
attempt.

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
