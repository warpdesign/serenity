# SerenityOS on PowerPC Pegasos 2: current status

Status checked on 2026-10-16.

The goal is to port SerenityOS to the Pegasos 2, a 32-bit PowerPC G3/G4
machine. The port currently boots and runs a graphical SerenityOS desktop on
QEMU's `pegasos2` machine. The work is on the
`powerpc32-pegasos2-bringup` branch. The earlier bring-up record in
[PowerPCPegasos2.md](PowerPCPegasos2.md) documents the initial firmware
probe and is historical.

## Working in QEMU

- The Open Firmware loader starts the PowerPC kernel with hashed page-table
  mappings, exceptions, interrupts, timers, PCI, virtio storage, graphics,
  input, networking, and entropy. It embeds the kernel gzip-compressed and
  inflates it with a CRC-32 check, so the loader file is 6.1 MB instead of
  26.8 MB.
- The 32-bit big-endian userland boots WindowServer, Taskbar, Terminal, and
  other desktop utilities. Browser renders a local HTML page.
- DHCP, DNS resolution, and the Pegasos 2 RTC pass the Browser smoke check.
- PixelPaint opens with its default palette and canvas, draws a brush stroke,
  and saves a valid 510×356 project through FileSystemAccessServer.
- ImageViewer, VideoPlayer, and SoundPlayer build for PowerPC and open small
  image, video, and audio samples in QEMU.
- Calendar renders its month grid, and TextEditor opens and saves a text file
  in the PowerPC desktop check. The earlier sound demo image omitted
  `/bin/CalendarSettings`, so Calendar exited during `unveil` before its
  window appeared. The desktop image recipe now includes that executable,
  TextEditor, and their required portal services.
- The Pegasos 2 VIA AC'97 output driver creates `/dev/audio/0`; QEMU captures
  nonzero PCM when SoundPlayer plays a WAV tone.
- The shared i8042 driver detects the VT8231's PS/2 keyboard controller,
  reached through the PCI I/O window. Booted without VirtIO input devices,
  the PS/2 mouse moves the cursor and the PS/2 keyboard types into Terminal.
- The VT8231 IDE controller (`1106:0571`) runs in PCI native mode with polled
  PIO and LBA48. The desktop boots with its root filesystem as the only disk,
  attached to the IDE primary master, and writes leave a clean ext2 image.
  An opt-in bus-master DMA path is built but has not completed a physical
  transfer successfully yet; PIO remains the physical boot default.
- LibPartition reads Amiga RDB partition tables. `Meta/PowerPC/make-pegasos-card.py`
  builds the SD card layout planned for SmartFirmware: an RDB with an FFS boot
  partition holding the loader and an ext2 root partition. Booted on the IDE
  disk with `root=lun3:0:0;part1`, the desktop runs from the ext2 partition.
- The loader can survey native Open Firmware: it prints the firmware identity,
  entry MSR, PVR, console and boot properties, and dumps the device tree when
  `ofdump` is requested. The physical survey completed, and a later 4 KiB
  exact-address claim at `0x01800000` succeeded and was released. The current
  native path allocates kernel staging, the loaded image and HPT from
  `/memory@0/available`, captures firmware settings, calls `quiesce`, switches
  to direct UART output, sets up its MMU, and enters the kernel. That combined
  path has reached the desktop in QEMU. On the physical board it now enters
  the kernel and reaches storage discovery.
- An R100/R200-family Radeon driver sets 1024x768x32 on CRTC 1, keeping the
  output routing left by the video BIOS, and programs the pixel PLL from the
  expansion ROM's PLL table or radeonfb's defaults. QEMU's `ati-vga,model=rv100`
  shows the desktop with correct colours. QEMU only implements the Rage 128
  `CONFIG_CNTL` aperture swap, so the driver uses it for that device and
  `SURFACE_CNTL` on real cards. If the video BIOS enabled the internal TMDS
  transmitter, the driver also drives that DVI output from CRTC 1 without RMX
  scaling, as Haiku does; external TMDS transmitters and LVDS panels are not
  handled. Each mode set logs the output routing registers. QEMU has no DVI
  output, and the Radeon 9200 SE in the physical Pegasos 2 has not run this
  driver yet.
- The HPT virtual alias is reserved in the kernel region tree so kmalloc
  expansion cannot overlap it. The diagnosis and red/green evidence are in
  [PowerPCKmallocCrashInvestigation.md](PowerPCKmallocCrashInvestigation.md).

These statements are backed by the PowerPC QEMU test targets below. Real
Pegasos 2 hardware has not been validated by these tests; the plan for booting
it from SmartFirmware is in
[PowerPCSmartFirmwareBootPlan.md](PowerPCSmartFirmwareBootPlan.md).

## Verification targets

Configure the PowerPC build, then run:

```sh
cmake --build Build/powerpc --target PowerPCKmallocHPTCheck
cmake --build Build/powerpc --target PowerPCDesktopProgramsCheck
cmake --build Build/powerpc --target PowerPCBrowserCheck
cmake --build Build/powerpc --target PowerPCPixelPaintCheck
cmake --build Build/powerpc --target PowerPCVIAAudioCheck
cmake --build Build/powerpc --target PowerPCPS2InputCheck
cmake --build Build/powerpc --target PowerPCIDEBootCheck
cmake --build Build/powerpc --target PowerPCRDBCardBootCheck
cmake --build Build/powerpc --target PowerPCNativeCardBootCheck
cmake --build Build/powerpc --target PowerPCRadeonDesktopCheck
```

Each target has a `Quick` variant for a shorter run. The Browser target
tests local rendering and network configuration; it does not replay a live
Google search. The user's interactive sequence for the earlier allocator
crash is recorded in the kmalloc investigation document.

The RTAS PCI address contract has a host-only quick/normal target:
`PowerPCRtasPCIContractCheckQuick` and `PowerPCRtasPCIContractCheck`. After a
physical boot, set `PEGASOS2_NATIVE_SERIAL_LOG` to its capture and run
`PowerPCNativePCIHardwareCheckQuick` for PCI IDs and storage discovery, or
`PowerPCNativePCIHardwareCheck` to require the first user process too. The
recorded 2026-10-15 PCI failure is rejected by that checker.
`PowerPCNativeRTASHardwareCheckQuick` and `PowerPCNativeRTASHardwareCheck`
additionally require the physical boot to use RTAS rather than the guarded
direct-config fallback.
`PowerPCNativeUHCIBootCheckQuick` and `PowerPCNativeUHCIBootCheck` require the
first UHCI controller to complete initialization or report a bounded timeout
before storage and, for the normal target, userland startup.
`PowerPCNativeUSBKeyboardCheckQuick` and `PowerPCNativeUSBKeyboardCheck`
require a hot-plugged keyboard to enumerate and its HID interface to
initialize; their self-test rejects the observed zero descriptor.
`PowerPCNativeEntropyBootCheckQuick` and `PowerPCNativeEntropyBootCheck`
check the explicitly enabled bring-up entropy pool and, for the normal target,
WindowServer startup. `PowerPCNativePS2MouseCheckQuick` and
`PowerPCNativePS2MouseCheck` check that a missing PS/2 keyboard does not
prevent the attached PS/2 mouse from initializing.
`PowerPCNativeVIAAudioCheckQuick` and `PowerPCNativeVIAAudioCheck` reject the
observed 1040 Hz codec readback and require a rate near 44.1 kHz.
`PowerPCVIAAC97CodecContractCheckQuick` and
`PowerPCVIAAC97CodecContractCheck` verify the VIA command words on the host;
both pass without starting QEMU.
`PowerPCRadeonFramebufferBudgetCheckQuick` and
`PowerPCRadeonFramebufferBudgetCheck` check the framebuffer mapping cap on
the host. `PowerPCNativeDesktopMemoryCheckQuick` and
`PowerPCNativeDesktopMemoryCheck` check the physical memory budget and reject
the userland out-of-memory failure.

