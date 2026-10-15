/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Arch/powerpc/ExceptionVectorAddresses.h>
#include <Kernel/Arch/powerpc/Time/Timebase.h>

namespace Kernel::PowerPC {

Optional<u32> boot_timebase_frequency(BootInfo const& boot_info)
{
    if (boot_info.boot_method != BootMethod::FirmwareMemoryMap
        || !decrementer_interval(boot_info.arch_specific.timebase_frequency_hz, default_decrementer_interrupts_per_second))
        return {};
    return boot_info.arch_specific.timebase_frequency_hz;
}

Optional<u32> boot_mv64361_registers_base(BootInfo const& boot_info)
{
    if (boot_info.boot_method != BootMethod::FirmwareMemoryMap)
        return {};
    auto base = boot_info.arch_specific.mv64361_registers_physical_base;
    if (base == 0 || (base & 0xffffu) != 0 || base > 0xffff0000u)
        return {};
    return base;
}

Optional<ReadonlySpan<PowerPCPCIHostBridgeInfo>> boot_pci_host_bridges(BootInfo const& boot_info)
{
    if (!boot_mv64361_registers_base(boot_info).has_value())
        return {};
    auto const& arch = boot_info.arch_specific;
    if (arch.pci_host_bridge_count != POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT)
        return {};
    for (u32 index = 0; index < arch.pci_host_bridge_count; ++index) {
        auto const& bridge = arch.pci_host_bridges[index];
        if (bridge.bridge_number != index || bridge.start_bus > bridge.end_bus || bridge.end_bus > 255
            || bridge.range_count == 0 || bridge.range_count > POWERPC_PEGASOS2_PCI_MAX_RANGES)
            return {};
        for (u32 range_index = 0; range_index < bridge.range_count; ++range_index) {
            auto const& range = bridge.ranges[range_index];
            if (range.space < 1 || range.space > 3 || range.length == 0 || range.length > 0xffffffffull
                || range.host_address >= 0x100000000ull || range.length > 0x100000000ull - range.host_address
                || range.bus_address > ~0ull - range.length)
                return {};
        }
    }
    return ReadonlySpan<PowerPCPCIHostBridgeInfo> { arch.pci_host_bridges, arch.pci_host_bridge_count };
}

// The base must be where the south bridge's I/O range maps ISA port 0, covering the 64 KiB legacy ports.
Optional<u32> boot_legacy_io_base(BootInfo const& boot_info)
{
    auto bridges = boot_pci_host_bridges(boot_info);
    auto base = boot_info.arch_specific.legacy_io_physical_base;
    if (!bridges.has_value() || base == 0 || (base & 0xffffu) != 0 || base > 0xffff0000u)
        return {};
    auto const& south = (*bridges)[0];
    for (u32 index = 0; index < south.range_count; ++index) {
        auto const& range = south.ranges[index];
        if (range.space == 1 && range.bus_address == 0 && range.host_address == base && range.length >= 0x10000)
            return base;
    }
    return {};
}

Optional<PowerPCRtasInfo> boot_rtas_info_after_handoff(BootInfo const& boot_info)
{
    if (boot_info.boot_method != BootMethod::FirmwareMemoryMap)
        return {};
    auto const& info = boot_info.arch_specific.rtas;
    if (info.runtime_base < POWERPC_EXCEPTION_CODE_LIMIT || (info.runtime_base & 4095u) != 0
        || info.runtime_size < 8192 || (info.runtime_size & 4095u) != 0
        || info.runtime_base > 0xffffffffu - info.runtime_size
        || info.arguments_physical != info.runtime_base + info.runtime_size - 4096
        || info.entry_physical < info.runtime_base || info.entry_physical >= info.arguments_physical
        || info.power_off_token == 0 || info.system_reboot_token == 0)
        return {};

    return info;
}

Optional<PowerPCRtasInfo> boot_rtas_info(BootInfo const& boot_info)
{
    auto info = boot_rtas_info_after_handoff(boot_info);
    if (!info.has_value())
        return {};
    auto const& memory_map = boot_info.boot_method_specific.firmware_memory_map;
    if (!memory_map.reserved_ranges)
        return {};
    for (u32 index = 0; index < memory_map.reserved_range_count; ++index) {
        auto const& reservation = memory_map.reserved_ranges[index];
        if (reservation.physical_start <= info->runtime_base
            && reservation.length >= info->runtime_size
            && reservation.physical_start <= ~0ull - reservation.length
            && reservation.physical_start + reservation.length >= static_cast<u64>(info->runtime_base) + info->runtime_size)
            return info;
    }
    return {};
}

bool boot_hpt_from_boot_info(BootInfo const& boot_info, PowerPCHashPageTable& table)
{
    if (boot_info.boot_method != BootMethod::FirmwareMemoryMap || boot_info.kernel_mapping_base != KERNEL_MAPPING_BASE)
        return false;
    auto const& arch = boot_info.arch_specific;
    auto physical = arch.hpt_physical_address.get();
    constexpr u32 minimum_groups = (64 * 1024) / 64;
    constexpr u32 maximum_groups = (32 * 1024 * 1024) / 64;
    auto group_count = arch.hpt_group_count;
    if (group_count < minimum_groups || group_count > maximum_groups
        || (group_count & (group_count - 1)) != 0)
        return false;
    u32 table_size = group_count * 64;
    if (physical >= 0x100000000ull || (physical & (table_size - 1)) != 0
        || table_size > 0x100000000ull - physical
        || arch.hpt_virtual_address == 0 || (arch.hpt_virtual_address & 7u) != 0)
        return false;

    auto const& memory_map = boot_info.boot_method_specific.firmware_memory_map;
    if (!memory_map.usable_ranges || memory_map.usable_range_count == 0
        || !memory_map.reserved_ranges || memory_map.reserved_range_count == 0)
        return false;
    bool hpt_reserved = false;
    bool exception_code_reserved = false;
    for (u32 index = 0; index < memory_map.reserved_range_count; ++index) {
        auto const& range = memory_map.reserved_ranges[index];
        if (range.physical_start == 0 && range.length >= POWERPC_EXCEPTION_CODE_LIMIT)
            exception_code_reserved = true;
        if (range.physical_start <= physical && range.length >= table_size
            && range.physical_start <= ~0ull - range.length
            && range.physical_start + range.length >= physical + table_size)
            hpt_reserved = true;
    }
    if (!hpt_reserved || !exception_code_reserved)
        return false;

    u32 sdr1;
    asm volatile("mfspr %0,25" : "=r"(sdr1));
    if (sdr1 != (physical | (table_size / (64 * 1024) - 1)))
        return false;

    table = PowerPCHashPageTable { reinterpret_cast<PowerPCHPTEntry*>(arch.hpt_virtual_address), arch.hpt_group_count };
    return true;
}

}
