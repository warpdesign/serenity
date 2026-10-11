/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/OpenFirmwarePCIRanges.h>

int powerpc_open_firmware_pci_ranges_test(unsigned int const* firmware_cells, unsigned int firmware_cell_count,
    unsigned int parent_address_cells, unsigned int size_cells)
{
    struct PowerPCPCIRange ranges[8];
    unsigned int count;
    unsigned int synthetic[] = {
        0x01000000u, 0, 0, 0xfe000000u, 0x10000u,
        0x02000000u, 0, 0x80000000u, 0x80000000u, 0x10000000u,
    };
    if (!powerpc_parse_open_firmware_pci_ranges(synthetic, 10, 3, 1, 1, ranges, 8, &count)
        || count != 2 || ranges[0].space != 1 || ranges[0].bus_address != 0
        || ranges[0].host_address != 0xfe000000ull || ranges[0].length != 0x10000ull
        || ranges[1].space != 2 || ranges[1].bus_address != 0x80000000ull
        || ranges[1].host_address != 0x80000000ull)
        return 0;

    unsigned int wide[] = { 0x03000000u, 1, 0, 0, 0x90000000u, 0, 0x10000000u };
    if (!powerpc_parse_open_firmware_pci_ranges(wide, 7, 3, 2, 2, ranges, 8, &count)
        || count != 1 || ranges[0].space != 3 || ranges[0].bus_address != 0x100000000ull
        || ranges[0].host_address != 0x90000000ull || ranges[0].length != 0x10000000ull)
        return 0;

    unsigned int overflow[] = { 0x01000000u, 0, 0, 0xfffff000u, 0x2000u };
    unsigned int empty[] = { 0x01000000u, 0, 0, 0xfe000000u, 0 };
    if (powerpc_parse_open_firmware_pci_ranges(overflow, 5, 3, 1, 1, ranges, 8, &count)
        || powerpc_parse_open_firmware_pci_ranges(empty, 5, 3, 1, 1, ranges, 8, &count)
        || powerpc_parse_open_firmware_pci_ranges(synthetic, 9, 3, 1, 1, ranges, 8, &count)
        || powerpc_parse_open_firmware_pci_ranges(synthetic, 10, 2, 1, 1, ranges, 8, &count)
        || powerpc_parse_open_firmware_pci_ranges(synthetic, 10, 3, 1, 1, ranges, 1, &count))
        return 0;

    if (!powerpc_parse_open_firmware_pci_ranges(firmware_cells, firmware_cell_count,
            3, parent_address_cells, size_cells, ranges, 8, &count))
        return 0;
    int uart_window_found = 0;
    int memory_window_found = 0;
    for (unsigned int index = 0; index < count; ++index) {
        struct PowerPCPCIRange const* range = &ranges[index];
        if (range->space == 1 && range->host_address <= PEGASOS2_UART_PHYSICAL_BASE
            && PEGASOS2_UART_PHYSICAL_BASE - range->host_address < range->length
            && range->bus_address + PEGASOS2_UART_PHYSICAL_BASE - range->host_address == 0x2f8u)
            uart_window_found = 1;
        if ((range->space == 2 || range->space == 3) && range->length >= 0x100000u)
            memory_window_found = 1;
    }
    return uart_window_found && memory_window_found;
}
