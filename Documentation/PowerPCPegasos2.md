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

The native console reports `serial online`, `setjmp roundtrip passed`,
`RAM 256 MiB`, `program exception roundtrip passed`,
`MMU high alias passed`, `HPT high alias passed`,
`HPT duplicate mapping rejected`, `HPT remap and secondary hash passed`,
`HPT group reuse passed`, `HPT unmapped fault passed`,
`HPT read-only fault passed`, `HPT VSID switch passed`,
`HPT kernel protection passed`, `user-mode page protection passed`,
`user-mode stack passed`, `user-mode syscall resume passed`, and
`user-mode exception frame passed` on the terminal.
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
These layouts compile. The probe also enables the PowerPC MMU briefly with
16 MiB BAT mappings: low identity RAM, a high `0xC0000000` alias of the same
RAM, and the Pegasos 2 PCI I/O window. It executes through the high alias,
returns from a trap taken there, checks big-endian bytes through the high data
alias, and then disables translation before printing its result. This is a
bootstrap mapping test. A second probe invalidates the high BAT and installs
a 64 KiB aligned hashed page table through `SDR1`, using VSID 12 for the
high segment. It maps a high code page and a separate high data page, then
checks a trap return, data writes, the segment register, `SDR1`, and the
invalidated high BATs. The data page maps to a different physical page than
the old BAT would have selected, so it checks actual HPT translation. This
is followed by nine virtual pages that share a primary hash group, forcing
the ninth into the secondary group. The probe reads all nine aliases, remaps
an already used virtual page to another physical page, invalidates its TLB
entry, and checks reads and writes through the new mapping. It then fills both
hash groups, confirms another insertion fails, removes a live mapping, and
reuses the slot for a different virtual and physical page. A load from the
removed alias then enters a probe-only data-storage exception vector at
`0x300`; the check verifies the fault address in `DAR` and the missing-page
status in `DSISR`. The vector skips that one load to resume the probe; it is
not a general kernel page-fault handler. The map, lookup, remap, and unmap
operations now live in `Kernel/Arch/powerpc/HashPageTable.c`, take an explicit
VSID, and reject a second mapping for the same virtual page. The boot image
also maps a read-only page and confirms that a store faults without changing
either the virtual or physical value. It then maps the same low virtual page
under two VSIDs, changes SR1 while translation is enabled, and checks that
each VSID reaches a distinct physical page while the kernel's SR12 mapping
remains accessible. The high kernel PTEs use supervisor-only protection with
SR12's user key set. The probe temporarily selects the protected key for
supervisor access and checks that the high page faults, then restores access.
A small problem-state probe fetches from a user HPT page, uses a mapped user
stack, writes and reads a separate user data page, then attempts to read the
protected kernel page. It checks the data-storage protection fault and returns
through the low `0xc00` system-call vector. The first `sc` resumes in problem
state and the probe checks its general registers, condition register, and
stack pointer. A second `sc` returns to supervisor real mode and restores the
supervisor stack. The low vector branches to an out-of-line handler that saves
all 32 general registers, CR, CTR, XER, LR, SRR0, and SRR1 in an integer
exception frame. This remains a single static probe frame; nested exceptions,
floating-point state, and per-thread context handling remain. The main kernel
build does not yet use this code. General allocation, an eviction policy when
both hash groups fill, per-process VSID allocation, and a complete kernel
address space remain.
Physical Pegasos 2 hardware must also validate cache and TLB ordering.
The [MPC750 BAT format](https://www.nxp.com/docs/en/application-note/AN1809.pdf)
defines the block-length, cache, and protection fields used here.
The [NXP page-table setup note](https://www.nxp.com/docs/en/application-note/AN2794.pdf)
describes the HPT entry and hash layout.
The [MPC750 manual](https://www.nxp.com/docs/en/reference-manual/MPC750UM.pdf)
defines the data-storage vector and missing-PTE status used by the fault check.
The [PowerPC Programming Environments manual](https://www.nxp.com/docs/en/user-guide/MPCFPE.pdf)
defines the PTE protection bits and the system-call exception state.

After its last Open Firmware call, the probe copies a program-exception
handler to physical address `0x700`, synchronizes the data and instruction
caches, selects low exception vectors, and executes a trap instruction. The
handler records the trap status, advances `SRR0` past that instruction, and
returns with `rfi`. The QEMU regression checks the exception count and status
before reporting success. This covers one exception path; the kernel still
needs the complete vector table and register-state handling. The
[MPC750 manual](https://www.nxp.com/docs/en/reference-manual/MPC750UM.pdf)
documents the `0x700` vector and program-exception state.

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

## Big-endian audit

Big-endian support is separate from the pointer-width port. The QEMU alias
test confirms native PowerPC byte order for one RAM value; it does not validate
Serenity's file formats, device protocols, or applications. Current audit
targets include:

| Area | Finding to investigate |
| --- | --- |
| AK | `StringBase.h` and `FloatingPointStringConversions.cpp` contain explicit little-endian assertions. Their representations and fast paths need portable implementations and PowerPC tests. |
| ELF and loader | `LibELF/Validation.cpp` currently expects ELF64 little-endian, while `ELFBuild.cpp` writes that format. PowerPC needs ELF32 big-endian validation, loading, relocation, and output. |
| Kernel | `Tasks/Coredump.cpp` writes ELF64 little-endian. Memory-mapped PCI, storage, and device registers need protocol-specific byte-order checks. |
| Libraries and apps | Direct native-width reads in code such as LibPDF, LibSQL, and LibCompress need format-by-format review. Existing `LittleEndian<T>` and `BigEndian<T>` fields provide a starting point, but do not cover every parser or writer. |

Each fix needs a format-level regression on both a little-endian build and the
PowerPC target. Swapping at a file or device boundary keeps native in-memory
values in the CPU's byte order.

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
