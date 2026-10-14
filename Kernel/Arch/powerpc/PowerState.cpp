/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/PowerState.h>
#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Arch/powerpc/Pegasos2/RTAS.h>
#include <Kernel/Arch/powerpc/Pegasos2/RTASMapping.h>
#include <Kernel/Boot/BootInfo.h>
#include <Kernel/Interrupts/InterruptDisabler.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Memory/TypedMapping.h>

namespace Kernel {

static bool call_rtas_power_service(u32 token, u32 argument_count)
{
    auto info = PowerPC::boot_rtas_info_after_handoff(g_boot_info);
    if (!info.has_value())
        return false;

    auto mapping = PowerPC::Pegasos2::map_rtas_arguments(PhysicalAddress { info->arguments_physical });
    if (mapping.is_error())
        return false;
    auto arguments = mapping.release_value();
    arguments->token = token;
    arguments->argument_count = argument_count;
    arguments->result_count = 1;
    for (u32 index = 0; index <= argument_count; ++index)
        arguments->values[index] = 0;
    asm volatile("sync" : : : "memory");

    InterruptDisabler disable_interrupts;
    auto real_return = Memory::virtual_to_low_physical(reinterpret_cast<FlatPtr>(powerpc_rtas_return_real));
    auto virtual_return = reinterpret_cast<FlatPtr>(powerpc_rtas_return_virtual);
    auto status = powerpc_rtas_enter(info->arguments_physical, info->entry_physical, real_return, virtual_return);
    asm volatile("sync" : : : "memory");
    return status == 0 && arguments->values[argument_count] == 0;
}

void arch_specific_poweroff(PowerOffOrRebootReason)
{
    auto info = PowerPC::boot_rtas_info_after_handoff(g_boot_info);
    if (!info.has_value() || !call_rtas_power_service(info->power_off_token, 2))
        dmesgln("PowerPC: RTAS power-off unavailable or returned an error");
    Processor::halt();
}

void arch_specific_reboot(PowerOffOrRebootReason)
{
    auto info = PowerPC::boot_rtas_info_after_handoff(g_boot_info);
    if (!info.has_value() || !call_rtas_power_service(info->system_reboot_token, 0))
        dmesgln("PowerPC: RTAS system-reboot unavailable or returned an error");
    Processor::halt();
}

}
