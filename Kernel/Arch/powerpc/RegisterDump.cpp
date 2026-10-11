/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Format.h>
#include <Kernel/Arch/CPU.h>
#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Debug.h>

namespace Kernel {

void dump_registers(RegisterState const& regs)
{
    dbgln("SRR0={:08x} SRR1={:08x} LR={:08x} CTR={:08x}", regs.srr0, regs.srr1, regs.lr, regs.ctr);
    dbgln("CR={:08x} XER={:08x}", regs.cr, regs.xer);
    for (u32 index = 0; index < 32; index += 4) {
        dbgln("r{}={:08x} r{}={:08x} r{}={:08x} r{}={:08x}",
            index, regs.gpr[index], index + 1, regs.gpr[index + 1],
            index + 2, regs.gpr[index + 2], index + 3, regs.gpr[index + 3]);
    }
}

}
