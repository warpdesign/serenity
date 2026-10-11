/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/API/Syscall.h>
#include <Kernel/Arch/ArchSpecificThreadData.h>
#include <Kernel/Arch/CreateThread.h>
#include <Kernel/Arch/ThreadRegisters.h>

namespace Kernel {

void prepare_user_thread_registers(ThreadRegisters& registers, ArchSpecificThreadData&, Memory::PageDirectory const& directory, Syscall::SC_create_thread_params const& params)
{
    registers.page_directory = &directory;
    registers.gpr[3] = bit_cast<FlatPtr>(params.entry);
    registers.gpr[4] = bit_cast<FlatPtr>(params.entry_argument);
    registers.gpr[5] = bit_cast<FlatPtr>(params.stack_location);
    registers.gpr[6] = params.stack_size;
    registers.gpr[2] = bit_cast<FlatPtr>(params.tls_pointer);
}

}
