/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>
#include <Kernel/Arch/powerpc/ExceptionState.h>
#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Boot/BootInfo.h>

static_assert(ARCH(POWERPC32));
static_assert(KERNEL_HAS_EFI_BOOT == 0);
static_assert(KERNEL_MAPPING_BASE == 0xc0000000u);
static_assert(sizeof(FlatPtr) == 4);
static_assert(sizeof(BootMemoryRange) == 16);
static_assert(sizeof(BootMemoryMap) == 16);
static_assert(sizeof(decltype(Kernel::BootInfo::kernel_mapping_base)) == 4);
static_assert(sizeof(decltype(Kernel::ArchSpecificBootInfo::open_firmware_entry)) == 4);
static_assert(sizeof(decltype(Kernel::ArchSpecificBootInfo::hpt_virtual_address)) == 4);
static_assert(sizeof(PowerPCHPTEntry) == 8);
static_assert(sizeof(PowerPCHashPageTable) == 8);
static_assert(sizeof(PowerPCExceptionState) == 152);
static_assert(__builtin_offsetof(PowerPCExceptionState, cr) == 128);
static_assert(__builtin_offsetof(PowerPCExceptionState, ctr) == 132);
static_assert(__builtin_offsetof(PowerPCExceptionState, lr) == 140);
static_assert(__builtin_offsetof(PowerPCExceptionState, srr0) == 144);
static_assert(__builtin_offsetof(PowerPCExceptionState, srr1) == 148);

extern "C" u32 powerpc_hpt_table_physical();
extern "C" u32 powerpc_hpt_group_count();

extern "C" int powerpc_boot_handoff_test(BootMemoryRange const* ranges, u32 range_count, unsigned long long total_bytes)
{
    if (!ranges || range_count == 0 || total_bytes == 0)
        return 0;

    BootMemoryRange hpt_reservation { powerpc_hpt_table_physical(), 64 * 1024 };
    Kernel::BootInfo boot_info {};
    boot_info.boot_method = Kernel::BootMethod::FirmwareMemoryMap;
    boot_info.boot_method_specific.firmware_memory_map = BootMemoryMap { ranges, range_count, &hpt_reservation, 1 };
    boot_info.kernel_mapping_base = KERNEL_MAPPING_BASE;
    boot_info.arch_specific.hpt_virtual_address = powerpc_hpt_table_physical();
    boot_info.arch_specific.hpt_physical_address = PhysicalAddress { powerpc_hpt_table_physical() };
    boot_info.arch_specific.hpt_group_count = powerpc_hpt_group_count();

    PowerPCHashPageTable table {};
    if (!Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table)
        || table.entries != reinterpret_cast<PowerPCHPTEntry*>(boot_info.arch_specific.hpt_virtual_address)
        || table.group_count != 1024
        || boot_info.boot_method_specific.firmware_memory_map.usable_ranges != ranges
        || boot_info.boot_method_specific.firmware_memory_map.reserved_ranges[0].physical_start != powerpc_hpt_table_physical())
        return 0;

    boot_info.boot_method_specific.firmware_memory_map.reserved_range_count = 0;
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    boot_info.boot_method_specific.firmware_memory_map.reserved_range_count = 1;
    hpt_reservation.physical_start += 0x20000u;
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    hpt_reservation.physical_start -= 0x20000u;
    boot_info.arch_specific.hpt_physical_address = PhysicalAddress { powerpc_hpt_table_physical() + 0x1000u };
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    boot_info.arch_specific.hpt_physical_address = PhysicalAddress { powerpc_hpt_table_physical() + 0x10000u };
    hpt_reservation.physical_start += 0x10000u;
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    hpt_reservation.physical_start -= 0x10000u;
    boot_info.arch_specific.hpt_physical_address = PhysicalAddress { powerpc_hpt_table_physical() };
    boot_info.arch_specific.hpt_group_count = 512;
    return !Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table);
}
