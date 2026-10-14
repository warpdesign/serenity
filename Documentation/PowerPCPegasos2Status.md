# SerenityOS on PowerPC Pegasos 2: current status

Status checked on 2026-10-15.

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
  Interrupt-driven completion and bus-master DMA are not implemented yet.
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

## SmartFirmware probe card

On 2026-10-14 this host rebuilt the 6.1 MB loader and passed
`PowerPCFirmwareLoaderCheckQuick`. A separate QEMU run with
`ofdump serenity-loader-real-mode-calls
serenity-loader-stop-before-kernel` printed the firmware tree start and end
markers and reached `boot handoff validated`; its trace is
`/tmp/serenity-smartfirmware-vof-20261014.log`.

`PowerPCRDBCardBootCheckQuick` also passed: QEMU direct-loaded the loader,
booted the desktop from the card's ext2 partition on IDE, wrote a 108,894-byte
file, and left a clean filesystem. That test does not exercise SmartFirmware
loading `serenity.elf` from FFS. The one current local card image is
`Build/powerpc/PowerPCBoot/pegasos2-sdcard.img` (929 MiB).
Its SHA-256 is
`eda98b476c228b9b0e6f2cf0741cca58c6399df4e7f5fd4cd171ed128617def0`.
The independent `rdbtool` sees `SERBOOT` as a 32 MiB bootable FFS partition
and `SERROOT` as an 896 MiB ext2 partition. Extracting `serenity.elf` from
FFS matched the rebuilt loader byte for byte.

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
