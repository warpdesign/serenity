/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Boot/BootMemoryMap.h>

#ifdef __cplusplus
extern "C" {
#endif

// The caller passes host-endian 32-bit cells read from Open Firmware. The
// PowerPC firmware client interface and this target both use big endian.
int powerpc_parse_open_firmware_memory_reg(unsigned int const* cells, unsigned int cell_count,
    unsigned int address_cells, unsigned int size_cells, struct BootMemoryRange* ranges,
    unsigned int range_capacity, unsigned int* range_count, unsigned long long* total_bytes);

#ifdef __cplusplus
}
#endif
