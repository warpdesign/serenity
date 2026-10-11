# PowerPC Pegasos 2 bring-up

The current PowerPC code is a **firmware boot probe**, not a SerenityOS kernel
port. It boots a freestanding 32-bit big-endian ELF on QEMU's `pegasos2`
machine, enters through Open Firmware, reads the root `model` and memory
properties, and writes diagnostics to the firmware console and QEMU's Pegasos 2
serial port. It does not initialize the kernel, display a desktop, or run user
programs.

## Build and run the probe

Build SerenityOS's PowerPC bootstrap compiler and binutils, then run the probe.
`qemu-system-ppc` is also required. The probe has a standalone Makefile and an
explicit target in the normal CMake build.

```sh
env ARCH=powerpc MAKEJOBS=8 ./Toolchain/BuildGNU.sh --bootstrap-only
make -C Kernel/Arch/powerpc/Boot check
```

The CMake route uses the same probe and runs both QEMU configurations:

```sh
cmake -S Meta/CMake/Superbuild -B Build/superbuild-powerpc -DSERENITY_ARCH=powerpc
cmake --build Build/superbuild-powerpc --target serenity-configure
cmake --build Build/powerpc --target PowerPCBootProbeCheck
```

`PowerPCBootProbe` builds the ELF without running QEMU;
`PowerPCBootProbeCheckQuick` runs the 256 MiB test. These targets do not build
the full SerenityOS kernel.

`CROSS_COMPILE` defaults to the resulting
`Toolchain/Local/powerpc/bin/powerpc-serenity-` prefix. The bootstrap option
builds GCC and libgcc for freestanding kernel code; the complete PowerPC LibC
and dynamic loader are later userland milestones. The single-ABI libstdc++
build succeeds with the current target headers. PowerPC multilibs are disabled,
so the toolchain builds only the big-endian 32-bit hardware-float ABI needed
here. The boot probe enables the FPU before entering C code and now uses that
ABI to link the full AK float parser with Serenity's PowerPC `libgcc`.

