/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/OpenFirmwareMemoryMap.h>

static_assert(sizeof(unsigned int) == 4);

int powerpc_parse_open_firmware_memory_reg(unsigned int const* cells, unsigned int cell_count,
    unsigned int address_cells, unsigned int size_cells, BootMemoryRange* ranges,
    unsigned int range_capacity, unsigned int* range_count, unsigned long long* total_bytes)
{
    if (!range_count || !total_bytes)
        return 0;
    *range_count = 0;
    *total_bytes = 0;

    if (!cells || !ranges || (address_cells != 1 && address_cells != 2)
        || (size_cells != 1 && size_cells != 2))
        return 0;
    unsigned int stride = address_cells + size_cells;
    if (cell_count == 0 || cell_count % stride != 0 || cell_count / stride > range_capacity)
        return 0;

    unsigned long long total = 0;
    unsigned int count = 0;
    for (unsigned int index = 0; index < cell_count; index += stride) {
        unsigned long long base = cells[index + address_cells - 1];
        if (address_cells == 2)
            base |= static_cast<unsigned long long>(cells[index]) << 32;
        unsigned long long length = cells[index + stride - 1];
        if (size_cells == 2)
            length |= static_cast<unsigned long long>(cells[index + address_cells]) << 32;
        if (base >= 0x100000000ull || length == 0 || length > 0x100000000ull - base
            || total > 0x100000000ull - length)
            return 0;

        unsigned long long end = base + length;
        for (unsigned int previous = 0; previous < count; ++previous) {
            unsigned long long previous_end = ranges[previous].physical_start + ranges[previous].length;
            if (base < previous_end && ranges[previous].physical_start < end)
                return 0;
        }
        ranges[count++] = BootMemoryRange { base, length };
        total += length;
    }

    *range_count = count;
    *total_bytes = total;
    return 1;
}
