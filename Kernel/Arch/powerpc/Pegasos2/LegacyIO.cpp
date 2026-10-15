/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/LegacyIO.h>
#include <Kernel/Boot/CommandLine.h>

namespace Kernel::PowerPC::Pegasos2 {

static constexpr u32 debug_uart_port = 0x2f8;

static u32 io_base(BootInfo const& boot_info)
{
    return boot_legacy_io_base(boot_info).value_or(PEGASOS2_PCI_IO_PHYSICAL_BASE);
}

// fixedio restores the addresses the kernel used before they came from the firmware.
static bool use_fixed_addresses()
{
    return kernel_command_line().contains("fixedio"sv);
}

PhysicalAddress legacy_io_physical_base()
{
    return PhysicalAddress { use_fixed_addresses() ? PEGASOS2_PCI_IO_PHYSICAL_BASE : io_base(g_boot_info) };
}

PhysicalAddress interrupt_acknowledge_physical_address()
{
    auto base = use_fixed_addresses()
        ? PEGASOS2_MV64361_REGISTERS_PHYSICAL_BASE
        : boot_mv64361_registers_base(g_boot_info).value_or(PEGASOS2_MV64361_REGISTERS_PHYSICAL_BASE);
    return PhysicalAddress { base + PEGASOS2_INTACK_OFFSET };
}

void configure_early_debug_uart(BootInfo const& boot_info)
{
    set_debug_uart_physical_address(io_base(boot_info) + debug_uart_port);
}

}