The PowerPC GNU target now defaults to secure PLT, including its `crtbeginS.o`
startup object; the CMake toolchain also selects `-msecure-plt` explicitly.
This produces writable PLT pointer slots and a data GOT instead of the legacy
BSS PLT code stubs. LibELF installs the PowerPC resolver in those GOT slots.
The QEMU probe uses a GCC-linked shared object to test the first lazy call and
the patched second call, with a test resolver in place of the full loader.
LibELF's PowerPC entry transfer also has a QEMU call test. A user process has
not been launched yet.

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
`CPU features passed`, `processor primitives passed`, `memory fences passed`,
`VSID allocator passed`,
`page mapping transaction passed`,
`cache maintenance passed`,
`big-endian short string passed`,
`big-endian float digits passed`,
`big-endian float parser passed`,
`PCI Open Firmware address passed`, `PCI 64-bit address assignment passed`,
`FPSCR environment passed`,
`FPU state roundtrip passed`,
`ELF32 big-endian header passed`,
`ELF32 big-endian image passed`,
`ELF32 relative relocations passed`,
`PPC32 TLS layout passed`,
`PPC32 static TLS lookup passed`,
`ELF32 entry transfer passed`,
`secure PLT lazy binding passed`,
`RAM 256 MiB`, `firmware memory ranges passed`,
`program exception roundtrip passed`,
`decrementer interrupt passed`,
`legacy PIC IRQ0 acknowledge passed`,
`external IRQ0 roundtrip passed`,
`loader breakpoint passed`, `syscall register ABI passed`,
`MMU high alias passed`, `translated external IRQ stack passed`,
`HPT high alias passed`,
`HPT external IRQ stack passed`,
`HPT high IRQ handler passed`,
`user-mode external IRQ resume passed`,
`HPT and timebase boot handoff passed`,
`HPT quickmap alias passed`, `HPT physical metadata array passed`,
`HPT duplicate mapping rejected`, `HPT remap and secondary hash passed`,
`processor TLB maintenance passed`,
`HPT group reuse passed`, `HPT unmapped fault passed`,
`HPT read-only fault passed`, `HPT VSID switch passed`,
`HPT kernel protection passed`, `user-mode page protection passed`,
`user-mode stack passed`, `user-mode syscall resume passed`,
`user-mode exception frame passed`, `register state from user trap passed`,
`thread register setup passed`,
and `stack backchain unwinder passed`
on the terminal.
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
`reg` cells are now decoded into generic boot RAM ranges, with one- and
two-cell forms checked in QEMU. The [Open Firmware standard](https://www.openfirmware.info/data/docs/of1275.pdf)
defines the memory node's `reg` and `available` properties. The HPT handoff
records both SDR1's physical table address and a mapped table address, and
requires an explicit reservation so the memory allocator cannot reuse it.
The kernel loader still has to preserve the range arrays and transfer control
to Serenity's kernel entry with this BootInfo.
The COM2 address used for early serial output is a QEMU Pegasos 2 fallback; a
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
operations now live in `Kernel/Arch/powerpc/HashPageTable.cpp`, take an explicit
VSID, and reject a second mapping for the same virtual page. The boot image
also maps a read-only page and confirms that a store faults without changing
either the virtual or physical value. It then maps the same low virtual page
under two VSIDs, switches all 12 user segment registers while translation is
enabled, and checks that each VSID reaches a distinct physical page while the
kernel's SR12 mapping remains accessible. The segment switch accepts aligned
16-VSID blocks and preserves SR12-SR15. A monotonic allocator now provides
unique blocks for user address spaces and reports exhaustion without reusing a
block while old HPT entries may remain. The QEMU isolation test maps two
allocated VSIDs to different physical pages through a PowerPC page-directory
backend. That backend binds an allocated VSID block to the shared HPT, maps,
remaps, and unmaps user pages, and activates its segment registers. The probe
remaps one user's virtual page to a third physical page and back while the
other user's mapping stays intact. A PowerPC `PageDirectory` now owns that
backend and registers user directories by VSID. Destruction purges the
directory's HPT entries; QEMU checks that purging one address space leaves the
other address space accessible. Its boot-time shared HPT installation now
validates the firmware BootInfo and SDR1. Generic `Region`
mapping now passes a complete request to an architecture-selected mapping
operation instead of mutating a PTE field by field. The existing hierarchical
architectures use their PTEs; the PowerPC backend applies the same request to
the HPT for user pages. QEMU checks the resulting physical page, permissions,
and normal, noncacheable, and guarded I/O attributes. Kernel-only activation
selects reserved user-segment VSIDs and fixed SR12-SR15 kernel VSIDs. The probe
maps three contiguous physical metadata pages into the high kernel range and
checks reads and writes across all three. It also maps the actual CPU 0
quickmap slot at `0xffec0000`, unmaps it, and checks the resulting fault. A
full kernel entry handoff and kernel text protection remain. The high kernel
PTEs use supervisor-only protection with SR12's user key set. The probe
temporarily selects the protected key for supervisor access and checks that
the high page faults, then restores access.
The current SR12 key pair cannot express a supervisor read-only page that is
also inaccessible to user mode with an HPT PP value. Kernel text protection
therefore needs a separate BAT or segment layout before the generic image
protection code can be enabled. The [PowerPC page-protection table](https://www.nxp.com/assets/documents/data/en/supporting-information/TPQ2CH09.pdf)
shows the key and PP combinations.
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
before reporting success. PowerPC's 32-bit integer `RegisterState` now uses
the captured frame layout; QEMU checks its mode, stack, instruction pointer,
syscall arguments, and ptrace conversion against a real user trap. The kernel
still needs a complete exception entry and return path and context switching.
PowerPC `ThreadRegisters` now uses that frame for startup and exec register
setup, including r1 stack state, r3 entry data, r2 thread-pointer reset, and
SRR1 privilege bits. It retains the page directory selected for the thread;
the context switch still needs to activate that directory's user VSIDs.
PowerPC's `FPUState` now saves all 32 scalar registers and FPSCR, and QEMU
checks a state roundtrip with different rounding modes. The kernel still needs
to call those routines during thread switches and handle vector state on CPUs
that provide it. PowerPC CPU identification now reads PVR and distinguishes
the tested 750-family and AltiVec-capable G4 models; QEMU checks the selected
features and the FPU-enable bit in MSR. The [MPC750 manual](https://www.nxp.com/docs/en/reference-manual/MPC750UM.pdf)
documents the `0x700` vector and program-exception state.
The single-core `Processor` path now identifies the boot CPU, tracks critical
depth and scheduler state, controls external interrupts through MSR, and reads
the 64-bit timebase. The QEMU probe checks these operations on the default CPU
and a 750cxe. Full processor initialization, TLB and cache maintenance, trap
entry, and context switching still require kernel integration.
The QEMU probe also reads `timebase-frequency` from the Open Firmware CPU node,
programs a 10 ms decrementer interval, and checks two resulting exceptions at
vector `0x900` against elapsed timebase ticks. The
[MPC750 manual](https://www.nxp.com/docs/en/reference-manual/MPC750UM.pdf)
defines the decrementer exception and its vector. The boot probe temporarily
masks both Pegasos 2 legacy 8259 controllers while CPU external interrupts are
enabled, then restores their masks. Without the masks, QEMU delivered a device
interrupt at `0x500` before the probe had installed that vector. The full kernel
still needs a device interrupt entry path and timer callback dispatch.
The Pegasos 2 controller now uses Serenity's `IRQController` interface with
MMIO mappings for the VT8231 I/O ports and MV64361 interrupt acknowledge
window. The same register logic runs in the real-mode probe: it programs PIT
channel 0, acknowledges IRQ0 through the bridge, checks edge EOI, and checks
slave cascade mask changes. The controller and handler table compile for
PowerPC; the kernel exception vector has not yet dispatched an IRQ to them.
PCI MSI and MSI-X remain unavailable on this PowerPC controller path. PCI
devices therefore select pin interrupts, matching the sixteen registered
legacy IRQ lines; the architecture capability check also lets generic PCI
code compile without a PowerPC conditional.
The probe now copies a `0x500` vector that saves all integer registers and
SRR0/SRR1, calls a C PIC dispatcher, then restores the state with `rfi`. QEMU
observes one external interrupt from PIT channel 0 and checks that the saved
state returns to the interrupted code. A second probe enables BAT translation
and runs with a high virtual stack. The low vector switches to a separate
physical exception stack before its first store, and QEMU checks that `rfi`
restores both the high stack and the translation bits. This still runs
supervisor code through the boot probe's identity instruction BAT. A kernel
entry must establish translated kernel mappings before it calls registered
handlers and handle user-mode IRQs on the same safe stack.
The HPT IRQ probe removes the high BAT, maps a dedicated high stack page in
the hash table, and checks an external IRQ roundtrip on that stack. A further
probe maps the text, exception stack, PIC I/O page, and interrupt-acknowledge
page through the HPT. The physical vector saves the interrupted state, then
uses `rfi` to enter the high mapped handler with translation enabled and EE
disabled. That handler calls C code through its high alias and restores the
original SRR state with a second `rfi`. QEMU checks the high return address
and access to cache-inhibited, guarded PIC mappings. The full kernel still
needs to install this entry path and dispatch to registered handlers, including
interrupts taken from user mode.
The probe also enters a mapped user program with MSR[EE] enabled. PIT IRQ0
arrives with user privilege and translation recorded in SRR1; the high handler
signals a mapped user data page, and `rfi` resumes the user program. Two
system calls then return to the supervisor probe. This verifies the exception
transition in QEMU; the Serenity kernel has not yet installed these vectors.
PowerPC boot information now has a validated timebase-frequency field, and the
kernel timer object can use it to update the monotonic clock and rearm DEC.
The standalone probe checks the handoff value and long-interval nanosecond
conversion. The kernel timer objects compile with the PowerPC Serenity toolchain;
they have not yet run through a full kernel exception dispatcher.
The architecture memory fence interface uses `sync` for full, load, and store
fences, so cached descriptor writes can be ordered before device access. The
probe executes all three fences; DMA visibility still needs validation on real
Pegasos 2 hardware.
The processor cache interface now writes back data blocks and invalidates
instruction blocks over unaligned ranges using the 32-byte block size of the
supported 750 and G4 CPUs. QEMU runs code copied across a block boundary,
changes its return value, flushes the instruction cache, and checks the new
result. Physical hardware still needs to validate cache coherency with DMA.
The processor TLB interface now invalidates a page range or all 64 TLB sets.
The HPT probe changes a live mapping without implicit invalidation and checks
that both targeted and full flushes reveal the new physical page. Disabling
those flushes makes the QEMU regression fail on a stale translation.

AK now unwinds PowerPC32 frames through the r1 backchain and reads the saved
return address from the caller's frame. QEMU checks two GCC-generated nested
calls; the little-endian host frame-record path also passes a native check.
A leaf function may retain its return address only in LR, so walking from that
frame needs an initial LR value that the current frame-pointer API does not
accept.

LibSystem now invokes PowerPC system calls with `sc`. The chosen Serenity
PowerPC syscall ABI puts the function number in r0, up to four arguments in
r3-r6, and the result in r3. A temporary QEMU trap handler checks all five
LibSystem wrappers and their returned values; the probe also checks the
loader's PowerPC breakpoint hook. The kernel still needs a real syscall
dispatcher and full user register state, including FPU state. `DynamicLinker.cpp` passes a
PowerPC cross-compiler syntax check but has not run as a user process.

The main build still cannot link Serenity's kernel for PowerPC. PowerPC CMake
configuration now selects the Open Firmware probe target and leaves out the EFI
prekernel. `Meta/serenity.sh` and `Kernel/CMakeLists.txt` have no complete PowerPC
kernel target, and the kernel needs integrated page tables, CPU primitives, exception entry,
and context switching before generic initialization can run. The current
`0xC0000000` layout is a starting point for 256 and 512 MiB QEMU guests; its
direct-map and high-memory limits need validation with the eventual MMU.
The PowerPC kernel CMake target now applies its 32-bit layout header to every
source before other kernel headers, so generic objects do not accidentally
select 64-bit EFI structures. This is a compile prerequisite, not a linked
kernel.
The shared memory manager now delegates PTE mapping, boot page-table setup,
and quickmap to an architecture-selected hierarchical implementation. The
PowerPC HPT backend accepts complete page-mapping requests, and a PowerPC
`PageDirectory` can install the validated HPT from BootInfo. PowerPC now has
HPT-backed physical-page metadata initialization and a quickmap path, including
data-cache invalidation before a RAM page becomes noncacheable. The PowerPC
time and legacy interrupt interfaces compile, but the kernel entry handoff
and full exception dispatcher are still absent. EFI
memory-map parsing is disabled by the PowerPC boot-layout setting. A single-ABI
libstdc++ build now
succeeds with the PowerPC `setjmp.h` and `mcontext_t` layouts present.
LibC now has PowerPC floating-point environment operations; QEMU checks actual
rounding results, exception flags, and environment save/restore. LibC still
needs signal-context handling and the rest of its architecture assembly before
user processes can run. The cross compiler emits local-exec TLS accesses
relative to r2. The [32-bit Power Architecture ABI supplement](https://www.polyomino.org.uk/publications/2011/Power-Arch-32-bit-ABI-supp-1.0-Unified.pdf)
places the thread pointer 0x7000 bytes past the start of the first TLS block
and defines a 0x8000-byte DTV pointer bias. LibELF now calculates this layout
and sets r2; the QEMU probe checks TCB placement, TLS alignment, and an r2
roundtrip. The probe also runs LibC's `__tls_get_addr()` against a static TLS
block using the PowerPC DTV and TP biases. The PowerPC secure PLT path is
exercised with a test resolver; full loader symbol resolution and end-to-end
relocation testing remain before user processes can run.

## Big-endian audit

Big-endian support is separate from the pointer-width port. The QEMU alias
test confirms native PowerPC byte order for one RAM value; it does not validate
Serenity's file formats, device protocols, or applications. Current audit
targets include:

| Area | Finding to investigate |
| --- | --- |
| AK | The short-string tag and float parser's eight-digit byte packing handle either host byte order. QEMU checks both layouts and parses decimals with the full parser on PowerPC; a native little-endian parser harness checks the same cases. The stack unwinder now follows the PowerPC backchain while preserving the existing frame layouts. Other AK data representations still need review. |
| ELF and loader | LibELF selects native ELF32 big-endian types on PowerPC. QEMU validates headers, parses an image, applies REL, RELA, and RELR relative relocations, and checks the PowerPC TLS layout, r2 accessor, static `__tls_get_addr()` lookup, entry transfer, and secure PLT first and second calls using a test resolver. `DynamicObject` decodes native symbol and relocation fields; full loader symbol resolution, dynamic TLS relocation application, and `ELFBuild.cpp` still need a complete ELF32 big-endian path. |
| Kernel PCI | Open Firmware PCI address cells are now decoded with explicit big-endian 32- and 64-bit fields, and the 64-bit BAR cursor stays 64-bit on a 32-bit CPU. The same wire bytes and address alignment cases pass a native little-endian test and the PowerPC QEMU probe. Other PCI configuration and device-register byte orders still need review. |
| Other kernel code | `Tasks/Coredump.cpp` writes ELF64 little-endian. Storage and device registers need protocol-specific byte-order checks. |
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
