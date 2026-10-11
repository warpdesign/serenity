/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/OpenFirmwarePCIRanges.h>

static_assert(sizeof(unsigned int) == 4);

static unsigned long long read_cells(unsigned int const* cells, unsigned int count)
{
    unsigned long long value = cells[0];
    if (count == 2)
        value = (value << 32) | cells[1];
    return value;
}

extern "C" int powerpc_parse_open_firmware_pci_ranges(unsigned int const* cells, unsigned int cell_count,
    unsigned int child_address_cells, unsigned int parent_address_cells, unsigned int size_cells,
    PowerPCPCIRange* ranges, unsigned int range_capacity, unsigned int* range_count)
{
    if (!range_count)
        return 0;
    *range_count = 0;
    if (!cells || !ranges || child_address_cells != 3
        || (parent_address_cells != 1 && parent_address_cells != 2)
        || (size_cells != 1 && size_cells != 2))
        return 0;

    unsigned int stride = child_address_cells + parent_address_cells + size_cells;
    if (cell_count == 0 || cell_count % stride != 0)
        return 0;

    unsigned int count = 0;
    for (unsigned int index = 0; index < cell_count; index += stride) {
        unsigned int space = (cells[index] >> 24) & 3u;
        if (space == 0)
            continue;
        unsigned long long bus = read_cells(cells + index + 1, 2);
        unsigned long long host = read_cells(cells + index + child_address_cells, parent_address_cells);
        unsigned long long length = read_cells(cells + index + child_address_cells + parent_address_cells, size_cells);
        if (count >= range_capacity || length == 0 || host >= 0x100000000ull
            || length > 0x100000000ull - host || bus > ~0ull - length)
            return 0;

        ranges[count++] = PowerPCPCIRange { space, bus, host, length };
    }
    if (count == 0)
        return 0;
    *range_count = count;
    return 1;
}
