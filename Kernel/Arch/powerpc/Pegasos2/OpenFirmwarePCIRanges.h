/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#define POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT 2
#define POWERPC_PEGASOS2_PCI_MAX_RANGES 8

struct PowerPCPCIRange {
    unsigned int space;
    unsigned long long bus_address;
    unsigned long long host_address;
    unsigned long long length;
};

struct PowerPCPCIHostBridgeInfo {
    unsigned int bridge_number;
    unsigned int start_bus;
    unsigned int end_bus;
    unsigned int range_count;
    struct PowerPCPCIRange ranges[POWERPC_PEGASOS2_PCI_MAX_RANGES];
};

#ifdef __cplusplus
extern "C" {
#endif

int powerpc_parse_open_firmware_pci_ranges(unsigned int const* cells, unsigned int cell_count,
    unsigned int child_address_cells, unsigned int parent_address_cells, unsigned int size_cells,
    struct PowerPCPCIRange* ranges, unsigned int range_capacity, unsigned int* range_count);

#ifdef __cplusplus
}
#endif
