/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022-2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/SyscallEntry.h>
#include <Kernel/Arch/riscv64/CSR.h>

namespace Kernel {

void prepare_syscall_entry()
{
    RISCV64::CSR::clear_bits<RISCV64::CSR::Address::SSTATUS>(RISCV64::CSR::SSTATUS::Bit::SUM);
}

void validate_syscall_entry_registers(RegisterState const&)
{
}

}
