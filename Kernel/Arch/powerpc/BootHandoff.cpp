/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/BootHandoff.h>

namespace Kernel::PowerPC {

bool boot_hpt_from_boot_info(BootInfo const& boot_info, PowerPCHashPageTable& table)
{
    if (boot_info.boot_method != BootMethod::FirmwareMemoryMap || boot_info.kernel_mapping_base != KERNEL_MAPPING_BASE)
        return false;
    auto const& arch = boot_info.arch_specific;
    auto physical = arch.hpt_physical_address.get();
    constexpr u32 table_size = 64 * 1024;
    if (arch.hpt_group_count != table_size / 64
        || physical >= 0x100000000ull || (physical & (table_size - 1)) != 0
        || arch.hpt_virtual_address == 0 || (arch.hpt_virtual_address & 7u) != 0)
        return false;

    auto const& memory_map = boot_info.boot_method_specific.firmware_memory_map;
    if (!memory_map.usable_ranges || memory_map.usable_range_count == 0
        || !memory_map.reserved_ranges || memory_map.reserved_range_count == 0)
        return false;
    bool hpt_reserved = false;
    for (u32 index = 0; index < memory_map.reserved_range_count; ++index) {
        auto const& range = memory_map.reserved_ranges[index];
        if (range.physical_start <= physical && range.length >= table_size
            && range.physical_start <= ~0ull - range.length
            && range.physical_start + range.length >= physical + table_size) {
            hpt_reserved = true;
            break;
        }
    }
    if (!hpt_reserved)
        return false;

    u32 sdr1;
    asm volatile("mfspr %0,25" : "=r"(sdr1));
    if (sdr1 != physical)
        return false;

    table = PowerPCHashPageTable { reinterpret_cast<PowerPCHPTEntry*>(arch.hpt_virtual_address), arch.hpt_group_count };
    return true;
}

}
