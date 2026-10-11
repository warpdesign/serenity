/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/PageFault.h>
#include <Kernel/Arch/powerpc/KernelContext.h>
#include <Kernel/Arch/powerpc/RegisterState.h>
#include <Kernel/Library/Panic.h>

namespace Kernel {

extern "C" [[noreturn]] void powerpc_data_storage_exception_handler(RegisterState*, u32, u32);

extern "C" [[noreturn]] void powerpc_data_storage_exception_handler(RegisterState* registers, u32 fault_address, u32 status)
{
    VERIFY(registers);
    VERIFY(registers->previous_mode() == ExecutionMode::Kernel);

    PageFault fault { VirtualAddress { fault_address } };
    fault.set_mode(ExecutionMode::Kernel);
    fault.set_access((status & 0x02000000u) ? PageFault::Access::Write : PageFault::Access::Read);
    fault.set_type((status & 0x08000000u) ? PageFault::Type::ProtectionViolation : PageFault::Type::PageNotPresent);
    fault.handle(*registers);
    powerpc_restore_supervisor_exception_state(*registers);
}

}