## SmartFirmware probe card

On 2026-10-14 this host rebuilt the 6.1 MB loader and passed
`PowerPCFirmwareLoaderCheckQuick`. A separate QEMU firmware survey printed
the firmware tree start and end
markers and reached `boot handoff validated`; its trace is
`/tmp/serenity-smartfirmware-vof-20261014.log`.

`PowerPCRDBCardBootCheckQuick` also passed: QEMU direct-loaded the loader,
booted the desktop from the card's ext2 partition on IDE, wrote a 108,894-byte
file, and left a clean filesystem. That test does not exercise SmartFirmware
loading `serenity.elf` from FFS. The one current local card image is
`Build/powerpc/PowerPCBoot/pegasos2-sdcard.img` (929 MiB).
Its SHA-256 is
`54dfbe2d4a073b00c00dede91b0a23d4bfdfc0e9c02a379c5bfd081db3e167d6`.
The independent `rdbtool` sees `SERBOOT` as a 32 MiB bootable FFS partition
and `SERROOT` as an 896 MiB ext2 partition. Extracting `serenity.elf` from
FFS matched the rebuilt loader byte for byte.

Short boot arguments are `weakrng` for development entropy, `fullram` for
the optional installed-RAM map, `halt` to stop after validating the handoff,
and `ofdump` for the firmware tree. Diagnostic QEMU paths use `realmode`,
`claimram`, and `native`. `stop` remains the earlier native allocation stop.

The physical 4 KiB claim probe subsequently printed candidate `0x01800000`,
success and release markers. The current loader now claims three real memory
regions, loads and relocates the kernel, prepares the HPT, captures firmware
settings, switches to direct UART output around `quiesce`, and enters the
kernel by default. The same card accepts `stop` in `bootargs` to stop after
allocations, or `ofdump` to show the already captured device tree. QEMU's
forced native path boots the desktop from IDE with no explicit `root=` and
writes a 108,894-byte file with a clean ext2 result on both 750 and 750CXe.
The FFS `serenity.elf` in that card image matches the rebuilt loader byte for
byte. That physical boot reached `init_stage2`, then failed at PCI resource
assignment. The canonical image was updated afterward with the RTAS PCI
change; its FFS `serenity.elf` matches the new loader byte for byte.

The 2026-10-15 physical serial log confirms large RAM claims, kernel
decompression and relocation, HPT setup, firmware `quiesce`, native serial
output, kernel memory initialization, the scheduler and RTC. The first failure
was `PCI: Failed to assign Pegasos 2 device resources`; with no PCI controller
registered, i8042, graphics, audio and IDE were all absent. Root mounting
panicked after ten seconds. The local Haiku `pegasos2-support` port uses RTAS
PCI configuration on physical SmartFirmware and preserves firmware BARs, while
using direct MV64361 config access and BAR setup in QEMU. Serenity now
instantiates RTAS before `quiesce` and follows that physical PCI split. The
kernel and loader compile and the host RTAS PCI contract test passes. No QEMU
boot was run for this change at the user's request. The new physical behavior
remains unverified until the next card boot.

The next physical boot did instantiate and reserve RTAS at `0x0318a000`–
`0x0318dfff`, then faulted in `boot_rtas_info()` at kernel PCI startup. The
faulting load dereferenced `0x007c3268`, a pointer into the loader's temporary
reservation array after its low mapping was removed. RTAS itself had not yet
been called. The loader now checks the reservation while that array is mapped;
later PCI and power-service users validate the copied RTAS fields without
reading the stale memory-map pointer. A regression in `TestBootInfo.cpp`
checks that late RTAS lookup works with the reservation pointer unavailable.
The card's FFS loader for that boot matched the rebuilt ELF byte for byte.

The following physical boot passed the late handoff check and did make its
first RTAS PCI read, but the south bridge ID came back `ffff:ffff`. The kernel
again found no storage device. The returned ID alone cannot distinguish an
RTAS service error from an empty config address. The next build logs the RTAS
status and result. If RTAS cannot identify the VT8231, it tries the MV64361
direct-config path and continues only if that path identifies both the VT8231
and VIA IDE controller. Firmware BARs remain untouched in either native path.
This guarded fallback is compiled and its serial-log checker self-test passes;
the physical result is pending. No QEMU boot was run.

A source comparison found that the RTAS return-cell indexing was already the
same in Serenity, Haiku and Linux: status first, PCI value second. The
important difference was the argument page mapping. Haiku maps the RTAS
argument page as ordinary RAM; Linux uses a static kernel RAM argument block.
Serenity had mapped that RAM page with I/O attributes. The next build maps it
as normal cacheable RAM for both PCI and power-service calls. This is a
plausible cause of the `ffff:ffff` result, but only the next physical boot can
confirm it. The RTAS-specific native hardware check will distinguish success
from a boot that used direct PCI fallback.

The user then booted the preceding fallback image, not the new cacheable-RTAS
image. RTAS left both return cells at `ffffffff`; direct configuration found
the south bridge (`1106:8231`) and IDE (`1106:0571`), routed two PCI lines,
and enumerated all south-bridge functions. The AGP display still read
`ffff:ffff` through that direct path. Serial output stopped after the first
UHCI controller's `Interrupt line: 9` message. UHCI did not enable PCI I/O
decoding and had unbounded waits in stop, reset, and start. The next build
enables I/O decoding and bus mastering and bounds those waits; a nonresponding controller can
then report an error and let IDE discovery continue. The native serial-log
regression rejects the observed UHCI stop before storage discovery. The new
cacheable-RTAS path and this UHCI change have not run on physical hardware.

The next physical boot confirmed both fixes. RTAS returned status zero and
identified the VT8231 (`1106:8231`), VIA IDE (`1106:0571`), and Radeon
(`1002:5964`). Both UHCI controllers started. The Radeon drove DVI at
1024×768, 60 Hz; the SD adapter and both RDB partitions were found; ext2
mounted; and `/init` started. The final serial lines were `Entropy
starvation...` and a later ext2 sync. The display showed a black screen with
a static white mark, so the desktop had not appeared.

The physical board has a PS/2 mouse but no PS/2 keyboard; its USB keyboard is
plugged in after firmware boot. The i8042 driver exited before probing the
mouse because failed-keyboard recovery sent another command to the absent
keyboard. It now disables only that port and continues to the mouse.

No physical RNG is registered. An opt-in `weakrng` boot argument now
mixes interrupt timing into the existing Fortuna pool for development boots.
The RNG wait also releases its spinlock before sleeping, so an interrupt can
wake it. Interrupt timing is not validated cryptographic entropy, and this
fallback is disabled by default. Use `boot ide:0 serenity.elf weakrng`
for the next physical test. The Radeon currently sets a fixed 1024×768 safe
mode and advertises that as the preferred timing in its generic EDID; native
1920×1080 selection remains to implement after the desktop starts. The
physical VIA Ethernet `1106:3065` has no driver yet, and AC'97 reported
1040 Hz output on this board; both remain open.

`AudioChannel::create()` requests 44,100 Hz. The physical VIA driver wrote
that rate but read back 1040 Hz. Linux `via82xx` and Haiku `auvia` both set
the primary-codec-valid bit in AC'97 read and write commands; Serenity had
omitted it. The compiled driver now sets the bit, allows a read to settle,
and rejects an invalid DAC-rate readback after three attempts. The native
audio log regression rejects the observed 1040 Hz. This fix was later
included in the updated SD image. The host VIA codec-command contract check
passes. Physical audio behavior remains unverified; no QEMU run was made.

