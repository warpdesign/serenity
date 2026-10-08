# PowerPC Pegasos 2 bring-up

The current PowerPC code is a **firmware boot probe**, not a SerenityOS kernel
port. It boots a freestanding 32-bit big-endian ELF on QEMU's `pegasos2`
machine, enters through Open Firmware, reads the root `model` and memory
properties, and writes diagnostics to the firmware console and QEMU's Pegasos 2
serial port. It does not initialize the kernel, display a desktop, or run user
programs.

## Build and run the probe

Build SerenityOS's PowerPC bootstrap compiler and binutils, then run the probe.
`qemu-system-ppc` is also required. The probe build remains separate from the
normal superbuild until the complete PowerPC kernel and userland targets exist.

```sh
env ARCH=powerpc MAKEJOBS=8 ./Toolchain/BuildGNU.sh --bootstrap-only
make -C Kernel/Arch/powerpc/Boot check
```

`CROSS_COMPILE` defaults to the resulting
`Toolchain/Local/powerpc/bin/powerpc-serenity-` prefix. The bootstrap option
builds GCC and libgcc for freestanding kernel code; the complete PowerPC LibC
and dynamic loader are later userland milestones. The single-ABI libstdc++
build succeeds with the current target headers. PowerPC multilibs are disabled,
so the toolchain builds only the big-endian 32-bit ABI needed here.

Both `check` and `check-quick` run the PowerPC toolchain tests under
`Tests/Toolchain/PowerPC` and the Pegasos 2 QEMU tests under
`Tests/Kernel/PowerPC`. The quick target checks the default CPU with 256 MiB;
the normal target also checks a 750cxe CPU with 512 MiB to verify that the
reported RAM size comes from firmware and early boot works with both CPUs.
The test observes Open Firmware calls through QEMU's VOF trace and captures
the native serial output. It also exercises LibC's PowerPC `setjmp` and
`longjmp`, including nonvolatile GPR and FPR restoration. Firmware `stdout`
does not appear on `-serial stdio` in this setup. The probe switches to the
VIA UART after checking for QEMU's Pegasos 2 firmware signature and confirming
real mode. The probe stays in an
idle loop after its diagnostics.

To inspect the boot manually, start QEMU with the ELF and VOF tracing:

```sh
qemu-system-ppc -M pegasos2 -m 256 \
    -kernel Kernel/Arch/powerpc/Boot/build/serenity-powerpc-boot.elf \
    -display none -serial stdio -monitor none -no-reboot \
    -d trace:vof_getprop,trace:vof_write -D pegasos2-vof.log
```

The native console reports `serial online`, `setjmp roundtrip passed`, and
`RAM 256 MiB` on the terminal.
Stop QEMU after those messages appear; firmware messages are in
`pegasos2-vof.log`.

## Findings and next milestones

QEMU's Pegasos 2 machine can direct-boot an ELF with its built-in Virtual Open
Firmware, without a proprietary SmartFirmware ROM. The firmware client entry
arrives in PowerPC register `r5`; the probe uses its 32-bit client interface.
The firmware's root node reports `model = Pegasos2`, and its `/memory@0`
`reg` property describes RAM. The image is linked at `0x00200000`, like the
local Haiku Open Firmware loader. QEMU's Virtual Open Firmware does not expose
the RAM node through `/chosen`, so the probe finds `/memory@0` directly. The
COM2 address used for early serial output is a QEMU Pegasos 2 fallback; a
physical-machine implementation must discover the UART and PCI I/O window
from firmware.

The target compiler defines `__serenity__` and produces big-endian ELF32
PowerPC objects. `AK::FlatPtr` is four bytes on that target. The PowerPC build
selects a `0xC0000000` kernel virtual base and omits the 64-bit EFI boot data
from its boot information; the existing architectures retain their defaults.
These layouts compile, but the probe still runs in real mode and does not
enable the PowerPC MMU.

The main build still cannot link Serenity's kernel for PowerPC:
`Meta/serenity.sh` and `Kernel/CMakeLists.txt` have no complete PowerPC target,
and the kernel needs PowerPC page tables, CPU primitives, exception entry,
and context switching before generic initialization can run. The current
`0xC0000000` layout is a starting point for 256 and 512 MiB QEMU guests; its
direct-map and high-memory limits need validation with the eventual MMU.
`Kernel/Memory/MemoryManager.cpp` also has EFI-specific paths that must be
selected out for the Open Firmware build. A single-ABI libstdc++ build now
succeeds with the PowerPC `setjmp.h` and `mcontext_t` layouts present.
LibC still needs PowerPC floating-point environment support, signal-context
handling, and the rest of its architecture assembly before user processes can
run.

The shortest route from this probe to a SerenityOS kernel boot is:

1. Finish the 32-bit kernel and userland ABI: architecture CMake sources,
   LibC signal/context and floating-point headers, runtime libraries, and
   shared-code audits for pointer width and endianness.
2. Populate the PowerPC boot information passed to the kernel. Preserve each
   discovered RAM range and obtain reserved ranges from Open Firmware; keep
   firmware calls available until a native console is ready. Load the kernel
   ELF and transfer to its own stack.
3. Implement PowerPC exception vectors, context switching, page tables, cache
   synchronization, and a debug console in `Kernel/Arch/powerpc/`. Reach
   `Kernel::init()` with interrupts disabled and a working memory map.
4. Add Pegasos 2 board support for the MV64361 bridge, legacy interrupt
   controller, 8250-compatible UART, timer, PCI, and IDE storage. Select these
   through architecture build rules and device-tree or firmware identity.
5. Bring up a root filesystem and the PowerPC userland ABI, then test process
   creation, system calls, and the init process under QEMU.

The local Haiku PowerPC checkout has useful board and firmware references in
`src/system/boot/platform/openfirmware/arch/ppc/` and
`src/system/kernel/arch/ppc/`. Its QEMU-specific UART and interrupt code is a
guide to investigate, not a substitute for detecting the actual firmware and
board configuration. [QEMU's Pegasos documentation](https://www.qemu.org/docs/master/system/ppc/amigang.html)
describes the Virtual Open Firmware boot path and emulated devices.
