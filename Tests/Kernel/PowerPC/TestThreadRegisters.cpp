/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/ArchSpecificThreadData.h>
#include <Kernel/Arch/CreateThread.h>
#include <Kernel/Arch/ForkRegisters.h>
#include <Kernel/Arch/ProcessEntry.h>
#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Arch/ThreadRegisters.h>
#include <Kernel/Arch/powerpc/ExceptionState.h>
#include <Kernel/API/Syscall.h>

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
    Kernel::set_process_entry_arguments(registers, 3, 0x10030000u, 0x10030100u);
    if (registers.gpr[3] != 3 || registers.gpr[4] != 0x10030000u
        || registers.gpr[5] != 0x10030100u || registers.sp() != 0x10030ff0u
        || Kernel::process_entry_hwcap() != 0)
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

    Kernel::Syscall::SC_create_thread_params parameters {};
    parameters.entry = reinterpret_cast<void* (*)(void*)>(0x10012000u);
    parameters.entry_argument = reinterpret_cast<void*>(0x10021000u);
    parameters.stack_location = reinterpret_cast<void*>(0x10030000u);
    parameters.stack_size = 0x2000u;
    parameters.tls_pointer = reinterpret_cast<void*>(0x10050000u);
    Kernel::ArchSpecificThreadData thread_data {};
    Kernel::ThreadRegisters new_thread {};
    new_thread.set_initial_state(false, first_directory, 0xc0fd0000u);
    new_thread.set_ip(0x10001000u);
    new_thread.set_sp(0x10032000u);
    Kernel::prepare_user_thread_registers(new_thread, thread_data, first_directory, parameters);
    if (new_thread.gpr[3] != 0x10012000u || new_thread.gpr[4] != 0x10021000u
        || new_thread.gpr[5] != 0x10030000u || new_thread.gpr[6] != 0x2000u
        || new_thread.gpr[2] != 0x10050000u || new_thread.sp() != 0x10032000u
        || new_thread.ip() != 0x10001000u || new_thread.page_directory != &first_directory
        || new_thread.kernel_sp != 0xc0fd0000u)
        return 0;

    return (registers.srr1 & 0x6030u) == (powerpc_user_probe_frame.srr1 & 0x6030u)
        && registers.sp() == powerpc_user_probe_frame.gpr[1];
}
