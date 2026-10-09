/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Arch/powerpc/HashPageTable.h>

#ifdef __cplusplus
extern "C" {
#endif

// Install or remove a contiguous run of supervisor-only, writable RAM pages.
// Mapping rolls back every page in the run if any insertion fails.
int powerpc_map_kernel_page_array(struct PowerPCHashPageTable const*, unsigned int virtual_base, unsigned int physical_base, unsigned int page_count);
int powerpc_unmap_kernel_page_array(struct PowerPCHashPageTable const*, unsigned int virtual_base, unsigned int page_count);

#ifdef __cplusplus
}
#endif
