# SerenityOS on PowerPC Pegasos 2: current status

The goal is to port SerenityOS to the Pegasos 2, a 32-bit PowerPC G3/G4
machine. The port currently boots and runs a graphical SerenityOS desktop on
QEMU's `pegasos2` machine. The work is on the
`powerpc32-pegasos2-bringup` branch. The earlier bring-up record in
[PowerPCPegasos2.md](PowerPCPegasos2.md) documents the initial firmware
probe and is historical.

## Working in QEMU

- The Open Firmware loader starts the PowerPC kernel with hashed page-table
  mappings, exceptions, interrupts, timers, PCI, virtio storage, graphics,
  input, networking, and entropy.
- The 32-bit big-endian userland boots WindowServer, Taskbar, Terminal, and
  other desktop utilities. Browser renders a local HTML page.
- DHCP, DNS resolution, and the Pegasos 2 RTC pass the Browser smoke check.
- PixelPaint opens with its default palette and canvas, draws a brush stroke,
  and saves a valid 510×356 project through FileSystemAccessServer.
- The HPT virtual alias is reserved in the kernel region tree so kmalloc
  expansion cannot overlap it. The diagnosis and red/green evidence are in
  [PowerPCKmallocCrashInvestigation.md](PowerPCKmallocCrashInvestigation.md).

These statements are backed by the PowerPC QEMU test targets below. Real
Pegasos 2 hardware has not been validated by these tests.

## Verification targets

Configure the PowerPC build, then run:

```sh
cmake --build Build/powerpc --target PowerPCKmallocHPTCheck
cmake --build Build/powerpc --target PowerPCDesktopProgramsCheck
cmake --build Build/powerpc --target PowerPCBrowserCheck
cmake --build Build/powerpc --target PowerPCPixelPaintCheck
```

Each target has a `Quick` variant for a shorter run. The Browser target
tests local rendering and network configuration; it does not replay a live
Google search. The user's interactive sequence for the earlier allocator
crash is recorded in the kmalloc investigation document.

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
