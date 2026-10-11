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

void prepare_user_thread_registers(ThreadRegisters& registers, ArchSpecificThreadData&, Memory::PageDirectory const& directory, Syscall::SC_create_thread_params const& params)
{
    registers.ttbr0_el1 = directory.ttbr0();
    registers.x[0] = bit_cast<FlatPtr>(params.entry);
    registers.x[1] = bit_cast<FlatPtr>(params.entry_argument);
    registers.x[2] = bit_cast<FlatPtr>(params.stack_location);
    registers.x[3] = params.stack_size;
    registers.tpidr_el0 = bit_cast<FlatPtr>(params.tls_pointer);
}

}
