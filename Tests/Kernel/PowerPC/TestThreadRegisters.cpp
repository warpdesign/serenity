/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/ArchSpecificThreadData.h>
#include <Kernel/Arch/ForkRegisters.h>
#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Arch/ThreadRegisters.h>
#include <Kernel/Arch/powerpc/ExceptionState.h>

// The boot probe has no page-directory allocator yet. These objects let us
// check that a thread retains the address space selected at startup and exec.
namespace Kernel::Memory {
class PageDirectory {
};
}

static_assert(sizeof(Kernel::ArchSpecificThreadData) == 1);

extern "C" volatile PowerPCExceptionState powerpc_user_probe_frame;

extern "C" int powerpc_thread_registers_test()
{
    Kernel::Memory::PageDirectory first_directory;
    Kernel::Memory::PageDirectory second_directory;
    Kernel::ThreadRegisters registers {};
    registers.set_initial_state(true, first_directory, 0xc0ff0000u);
    if (registers.sp() != 0xc0ff0000u || registers.frame_pointer() != 0xc0ff0000u
        || registers.kernel_sp != 0xc0ff0000u || registers.page_directory != &first_directory
        || (registers.srr1 & 0x4000u) != 0 || (registers.srr1 & 0x30u) != 0x30u)
        return 0;

    registers.set_entry_function(0xc0001000u, 0x12345678u);
    if (registers.ip() != 0xc0001000u || registers.gpr[3] != 0x12345678u)
        return 0;

    registers.gpr[2] = 0x87654321u;
    registers.set_exec_state(0x10010000u, 0x10030ff0u, second_directory);
    if (registers.ip() != 0x10010000u || registers.sp() != 0x10030ff0u
        || registers.page_directory != &second_directory || registers.gpr[2] != 0
        || (registers.srr1 & 0x4000u) == 0 || (registers.srr1 & 0x30u) != 0x30u)
        return 0;

    Kernel::RegisterState parent {};
    for (unsigned index = 0; index < 32; ++index)
        parent.gpr[index] = 0x10000000u + index * 0x101u;
    parent.gpr[1] = 0x10030ff0u;
    parent.gpr[3] = 0xdeadbeefu;
    parent.cr = 0x01234567u;
    parent.ctr = 0x23456789u;
    parent.xer = 0x3456789au;
    parent.lr = 0x456789abu;
    parent.srr0 = 0x10001234u;
    parent.srr1 = 0xe030u;

    Kernel::ThreadRegisters child {};
    child.set_initial_state(false, second_directory, 0xc0fe0000u);
    Kernel::initialize_fork_registers(child, parent);
    for (unsigned index = 0; index < 32; ++index) {
        if (child.gpr[index] != (index == 3 ? 0u : parent.gpr[index]))
            return 0;
    }
    if (child.cr != parent.cr || child.ctr != parent.ctr || child.xer != parent.xer
        || child.lr != parent.lr || child.srr0 != parent.srr0 || child.srr1 != parent.srr1
        || child.page_directory != &second_directory || child.kernel_sp != 0xc0fe0000u)
        return 0;

    return (registers.srr1 & 0x6030u) == (powerpc_user_probe_frame.srr1 & 0x6030u)
        && registers.sp() == powerpc_user_probe_frame.gpr[1];
}