The user's next physical boot confirmed that the insecure timing pool became
ready and the PS/2 mouse initialized. Both UHCI controllers, Radeon DVI,
RDB disk, ext2 root, and `/init` also worked. `/init` then failed to map a
writable page for its dynamic loader: `MM: Unable to commit 3 pages, have
only 1`. Its memory map has about 183 MiB of usable RAM, while the Radeon
reports a 128 MiB framebuffer BAR. The shared framebuffer implementation
commits one RAM page for every mapped BAR page to support fake writes in
console mode; mapping all 128 MiB consumed most of the physical-page budget.
The Radeon driver now maps only the first 32 MiB of the aperture, enough for
the current 1024×768 mode and a future 1920×1080 scanout. It logs the
uncommitted-page count before and after the connector allocation. The
512 MiB installed RAM appears in `/memory/reg`. SmartFirmware's
`/memory/available` instead reports 16 MiB plus 228 MiB (244 MiB total),
and the loader currently uses that smaller property for the kernel map;
boot reservations and page metadata leave about 183 MiB. The reason the
upper 256 MiB is absent from `available` is not established. Linux and
Haiku use `reg` to describe RAM after the firmware handoff. The loader now
supports `fullram` as an opt-in boot argument:
it continues to claim the kernel and HPT from `available`, then hands the
full `reg` range to the kernel after firmware quiesce. Try the 32 MiB
framebuffer fix first with the default conservative map; the same SD image
can test the full installed range by adding that argument. Physical testing
must establish that the extra RAM is safe on this board.
The user authorized updating the existing card image. Its FFS `serenity.elf`
now contains the AC'97 fix, Radeon budget cap, optional installed-RAM map,
and short boot arguments. Extraction from FFS matched the
loader byte for byte. The root partition was not rewritten. The image SHA-256
is `54dfbe2d4a073b00c00dede91b0a23d4bfdfc0e9c02a379c5bfd081db3e167d6`.

The next physical boot reached the desktop with `weakrng` and the default
244 MiB firmware-available map. The user photographed a bluish desktop and
slow startup/repainting. The terminal's configured black `#000000` background
appears saturated blue, while the default desktop's dark gray `#505050` is
also blue. A big-endian opaque black ARGB pixel has bytes `ff 00 00 00`;
without aperture swapping, the Radeon interprets the alpha byte as blue.
This directly matches the image. The attached serial capture begins when
`/init` starts at 10.126 s. WindowServer entered its main loop at 24.275 s,
the user session started at 34.330 s, and Taskbar and Terminal started loading
at about 44 s.
The IDE driver uses polled PIO transfers; no timing breakdown yet separates
disk I/O, process loading, and framebuffer updates. The first appearance of
the pointer before Taskbar is consistent with this staged startup.
The user later clarified that the Terminal window frame stayed empty for at
least five seconds before its menu and contents appeared, followed by the
gray wallpaper and taskbar. The serial log shows Terminal starting at 44.085 s,
Shell at 49.668 s, and more Shell libraries loading through 58.995 s. The
Radeon framebuffer is mapped uncached and the current driver exposes no
hardware blitter or flush path. There is no measured CPU/disk/PCI breakdown,
so performance work should start with timing those stages rather than assuming
one cause.

