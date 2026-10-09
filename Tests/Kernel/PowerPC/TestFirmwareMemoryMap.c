/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/OpenFirmwareMemoryMap.h>
#include <Kernel/Boot/BootMemoryMap.h>

int powerpc_firmware_memory_map_test(struct BootMemoryRange const* discovered, unsigned int discovered_count, unsigned long long discovered_total)
{
    struct BootMemoryRange ranges[2];
    unsigned int count;
    unsigned long long total;
    unsigned int cells[] = { 0x10000000u, 0x02000000u, 0x30000000u, 0x01000000u };
    if (!powerpc_parse_open_firmware_memory_reg(cells, 4, 1, 1, ranges, 2, &count, &total)
        || count != 2 || total != 0x03000000ull
        || ranges[0].physical_start != 0x10000000ull || ranges[0].length != 0x02000000ull
        || ranges[1].physical_start != 0x30000000ull || ranges[1].length != 0x01000000ull)
        return 0;

    unsigned int wide_cells[] = { 0, 0x20000000u, 0, 0x10000000u };
    if (!powerpc_parse_open_firmware_memory_reg(wide_cells, 4, 2, 2, ranges, 2, &count, &total)
        || count != 1 || total != 0x10000000ull || ranges[0].physical_start != 0x20000000ull)
        return 0;

    wide_cells[0] = 1;
    if (powerpc_parse_open_firmware_memory_reg(wide_cells, 4, 2, 2, ranges, 2, &count, &total))
        return 0;
    unsigned int overflow_cells[] = { 0xfffff000u, 0x2000u };
    unsigned int empty_cells[] = { 0, 0 };
    unsigned int overlapping_cells[] = { 0x10000000u, 0x2000u, 0x10001000u, 0x1000u };
    if (powerpc_parse_open_firmware_memory_reg(overflow_cells, 2, 1, 1, ranges, 2, &count, &total)
        || powerpc_parse_open_firmware_memory_reg(empty_cells, 2, 1, 1, ranges, 2, &count, &total)
        || powerpc_parse_open_firmware_memory_reg(overlapping_cells, 4, 1, 1, ranges, 2, &count, &total)
        || powerpc_parse_open_firmware_memory_reg(cells, 4, 1, 1, ranges, 1, &count, &total))
        return 0;

    if (!discovered || discovered_count == 0 || discovered_total == 0)
        return 0;
    unsigned long long sum = 0;
    for (unsigned int index = 0; index < discovered_count; ++index) {
        if (discovered[index].length == 0 || discovered[index].physical_start + discovered[index].length > 0x100000000ull)
            return 0;
        sum += discovered[index].length;
    }
    return sum == discovered_total;
}
