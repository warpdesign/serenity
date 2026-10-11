/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>
#include <Kernel/Arch/powerpc/ExceptionState.h>
#include <Kernel/Arch/powerpc/ExceptionVectorAddresses.h>
#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Arch/powerpc/Time/Timebase.h>
#include <Kernel/Boot/BootInfo.h>
#include <Kernel/Sections.h>

static_assert(ARCH(POWERPC32));
static_assert(KERNEL_HAS_EFI_BOOT == 0);
static_assert(KERNEL_MAPPING_BASE == 0xc0000000u);
static_assert(sizeof(FlatPtr) == 4);
static_assert(sizeof(BootMemoryRange) == 16);
static_assert(sizeof(BootMemoryMap) == 16);
static_assert(sizeof(decltype(Kernel::BootInfo::kernel_mapping_base)) == 4);
static_assert(sizeof(decltype(Kernel::ArchSpecificBootInfo::open_firmware_entry)) == 4);
static_assert(sizeof(decltype(Kernel::ArchSpecificBootInfo::hpt_virtual_address)) == 4);
static_assert(sizeof(decltype(Kernel::ArchSpecificBootInfo::timebase_frequency_hz)) == 4);
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
extern "C" u32 powerpc_quickmap_probe_address();

extern "C" int powerpc_boot_handoff_test(BootMemoryRange const* ranges, u32 range_count, unsigned long long total_bytes, u32 timebase_frequency)
{
    if (!ranges || range_count == 0 || total_bytes == 0)
        return 0;
    if (powerpc_quickmap_probe_address() != KERNEL_MAPPING_BASE + KERNEL_PT1024_OFFSET + 3 * KERNEL_MAX_CPU_COUNT * POWERPC_HPT_PAGE_SIZE)
        return 0;

    BootMemoryRange reservations[] = {
        { 0, POWERPC_EXCEPTION_CODE_LIMIT },
        { powerpc_hpt_table_physical(), 64 * 1024 },
    };
    Kernel::BootInfo boot_info {};
    boot_info.boot_method = Kernel::BootMethod::FirmwareMemoryMap;
    boot_info.boot_method_specific.firmware_memory_map = BootMemoryMap { ranges, range_count, reservations, 2 };
    boot_info.kernel_mapping_base = KERNEL_MAPPING_BASE;
    boot_info.arch_specific.hpt_virtual_address = powerpc_hpt_table_physical();
    boot_info.arch_specific.hpt_physical_address = PhysicalAddress { powerpc_hpt_table_physical() };
    boot_info.arch_specific.hpt_group_count = powerpc_hpt_group_count();
    boot_info.arch_specific.timebase_frequency_hz = timebase_frequency;

    auto frequency = Kernel::PowerPC::boot_timebase_frequency(boot_info);
    if (!frequency.has_value() || frequency.value() != timebase_frequency
        || Kernel::PowerPC::timebase_ticks_to_nanoseconds(timebase_frequency, frequency.value()) != 1'000'000'000ull
        || Kernel::PowerPC::timebase_ticks_to_nanoseconds(static_cast<u64>(timebase_frequency) * 600, frequency.value()) != 600'000'000'000ull
        || Kernel::PowerPC::timebase_ticks_to_nanoseconds(~0ull, frequency.value()) != ~0ull
        || Kernel::PowerPC::timebase_ticks_for_microseconds(0, frequency.value()) != 0
        || Kernel::PowerPC::timebase_ticks_for_microseconds(1'000'000, frequency.value()) != frequency.value()
        || Kernel::PowerPC::timebase_ticks_for_microseconds(1, frequency.value()) == 0)
        return 0;
    auto start = Kernel::PowerPC::read_timebase();
    auto delay_ticks = Kernel::PowerPC::timebase_ticks_for_microseconds(50, frequency.value());
    Kernel::PowerPC::delay_timebase_ticks(delay_ticks);
    auto elapsed = Kernel::PowerPC::read_timebase() - start;
    if (elapsed < delay_ticks || elapsed > frequency.value())
        return 0;
    boot_info.arch_specific.timebase_frequency_hz = 0;
    if (Kernel::PowerPC::boot_timebase_frequency(boot_info).has_value())
        return 0;
    boot_info.arch_specific.timebase_frequency_hz = 249;
    if (Kernel::PowerPC::boot_timebase_frequency(boot_info).has_value())
        return 0;
    boot_info.arch_specific.timebase_frequency_hz = timebase_frequency;

    PowerPCHashPageTable table {};
    if (!Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table)
        || table.entries != reinterpret_cast<PowerPCHPTEntry*>(boot_info.arch_specific.hpt_virtual_address)
        || table.group_count != 1024
        || boot_info.boot_method_specific.firmware_memory_map.usable_ranges != ranges
        || boot_info.boot_method_specific.firmware_memory_map.reserved_ranges[0].physical_start != 0
        || boot_info.boot_method_specific.firmware_memory_map.reserved_ranges[1].physical_start != powerpc_hpt_table_physical())
        return 0;

    auto first_reservation = reservations[0];
    reservations[0] = reservations[1];
    reservations[1] = first_reservation;
    if (!Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    reservations[1] = reservations[0];
    reservations[0] = first_reservation;

    boot_info.kernel_mapping_base = 0xd0000000u;
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    boot_info.kernel_mapping_base = KERNEL_MAPPING_BASE;
    boot_info.boot_method_specific.firmware_memory_map.reserved_range_count = 0;
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    boot_info.boot_method_specific.firmware_memory_map.reserved_range_count = 1;
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    boot_info.boot_method_specific.firmware_memory_map.reserved_range_count = 2;
    reservations[0].length = POWERPC_EXCEPTION_CODE_LIMIT / 2;
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    reservations[0].length = POWERPC_EXCEPTION_CODE_LIMIT;
    reservations[1].physical_start += 0x20000u;
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    reservations[1].physical_start -= 0x20000u;
    boot_info.arch_specific.hpt_physical_address = PhysicalAddress { powerpc_hpt_table_physical() + 0x1000u };
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    boot_info.arch_specific.hpt_physical_address = PhysicalAddress { powerpc_hpt_table_physical() + 0x10000u };
    reservations[1].physical_start += 0x10000u;
    if (Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table))
        return 0;
    reservations[1].physical_start -= 0x10000u;
    boot_info.arch_specific.hpt_physical_address = PhysicalAddress { powerpc_hpt_table_physical() };
    boot_info.arch_specific.hpt_group_count = 512;
    return !Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, table);
}