The physical RV280 driver requested 32-bit aperture byte swapping while also
setting `SURF_TRANSLATION_DIS`. [Linux radeonfb](https://github.com/torvalds/linux/blob/master/drivers/video/fbdev/aty/radeon_base.c)
notes that this bit disables the swapper on later Radeon chips. On big-endian
native Radeon hardware, the driver now clears stale surface descriptors and
enables swapping on both nonsurface apertures without translation disable.
QEMU's older CONFIG_CNTL swap path is unchanged. The new serial log prints
`Radeon: SURFACE_CNTL=...`; a native log regression requires swap bits 21 and
23 and rejects translation-disable bit 8. Host checks and the kernel build
pass, and the user subsequently confirmed the colors are fixed. The updated
card contains this fix, with only its FFS loader partition rewritten.

After plugging the USB keyboard in at runtime, UHCI root port 2 reported a
detach at 121.914 s, an attach at 122.923 s, then `GetDescriptor(Device)`
returned descriptor type `0x0` instead of `0x1` at 123.112 s; device creation
failed with `EIO`. The root hub is emulated, so this is the first physical
UHCI data transfer. Its queue-head and transfer-descriptor pools were normal
cacheable kernel pages even though the controller reads and writes them by
DMA. [Linux UHCI](https://github.com/torvalds/linux/blob/master/drivers/usb/host/uhci-hcd.c)
uses coherent DMA pools for both. The pools now use Serenity's
cache-inhibited DMA page allocator. Control transfer polling also no longer
counts a temporary NAK as completion, returns only data-stage bytes, and
times out rather than spinning forever. A short transfer copies only its
received bytes. New serial diagnostics distinguish timeout from transfer
failure, and the native USB keyboard regression rejects the observed log.
The loader and kernel build pass. A later physical boot confirmed keyboard
enumeration; the current card SHA-256 is
`54dfbe2d4a073b00c00dede91b0a23d4bfdfc0e9c02a379c5bfd081db3e167d6`.

Before writing that card, the user launched SystemMonitor on the conservative
RAM map. At uptime 238.294 s the kernel reported `MM: Unable to commit 1024
pages, have only 508`: a 4 MiB request with under 2 MiB uncommitted. The
app then failed `release_value_but_fixme_should_propagate_errors()` at
`AK/Error.h:252`. This confirms an out-of-memory abort, although the log alone
does not prove why the rest of the desktop appeared frozen. Memory bandwidth
does not explain the failed commitment. The already-built `fullram` boot option
uses the installed 512 MiB map and needs no card rewrite; test the new card
with `boot ide:0 serenity.elf weakrng fullram`, then try keyboard hot-plug and
SystemMonitor. If the full-RAM path fails, boot the same card without `fullram`.

With the keyboard now enumerating, typing was erratic: a Backspace press
removed one character, but a later press often did nothing; Space and Return
were also missed. When a key did appear, it was prompt, so this is not
input latency. The UHCI asynchronous interrupt path reused the same transfer
descriptor for every report without advancing its DATA0/DATA1 toggle.
The keyboard HID driver emits events only when its report state changes, so
a missed release report explains the one-time Backspace behavior. The driver
now advances the toggle only after a successful nonempty report, preserves
it on NAK/error, and clears stale TD status and retry state when rearming.
A host regression checks successive toggle and status values; the kernel
build passes. The existing card image was updated only in its FFS loader
partition; extraction matched the rebuilt loader. The fix still needs a
physical repeat-key test. Boot the new image with `weakrng fullram`, plug in
the keyboard after reaching the desktop, and test repeated letters, two
separate Backspaces, Space, and Return. The user subsequently confirmed that
SystemMonitor runs with `fullram`. Physical typing after the DATA0/DATA1 fix
remains unverified.

As of 2026-10-15, [upstream SerenityOS UHCI](https://github.com/SerenityOS/serenity/blob/master/Kernel/Bus/USB/UHCI/UHCIController.cpp)
also rearms the same interrupt TD without advancing its toggle. Its
[descriptor implementation](https://github.com/SerenityOS/serenity/blob/master/Kernel/Bus/USB/UHCI/UHCIDescriptorTypes.h)
only ORs the toggle bit when setting it. This fix is committed in the Pegasos
branch and has not been submitted upstream.

On 2026-10-16 a new image from another computer enabled bus-master IDE DMA,
word-at-a-time PowerPC memory routines, and `membench`. With `fullram weakrng`
it mounted ext2 and entered `/init`, but a DMA read of eight sectors at LBA
68136 stalled: drive status `0x58` still had DRQ, and bus-master status `0x21`
still had Active set without error or completion. The code switched to PIO
without resetting the outstanding drive command, so the same block failed
twice more and `/init` exited. The disk LED stayed lit. The user then booted
the same image to the desktop with `fullram weakrng nodma`, isolating the
immediate boot failure to IDE DMA rather than the new memory routines.

The rebuilt source now chooses UDMA2 when the drive advertises UDMA0–5 but
firmware selected none, instead of jumping to UDMA5. `udma=0` and `udma=5`
allow explicit modes on the same future image. On DMA failure the channel
performs an ATA software reset before retrying the request with PIO. The
one-page DMA allocator now zeroes pages with the memory type requested for
their final mapping, avoiding a dirty cacheable alias of an uncached PRD
table. [Linux libata](https://github.com/torvalds/linux/blob/master/drivers/ata/libata-sff.c)
uses the same reset mechanism after drive errors, and [pata_via](https://github.com/torvalds/linux/blob/master/drivers/ata/pata_via.c)
notes that firmware cable reporting needs care on non-PC VIA systems.
Diagnostic logs now include descriptor and bounce-buffer physical addresses
and PRD contents on failure. Host UDMA policy and physical boot-log regression
self-tests pass; the PowerPC kernel and loader compile. No QEMU run was made.

The local card image was older than the one built on the user's other computer:
it had neither `/bin/membench` nor an automatic benchmark service. A new ext2
root was built from current binaries with `/bin/membench` installed and no
boot service. The root-image test verifies both facts. Run `membench` from a
Terminal after the desktop is ready for RAM throughput, or `membench --verify`
for small aligned and overlapping memory operations. `membench -f
/dev/gpu/connector0 -d /dev/hda` also measures framebuffer and disk reads
when run with permission to access those devices.

The loader now accepts `/?`: `boot ide:0 serenity.elf /?` prints loader and
kernel bring-up options, then presents `boot>`. Type a new full option line,
for example `fullram weakrng nodma`, and press Enter to continue in the same
loaded image. Entering `exit` returns to SmartFirmware without using its
`exit` client service. The boot option line supports Backspace and can show
help again with `/?`. This interactive firmware path is built and host-tested,
but a physical prompt and return test is still needed.

The first rebuilt one-card image was `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img`
(929 MiB), SHA-256 `44ae3b8869c28135f33d0e81beb798ab32cb766a6587283508f9ef8a67a31d92`.
Its FFS loader and ext2 root were extracted/compared against the built inputs.
That first image subsequently booted the desktop with `fullram weakrng` after
DMA failed and reset recovered it. The current image below supersedes it.
The user also tested `fullram weakrng udma=0` on that image, as detailed
below; `fullram weakrng nodma` remains the known PIO boot path.

While the user writes that card, a separate USB keyboard issue was identified:
holding a key produces a single press. Commit `3ee3673b02` correctly advances
the UHCI DATA toggle and ignores NAK/empty transfers; it did not remove
typematic handling. The generic HID keyboard driver only emits an event when
a usage changes between input reports, and it has no repeat timer. The same
state-diff-only behavior is present in upstream SerenityOS (2026-10-16).
The HID driver source now schedules 500 ms initial and 40 ms subsequent
key-down repeats, stops them on release/disconnect, and excludes modifiers and
lock keys. Host repeat tests and a PowerPC kernel build pass. The card image
first written by the user did not include this fix. A subsequent image now
includes it for physical repeat testing.

The user's 2026-10-16 serial capture at
`/Users/leo/.t3/userdata/attachments/046af1a9-eae0-4cea-8013-ceea9167016f-637a902f-e672-4349-a2c1-71820db91d58-txt.txt`
shows a complete desktop boot with `fullram weakrng` and no `nodma`. The IDE
drive selected UDMA2 (VIA timing `0x17 -> 0xf4`), then a DMA read of eight
sectors at LBA 198360 failed at 14.154 s: drive status `0x23`, bus-master
status `0x21`, PRD buffer `0x05d77000`, 4096 bytes, EOT `0x8000`.
The channel reset returned drive status `0x50` and PIO fallback took over;
WindowServer entered its main loop at 32.811 s and the Shell started around
50 s. Thus the boot recovery works, but this run does **not** establish
reliable DMA. The physical boot-log regression passes for recovery and desktop
startup. A second run of that same card with
`boot ide:0 serenity.elf fullram weakrng udma=0` also reached the desktop,
but DMA failed again. It selected UDMA0 (`0x17 -> 0xf5`) and failed an
eight-sector read at LBA 353536 at 20.842 s, with drive status `0x73` and
bus-master status `0x21`; reset returned `0x50` and PIO fallback resumed.
The drive's IDENTIFY selected mask was `0x04` before UDMA0 was requested,
unlike `0x00` on the prior boot; why that changed is not established. The
second full capture is
`/Users/leo/.t3/userdata/attachments/046af1a9-eae0-4cea-8013-ceea9167016f-dbef3f0f-8325-4dd0-9203-61f8af166dcd-txt.txt`.
Both physical logs pass the recovery/desktop regression; neither proves
reliable DMA. Since UDMA0 also fails, transfer speed alone is insufficient
to explain the problem. The subsequent image records the ATA error register to
separate interface CRC or media errors from a controller-side stall.

The previous 929 MiB image at `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img`
has SHA-256 `ef9c13eb88454b5f468757f17fabe5c7f40b452840d554d4f0c737f73a23f4cb`.
It includes the HID key-repeat commit and logs the ATA error register on a
DMA failure. The ext2 root is the same as the first rebuilt image. Its boot
partition and root were compared to the built inputs. Its physical results
follow below.

On the next physical run, USB held-key repeat was confirmed working. The full
serial capture is
`/Users/leo/.t3/userdata/attachments/046af1a9-eae0-4cea-8013-ceea9167016f-f89554b6-ef4b-4683-b1c9-5b934ab8d2b4-txt.txt`.
Default UDMA2 again stalled an eight-sector read, this time at LBA 184032:
drive status `0x58` still had DRQ but no ERR, ATA error register `0x00`, and
bus-master status `0x21` still had Active. The timeout and reset returned
`0x50`; PIO carried the desktop boot onward. This is a DMA hang, not evidence
of an ATA CRC error.

Shell logged its first dynamic object at uptime 63.067 s. After the fifth
library at 63.844 s, the next Shell log did not arrive until 70.896 s,
immediately after a VFS sync reported 16 blocks flushed at 70.866 s. Further
7–8 s gaps accompany VFS syncs. The user's `disk_benchmark -t 1 -f 131072
-b 8192 -d /home/anon` reported two runs in 8815 ms, with write throughput
102500 bytes/s and read throughput 1311000 bytes/s. Each ext2 block is 4 KiB,
so 16 reported cache blocks are 64 KiB, but direct metadata writes in that
same sync are not counted. Shell is loading libraries, while the background
sync writes desktop and filesystem changes. The current IDE driver sends ATA
FLUSH CACHE after every small block write. `BlockBasedFileSystem` also holds
its cache lock through the entire writeback loop, an independent generic
upstream issue now recorded in `Documentation/UpstreamBugBacklog.md`.

An experiment batched ATA cache flushes at filesystem sync instead of after
each block write. The physical PIO result did not show a startup improvement
and measured the batched flush at 0 ms, so the experiment was reverted and
per-write flush semantics restored. That result does not measure each flush
in the current UDMA5 path. The PowerPC dynamic loader also recognizes
`DT_PPC_GOT` as a handled architecture tag: its PLT code already used the tag,
but the generic scanner
had printed an “unhandled DT_LOPROC” line for every library. Host regressions
for dynamic-tag recognition pass, and the kernel, dynamic loader, and LibELF
build. Physical DMA stability remains unverified.

The prior 929 MiB image was `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img`,
SHA-256 `050dade55256bc5f42c17f8cc13d929910c7313afe7e98c4eab7d5c04da82ed4`.
It contains the rebuilt PowerPC dynamic loader in the ext2 root; its FFS
loader and root bytes match the built inputs. The root has `/bin/membench`
but no benchmark boot service, and e2fsck reports a clean filesystem. No
QEMU run was made, per the user's request.

The user's next physical run of that image did not reach the desktop. A DMA
read of eight sectors at LBA 151776 failed with drive status `0x55`, ATA
error `0x00`, and bus-master status `0x21`. This time the channel software
reset timed out with drive status `0x58`; later PIO reads failed and `/init`
faulted while loading code. The crash follows the failed disk reads. In the
driver, PIO retry follows `TRY(reset_after_dma_error())`, so a reset timeout
returns before PIO is attempted; earlier images reached status `0x50` and
could retry. The new writeback image cannot establish a speed improvement
from this failed boot. Booting this card with explicit `nodma` avoids the
experimental DMA path.

Source review found a likely root cause. The IDE channel sets ATA `nIEN`
(interrupt disable) for polling and left it set for DMA too. The
[PCI IDE bus-master specification](https://read.seas.harvard.edu/~kohler/class/04f-aos/ref/hardware/IDE-BusMaster.pdf)
says the drive signals INTRQ at DMA completion and the controller latches it
in status bit 2. The observed `0x21` has Active set but Interrupt clear. The
driver also previously accepted Active clear without that completion bit.
The revised source masks the IDE PCI function's interrupt to the CPU, checks
that this mask is supported, briefly clears drive `nIEN` for DMA, acknowledges
any old interrupt, and requires the controller's completion latch. A failed
DMA command now gets a second bounded soft reset with a longer pulse before
the drive is declared unusable; this cannot guarantee recovery from a wedged
adapter. If the PCI interrupt cannot be masked, it uses PIO. This is built
and covered by host status tests, but **physical DMA is not yet proven**.

The previous image at
`Build/powerpc/PowerPCBoot/pegasos2-sdcard.img` (929 MiB; SHA-256
`a2ca58ac738634188fa95f8ebe74fe075f4a24d066cea5485492d2bcd925a3bf`).
It contains the revised loader and restores per-write ATA cache flushes.
The kernel logs up to eight slow PIO writes with separate setup, device wait,
data-port transfer, and completion times.
Physical Pegasos boots default to PIO for safety; `dma` or `udma=N` opts into
experimental DMA on this same image. QEMU retains its existing default.
Loader argument, DMA status, and recovery self-tests pass; the FFS loader and
ext2 root match the built inputs. The temporary candidate image was removed
so there is only one card image to write. On physical hardware, that image
ran with `fullram weakrng dma`, but the VT8231 did not retain the PCI
Command interrupt-disable bit. The log says `PCI interrupt output masked for
polled DMA: false` and `PCI interrupt cannot be masked, using PIO`. This run
did **not** test a DMA transfer.

The same run measured five 8-sector PIO writes with 35–36 us setup,
375–434 us waiting for data, 904–999 us sending data, and 113–170 ms
waiting for command completion. Two later 1-sector writes also spent
159–166 ms waiting for completion, with 120 us sending data. The long
completion wait is in polling the drive's busy status after writing the data,
not in Serenity's PIO data-copy loop. This does not yet prove why each VFS
sync takes about 8 s: those syncs report 16–23 flushed blocks, and other
metadata writes or serialization may add time. The full log reached `/init`
and later VFS syncs without reporting a DMA failure; desktop appearance was
not explicitly confirmed for this run.

Source comparison: [Haiku's VT8231 driver](https://github.com/warpdesign/Haiku-PowerPC/blob/pegasos2-support/src/add-ons/kernel/busses/ata/via_ide_pci/via_ide_pci.cpp)
uses its shared ATA adapter, which [installs an interrupt handler](https://github.com/warpdesign/Haiku-PowerPC/blob/pegasos2-support/src/add-ons/kernel/generic/ata_adapter/ata_adapter.cpp)
for DMA and enables drive interrupts when issuing DMA requests. It does not
need the PCI Command interrupt-disable bit to stick. [Linux's CHRP PCI
fixup](https://github.com/torvalds/linux/blob/master/arch/powerpc/platforms/chrp/pci.c)
also detects a Pegasos 2 firmware case where the IDE class byte reports PCI
native mode while the controller is actually in legacy mode; when its PCI
interrupt line is 14, Linux forces the programming interface to legacy mode.
The first IRQ-enabled Serenity image left this IDE function in native mode
and used the firmware BARs. Its physical run later confirmed PCI line 14,
meeting Linux's condition for the legacy-mode fixup. Do not assume the old
`0x21` DMA status identifies an adapter defect.

The first IRQ-enabled source revision followed Linux's use of the IDE completion interrupt.
It registers an IRQ handler when the controller cannot mask its PCI output,
records the bus-master completion latch before acknowledging ATA status, and
lets the transfer loop observe that recorded completion. For the onboard
VT8231, Pegasos-specific routing uses primary IRQ 14 and secondary IRQ 15;
the physical log also printed the firmware's PCI Interrupt Line value.
That image did not yet apply Linux's mode fixup. The IRQ route and completion
tests passed, and the PowerPC kernel built. Physical DMA remained unverified.
The first IRQ-enabled image
at `Build/powerpc/PowerPCBoot/pegasos2-sdcard.img` (SHA-256
`a4b913cee8c5702ae07e00b150a2af54dab0c70d393e27bb0f5522c567b3ef7c`)
contains this revision; its FFS boot partition was updated in place, leaving
the ext2 root partition untouched. No QEMU run was made.

The local MorphOS 1.4 Pegasos 2 `boot.img` has two layers. Its outer gzip
member contains a PowerPC ELF with a symbolized `Driver_VIA8231` routine;
that routine configures southbridge routing and assigns IRQ 9 to USB and
audio. The ELF also has an embedded `COM1` payload between `module_start`
(`0x4b6000`) and `module_end` (`0x5c4000`). Decompressing that payload
reveals `idevia.device 50.17 (5.5.03)` and `ide.device`, plus messages for
drive IDENTIFY data, selecting PIO/MWDMA/UDMA modes, allocating physical PRD
tables, bus-master I/O, and handling ATA interrupts. The IDE driver was
indeed in `boot.img`, rather than a separate file in the ISO's `morphos/Devs`
directory. These embedded messages establish that MorphOS supports and
configures UDMA, but do not show which mode the user's adapter actually
negotiated at runtime. The
[VT8231 datasheet](https://www.bitsavers.org/components/viaTechnologies/VT8231_South_Bridge_rev_2.32_20040510.pdf)
and [Linux's Pegasos 2 fixup](https://github.com/torvalds/linux/blob/master/arch/powerpc/platforms/chrp/pci.c)
are the references for IDE interrupt routing.

The first physical boot of the IRQ-enabled image (`a4b913...`) used
`weakrng fullram dma`. Firmware PCI Interrupt Line was 14; the driver
registered IRQ 14 for primary and IRQ 15 for secondary, then selected UDMA2.
The first logged DMA read, eight sectors at LBA 151920, failed with drive
status `0x2f`, ATA error register `0x00`, and bus-master status `0x21`.
There was no bus-master completion interrupt. The first channel reset returned
ready status `0x50`, PIO fallback continued, and `/init`, WindowServer, and
later VFS syncs ran without a kernel panic in the supplied excerpt. This
does not establish a successful DMA transfer or a complete desktop boot.

The driver used to treat ATA error/fault bits sampled during an active DMA
command as final, even before the bus-master interrupt. This can explain the
new fast failure on status `0x2f` with bus-master `0x21`; whether that ATA
status was transient is not yet proven. Linux's
[BMDMA interrupt path](https://github.com/torvalds/linux/blob/master/drivers/ata/libata-sff.c)
checks the bus-master interrupt before evaluating ATA status and pauses after
issuing the ATA command. The current source revision follows that order: only
bus-master error is immediately fatal; ATA status is final after the
completion interrupt. It also gives the command a 400 ns settling interval
and waits briefly after starting the bus master before polling its status.
The physical `0x2f`/`0x21` case now has a regression test, but hardware DMA
success is still unverified.

The next image (`7cf6ebff...`) tried Linux's Pegasos 2 compatibility-mode
fixup on an explicit `dma` boot because PCI line 14 matched its condition.
It switched both channels to the standard legacy ports. On this physical
board neither channel found a drive, and storage discovery reported zero
devices before panicking. No DMA transfer was attempted in that run. This
proves that PCI line 14 alone does not establish usable legacy I/O decoding
on this firmware; changing the programming-interface bits is insufficient.
Do not reapply the legacy-mode fixup without evidence that the fixed ports
actually decode on this machine.

The previous source revision restored the working native BAR path for every
boot, including `dma`. It retains the independent correction that waits for
the bus-master completion interrupt before treating ATA status as final.
The physical legacy-mode failure has a regression case in the native IDE
boot checker. The IRQ route and completion tests, native IDE checker
self-test, and PowerPC kernel and loader build passed. That card image at
`Build/powerpc/PowerPCBoot/pegasos2-sdcard.img` (SHA-256
`4d114132491e6efb24bf52fd60f2c2bc75a3ac0594e4d3345d1c25a4fc992727`)
has the new loader in its FFS partition; the ext2 root is byte-for-byte
unchanged. FFS loader bytes match the built loader. No QEMU run was made.

The physical run of that native-mode image (`4d114132...`) found the drive,
negotiated UDMA2 and mounted ext2. `/init` and the initial SystemServer
services started. A later eight-sector DMA read at LBA 277408 timed out with
drive status `0x58`, ATA error `0x00`, and bus-master status `0x21`: the drive
requested data, the bus master was active, and no completion interrupt was
visible in that later status snapshot. The handler may already have cleared
the interrupt bit, so this snapshot alone cannot prove that no IRQ arrived.
Reset returned `0x50` on its first attempt, then PIO fallback
continued into the WindowServer main loop. Thus ignoring the early `0x2f`
sample avoids the premature abort but does not solve the stalled transfer.
Because root and user programs loaded before this first reported failure,
earlier DMA reads may have succeeded; old logs did not record successful
transfers, so that remains an inference.

The previous source revision checked that PCI Command retained both I/O and bus
master enable bits, confirmed VIA UDMA timing register readback, and logged the
bus-master command and PRD pointer if a transfer stalled. It also used full
memory fences between DMA setup, ATA command issue, and bus-master start,
matching the ordering around PowerPC PCI I/O in Haiku's
[PCI I/O implementation](https://github.com/warpdesign/Haiku-PowerPC/blob/pegasos2-support/src/add-ons/kernel/bus_managers/pci/pci_io.cpp).
The driver logged the first successful DMA transfer and reported the number
of successful transfers and handled IRQs at failure. Haiku's
[PowerPC PCI DMA address conversion](https://github.com/warpdesign/Haiku-PowerPC/blob/pegasos2-support/src/add-ons/kernel/bus_managers/pci/pci.cpp)
uses physical RAM addresses directly, as Serenity does. This makes a missing
PCI DMA address offset less likely, but does not prove the controller can
reach every RAM page used here. Hardware DMA stability is still unverified.
That card image was at
`Build/powerpc/PowerPCBoot/pegasos2-sdcard.img` (SHA-256
`499b0a2ae2850b86315631f23b2c8a82fe76ba232b146117fc042e95363e65e7`).
The FFS loader matched the built loader and ext2 root was unchanged; no QEMU
run was made.

The physical run of that image (`499b0a2a...`) confirmed PCI Command `0x0007`
(I/O and bus master enabled), and the VIA UDMA2 timing byte read back as
`0xf4`. The first DMA read, one sector at LBA 0, timed out with drive status
`0x58` and a final bus-master status snapshot of `0x21`. The bus-master
command was `0x09` (started, device to memory), and the PRD pointer read back
as the programmed physical address. The log counted zero successful DMA
transfers and two handled IRQs, then reset succeeded and PIO booted ext2.
Those two IRQs were counted since channel creation, **not** since this DMA
command. Also, the IRQ handler clears bus-master interrupt bit 2 when it
runs. It was therefore incorrect to interpret final status `0x21` as proof
that no DMA interrupt occurred during this read.

Haiku's [IDE DMA path](https://github.com/warpdesign/Haiku-PowerPC/blob/pegasos2-support/src/add-ons/kernel/bus_managers/ata/ATADevice.cpp)
waits for an IRQ and stops the bus master before checking final ATA status;
Linux's [BMDMA interrupt handler](https://github.com/torvalds/linux/blob/master/drivers/ata/libata-sff.c)
also stops it upon the DMA IRQ. Serenity had instead waited for DRQ to clear
before stopping the engine. The current source revision records IRQ count and
controller/drive status for each transfer, stops the engine on a recorded
or still-latched IRQ, and only then decides whether the transfer completed.
It acknowledges old controller IRQs without attributing them to a transfer
that has not started yet.
It rejects an interrupt received while the PRD engine is still active, to
avoid treating incomplete DMA data as a successful disk read. DMA stability
remains unverified on physical hardware.

The previous card image at
`Build/powerpc/PowerPCBoot/pegasos2-sdcard.img` (SHA-256
`8f6b8964e283b7349d168bab57bc9261abfca8d7b4951f09a461002dcf578a8e`)
contained that change in the FFS loader. Its ext2 root partition was unchanged.

The physical run of that image reached 14 successful DMA transfers before
reporting an error on an eight-sector read at LBA 151920. This was a false
failure: the IRQ handler captured bus-master status `0x24` (interrupt set,
PRD engine inactive) and ATA status `0x50` (ready), with one IRQ assigned
to that transfer. The polling thread had read `0x21` before the IRQ, then
observed the handler's completion flag and used its stale `0x21` value to
reject the transfer. Reset succeeded and PIO fallback continued.

The current source uses the handler's recorded bus-master status when it
observes that handler's IRQ. It still treats an actual active PRD engine or
ATA error as a failure. The physical `0x21`/IRQ `0x24` race reproduced in a
host regression test before the change and passes afterward. The sole current
card image at the same path has SHA-256
`3667a4e95d5b7227ce2582eba7836157d1f4c00dd2984df5c23b1d9b9106d65f`.
Its FFS loader and unchanged ext2 root match the built inputs; host tests
and the PowerPC kernel and loader build pass. Physical validation remains
for a later time, at the user's preference. No QEMU run was made.

The physical run of this image confirmed that the earlier false-completion
race is fixed: the first transfer completed with ATA status `0x50`, bus-master
status `0x24`, and a recorded IRQ. It completed 134 DMA transfers before a
different eight-sector read at LBA 635776 stalled. After roughly 6.9 seconds,
the driver reported ATA status `0x58` (DRQ still set), bus-master status `0x21`
(engine active, no interrupt or error), bus-master command `0x09`, and zero
IRQs assigned to this transfer. The log also showed 138 acknowledged bus-master IRQs
across the channel versus 134 successful DMA transfers. The IRQ counter
increments even outside an active DMA transfer, so the aggregate mismatch
does not identify which transfers received extra IRQs. The PRD pointer read
back as programmed and the channel reset to ATA status `0x50`, after which
PIO continued. This is an unfinished DMA transfer, not the stale `0x21`
versus IRQ `0x24` race. The log does not show whether frequent PCI status
reads contributed to the stall; an IRQ wait would time out if the engine
remained active without asserting an interrupt.

The cause of this remaining stall is unknown. Linux's
[VT8231 driver](https://github.com/torvalds/linux/blob/master/drivers/ata/pata_via.c)
identifies the chip as UDMA100 without the older VIA FIFO/PREQ erratum, and
its UDMA2 timing calculation agrees with the programmed `0xf4`. The existing
card can isolate timing without another write: its `udma=0` boot option
selects the slowest supported UDMA mode. Do not infer that the SD adapter is
faulty from this log; the same hardware reportedly runs UDMA under MorphOS.

The next source revision replaces the ten-microsecond bus-master status poll
with an IRQ-backed wait on channels with a registered handler. The IRQ handler
records status, acknowledges the latch, and wakes the waiting I/O thread.
The wait has a five-second deadline and samples status once more at timeout
to catch a completion racing the timer. Channels with masked PCI interrupts
still use the original poll. This also avoids a status read immediately after
setting bus-master START, which [Linux's BMDMA implementation](https://github.com/torvalds/linux/blob/master/drivers/ata/libata-sff.c)
warns may disturb some controllers. On failure, logs now show whether the
IRQ wait expired, the ATA taskfile registers, and how many bus-master IRQs
arrived outside an active transfer. A physical boot check requires an
IRQ-waited success and no later PIO fallback for a stable-DMA result. The
PowerPC kernel and host regression checks build; physical behavior remains
unverified. The sole current image is
`Build/powerpc/PowerPCBoot/pegasos2-sdcard.img` with SHA-256
`11b4366dd5dcc70fb5de07a315a6e3a730d88124f4a87e9f9dad796eec31571a`.
Its extracted FFS loader matches the built ELF byte for byte, and its ext2
root partition still matches `pegasos2-root.ext2` byte for byte. No QEMU run
was made. PIO is the physical Pegasos 2 default in this image: the loader
internally adds `nodma` unless `dma` or `udma=N` is explicit. Boot with
`fullram weakrng dma` for the default UDMA2 path; the same image accepts
`fullram weakrng udma=0` to isolate UDMA timing. The source now removes that
implicit `nodma` argument and its user-facing help entry. Native Pegasos IDE
instead requires explicit `dma` or `udma=N`; QEMU keeps its existing DMA
default. This source cleanup is **not** in the already built card image and
does not require another SD write for the current IRQ-wait test.

A physical boot of the existing card with `fullram weakrng udma=0` also
stalled. It completed 619 DMA transfers, then an eight-sector read at LBA
125928 timed out with ATA status `0x58`, bus-master status `0x21`, command
`0x09`, and no IRQ for that transfer. The log counted 629 bus-master IRQs
since channel creation. Reset recovered the drive and switched to PIO.
Thus UDMA2 timing alone cannot explain the stall; UDMA0 changes neither the
failure shape nor the need to recover. The excerpt does not establish whether
the IRQ-wait image above was on the physical card for this boot.

The first real Pegasos 2 boot of the IRQ-wait image used explicit
`dma fullram weakrng`. It reported the first DMA success at 9.365 seconds and
`First IRQ-waited DMA transfer succeeded` at 9.382 seconds. Ext2 mounted at
10.032 seconds, WindowServer entered its main loop at 30.485 seconds, and
serial output continued through 86.909 seconds without any DMA failure or
PIO fallback line. This validates the IRQ-backed completion path through
desktop startup on this machine. Longer-running DMA stability and write
throughput remain to be measured; a slow UDMA0 boot on the previous image
does not negate this new-image result.

The user reports that desktop startup still feels slow and the SD adapter's
LED can stay lit for several seconds. The DMA boot log shows Ext2 syncs of
16-23 blocks continuing at 30.206, 41.351, 51.555, 59.873, 70.799, 78.798,
and 86.909 seconds. Thus successful DMA completion has not resolved the
filesystem write latency. `IDEChannel::write_sectors()` still issues an ATA
cache flush after each request. Earlier PIO logs measured a batched flush at
near zero; the per-request UDMA5 flush cost, drive write completion, and Ext2
sync locking remain candidates. Compare the same `disk_benchmark` command
against the
earlier PIO result before attributing the LED stalls to any one component.

On a later physical boot with `udma=5`, the same
`disk_benchmark -t 1 -f 131072 -b 8192 -d /home/anon` reported
`write_bps=453666 read_bps=2060000`. The earlier PIO run reported
`write_bps=102500 read_bps=1311000`, so the measured write rate rose about
4.4 times and read rate about 1.6 times. The tool uses `O_DIRECT` by default
and issues sequential 8 KiB calls; ext2 splits each call into 4 KiB
filesystem blocks. Its per-phase rates exclude file close/unlink and do not
directly measure background ext2 sync. This confirms
a useful DMA speedup for that workload, while leaving the seconds-long
desktop and LED pauses as a separate unresolved problem. The UDMA5 result
also does not prove long-term DMA stability.

On that UDMA5 boot, a second command with a 1 MiB file and 64 KiB userland
buffer (`disk_benchmark -t 1 -f 1048576 -b 65536 -d /home/anon`) took
16,342 ms and reported `write_bps=66000 read_bps=2240000`. The larger file's
write rate fell sharply while its read rate barely changed. Source review
shows `Ext2FSInode::write_bytes_locked()` splits `O_DIRECT` file writes into
4 KiB blocks; `BlockBasedFileSystem::write_block()` sends each block to the
IDE driver, and the driver issues ATA FLUSH CACHE after each write request.
Thus this benchmark made about 256 separate 4 KiB device writes, not sixteen
64 KiB DMA transfers. It cannot isolate the two-entry 64 KiB PRD path or
prove that FLUSH CACHE alone caused the slowdown. The extra write latency may
depend on sustained media writes, metadata allocation, flushes, or locking.

The user also ran the 1 MiB case with `-b 8192` on the same UDMA5 boot:
`write_bps=65000 read_bps=2012000`, with no DMA failure or PIO fallback in
the serial log. Holding file size fixed while changing the userland buffer
from 64 KiB to 8 KiB therefore changed little. This agrees with ext2's
4 KiB device requests and points away from a two-entry 64 KiB PRD defect.
The reported total time of 1,649 ms conflicts with the tool's 65,000 B/s
write rate for a 1 MiB file (which implies roughly 16 seconds), so only the
rates are used for comparison.

A read-only `membench --no-ram --disk /dev/hda --disk-mib 8
--disk-block-kib 64` test returned permission denied for the desktop user;
no privilege escalation or raw disk write was attempted. Source inspection
then found that `StorageDevice::read()` and `write()` cap every request at one
page because older PATA controllers use a one-page DMA buffer. The Pegasos
IDE device already has a contiguous 64 KiB buffer, so the source now lets
that device advertise a 64 KiB limit while all other storage devices retain
the one-page default. A host regression checks that a 1 MiB request is
planned as sixteen IDE transfers instead of 256 page-sized transfers; the
PowerPC kernel builds. This is not in the currently running SD image and
does not change ext2's separate 4 KiB direct-I/O splitting. Physical
throughput for the larger raw requests remains unmeasured.

The explicit `nodma` run of the `050dade...` image reached the desktop, but
did not clearly improve startup time. VFS sync still took around 8 s for
16–23 blocks, while `IDE: cache flush after 18/25/22 writes took 0 ms`
showed the batched ATA FLUSH CACHE command was cheap in that PIO run. The user
saw the Shell window fully drawn around uptime 81.090 s. This does not
measure per-write flush cost in UDMA5. Slow data writes, the
PIO data transfer, device write latency, and the filesystem cache lock remain
candidates; the upstream lock issue is documented separately. The batching
change has been reverted in source. Per-write cache flushes are restored.
The user recalls that MorphOS uses UDMA on this same adapter, strengthening
the case for a Serenity DMA protocol issue rather than an inherent adapter
limit. MorphOS's different boot workload does not measure Serenity's PIO
write path.

The periodic timing fallback is opt-in. PowerPC external interrupt timing is
also mixed into the RNG by default, matching the existing x86 interrupt
source. The RNG wait now uses the predicate-based wait queue, which rechecks
readiness while holding the RNG lock and cannot miss a wake between checking
and sleeping. [NXP's Pegasos II firmware guide](https://www.nxp.com/docs/en/application-note/AN2738.pdf)
documents passing a kernel argument after the boot filename.

The first physical serial capture confirmed SmartFirmware 1.2, real-mode
client calls, PVR `0x00083311`, 512 MiB RAM and the PCI/IDE device tree. It
stopped while printing the Radeon `rom` property because the printer used a
reported property length beyond its 128-byte buffer. A host regression failed
before the bound and passed after it; the quick and normal QEMU loader checks
pass. `serenity.elf` in the local FFS partition has been replaced with the
fixed loader and matches the rebuilt ELF byte for byte. The second physical
capture showed the bounded ROM property, `firmware tree end`, the stop
message, and `Program terminated!` before SmartFirmware restarted. The
read-only firmware survey is complete. The hardware plan and full observations are in
[PowerPCSmartFirmwareBootPlan.md](PowerPCSmartFirmwareBootPlan.md).

## PixelPaint bring-up

The initial PixelPaint build failed because the GML string generator did not
create the directory for `Filters/MedianSettingsGML.h`. After that was fixed,
linking exposed the missing double-precision `sincos` function in LibC. The
PowerPC math fixture now checks that function, and the PixelPaint QEMU test
copies the default color palette into its guest image.

The PixelPaint test boots a fresh desktop, waits for the app to render, draws
a black line on the default canvas, saves `Untitled.pp`, and checks the saved
project's dimensions and layer count. Its quick 750 and normal 750cxe targets
both pass. PixelPaint's other tools, filters, and export formats have not
been exercised by this smoke test.

For a manual PixelPaint run on this development host, a snapshot made from
the passing 750cxe test is at
`Build/powerpc/PowerPCBoot/pixelpaint-ready-20261012/root.ext2`. It is a local
build artifact and PixelPaint launches automatically in its desktop session.
Copy it before booting so QEMU writes only to the copy:

```sh
disk=$(mktemp -d)/root.ext2
cp -c Build/powerpc/PowerPCBoot/pixelpaint-ready-20261012/root.ext2 "$disk"
qemu-system-ppc -M pegasos2 -cpu 750cxe -m 512 \
    -vga none -device virtio-gpu-pci -device virtio-keyboard-pci \
    -device virtio-mouse-pci \
    -kernel Build/powerpc/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf \
    -append root=lun3:0:0 \
    -drive "if=none,format=raw,id=virtio_disk,file=$disk" \
    -device virtio-blk-pci,drive=virtio_disk \
    -object rng-random,id=rng0,filename=/dev/urandom \
    -device virtio-rng-pci,rng=rng0 \
    -display cocoa,zoom-to-fit=on -serial stdio -monitor none -no-reboot
```

The `browser-ready-20261012/root.ext2` snapshot under the same build tree
predates the PixelPaint test image and should not be used to verify the
current PixelPaint build.

## Media application image

`Build/powerpc/PowerPCBoot/media-ready-20261012/root.ext2` is a separate
local guest image with PixelPaint, ImageViewer, VideoPlayer, SoundPlayer,
their services and app entries, and three test files on the Desktop:

| File | Format | Size |
| --- | --- | ---: |
| `test-picture.png` | 96×64 PNG test pattern | 549 bytes |
| `test-video.webm` | 96×64 VP8, 5 frames/s, 2 seconds | 6 KiB |
| `test-sound.wav` | 22,050 Hz mono PCM16, 1 second | 44 KiB |

The image and samples live under `Build/` and are not tracked by Git. Use a
copy of the image for QEMU, as in the command above, substituting this image
path. The older PixelPaint QEMU session uses a different disk and cannot see
these new files until a new guest is booted.

Headless QEMU checks from this image showed ImageViewer rendering the PNG,
VideoPlayer displaying a decoded WebM frame, and SoundPlayer opening the
minimal WAV without an error dialog. Screenshots and serial logs are under
`Build/powerpc/PowerPCBoot/media-ready-20261012/{ImageViewer,VideoPlayer,SoundPlayer}/`.

An FFmpeg-generated WAV with a `LIST/INFO` metadata chunk was rejected with
an offset error; the minimal PCM WAV above loads.

## VIA AC'97 audio driver

The Pegasos 2 PCI audio function is VIA `1106:3058`. The existing generic
AC'97 driver expects Intel-style native audio bus registers, so its probe
rejects this device. QEMU's [VIA AC'97 implementation](https://gitlab.com/qemu-project/qemu/-/raw/v11.0.2/hw/audio/via-ac97.c)
models a separate scatter/gather playback engine in BAR0 and an AC'97 codec
command register at BAR0 offset `0x80`. The [Linux VIA82xx driver](https://github.com/torvalds/linux/blob/master/sound/pci/via82xx.c)
also identifies `1106:3058` and documents the VIA register layout.

The driver is under `Kernel/Arch/powerpc/Pegasos2/` and is registered through
an architecture-neutral audio-driver registration API before
`AudioManagement` enumerates PCI devices. It programs the AC'97 codec at
44.1 kHz, writes little-endian VIA DMA descriptors, and polls for playback
completion. The initial version uses one DMA buffer and serialized writes.

The `PowerPCVIAAudioCheckQuick` (750) and `PowerPCVIAAudioCheck` (750cxe)
targets boot SoundPlayer with a one-second tone and capture QEMU's audio
output. Before the driver, QEMU captured zero frames. Both targets now pass
with 76,800 nonzero samples and peak amplitude 8,495. A separate check using
the `media-ready-20261012` desktop image also captured nonzero SoundPlayer
output. QEMU's WAV audio backend leaves its RIFF length fields at zero, so
the regression checks the captured PCM payload directly.

For audible host output on macOS, boot the media image with
`-M pegasos2,audiodev=audio0 -audiodev coreaudio,id=audio0`; the QEMU tests
instead use a WAV capture backend. Physical Pegasos 2 audio hardware has not
been tested. Any guest booted before this change needs a restart with the
rebuilt loader to use the driver.

An optional Cocoa/CoreAudio boot with the media desktop image initialized the
VIA driver and AudioServer, but FileManager's desktop process faulted at
`SRR0=0x080c79fc` while writing `0x01330004`, before SoundPlayer was started.
The cause is unclassified; this run does not prove host speaker playback.
Its serial log is `Build/powerpc/PowerPCBoot/via-audio-coreaudio-gui.log`.
The separate headless QEMU WAV-capture tests verify nonzero device output.

A focused sound demo image at
`Build/powerpc/PowerPCBoot/via-audio-coreaudio-demo/root.ext2` starts
SoundPlayer with the one-second tone and omits FileManager's desktop process.
It booted under QEMU's CoreAudio backend with the VIA driver and SoundPlayer
running. A disposable copy was left running for the user to listen; the agent
cannot verify sound at the host speakers directly. A later Calendar process
logged an `unveil` error because that image lacked `/bin/CalendarSettings`.
The serial log is at
`Build/powerpc/PowerPCBoot/via-audio-coreaudio-demo/serial-live.log`.

## Calendar and TextEditor image

The `PowerPCDesktopProgramsCheckQuick` regression now launches Calendar,
checks its window, opens `/home/anon/editor.txt` in TextEditor, inserts text,
approves its write permission prompt, and verifies the saved file. The quick
750 CPU and normal 750CXe runs both passed. A reusable guest image is at
`Build/powerpc/PowerPCBoot/calendar-text-editor-ready-20261012/root.ext2`.
It booted to the desktop under QEMU and contains Calendar, CalendarSettings,
TextEditor, and their portal services. Copy the disk before booting, using
the PixelPaint QEMU command above with this image path. Launch the apps from
the Serenity menu or Terminal. The older sound demo image predates these
changes and still reproduces Calendar's `unveil` failure.

A Cocoa QEMU session was started from a disposable copy at
`/tmp/serenity-calendar-editor-ui.cV63gY/root.ext2` (PID 36664 at the time
of this update). Its serial log is
`/tmp/serenity-calendar-editor-ui.cV63gY/serial.log`; WindowServer entered
its main loop without a crash. This session has no host audio backend. If it
has exited, make another copy of the reusable disk and launch it with the
PixelPaint command above, substituting the Calendar/TextEditor image path.
