/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/API/Syscall.h>
#include <Kernel/Arch/ArchSpecificThreadData.h>
#include <Kernel/Arch/CreateThread.h>
#include <Kernel/Arch/PageDirectory.h>
#include <Kernel/Arch/ThreadRegisters.h>

namespace Kernel {

void prepare_user_thread_registers(ThreadRegisters& registers, ArchSpecificThreadData& data, Memory::PageDirectory const& directory, Syscall::SC_create_thread_params const& params)
{
    registers.set_flags(0x0202);
    registers.cr3 = directory.cr3();
    registers.rdi = bit_cast<FlatPtr>(params.entry);
    registers.rsi = bit_cast<FlatPtr>(params.entry_argument);
    registers.rdx = bit_cast<FlatPtr>(params.stack_location);
    registers.rcx = params.stack_size;
    data.fs_base = bit_cast<FlatPtr>(params.tls_pointer);
}

}
