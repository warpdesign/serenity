/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/PageFault.h>
#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/TrapFrame.h>
#include <Kernel/Arch/powerpc/KernelContext.h>
#include <Kernel/Arch/powerpc/HashPageFault.h>
#include <Kernel/Arch/powerpc/RegisterState.h>

namespace Kernel {

extern "C" [[noreturn]] void powerpc_instruction_storage_exception_handler(RegisterState*);

extern "C" [[noreturn]] void powerpc_instruction_storage_exception_handler(RegisterState* registers)
{
    VERIFY(registers);
    auto previous_mode = registers->previous_mode();
    TrapFrame trap { registers };
    Processor::current().enter_trap(trap, false);

    PageFault fault { VirtualAddress { registers->ip() } };
    fault.set_mode(previous_mode);
    fault.set_access(PageFault::Access::Read);
    fault.set_instruction_fetch(true);
    // MPC750 has no supervisor-mode access prevention facility.
    fault.set_was_smap_disabled(true);
    if (!Memory::try_refill_powerpc_hash_page(fault.vaddr()))
        fault.handle(*registers);

    Processor::disable_interrupts();
    Processor::current().exit_trap(trap);
    if (previous_mode == ExecutionMode::User)
        powerpc_return_to_user_from_high(*registers);
    powerpc_restore_supervisor_exception_state(*registers);
}

}
