/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifdef TEST_POWERPC_LAYOUT
#    include <Kernel/Arch/powerpc/MemoryLayout.h>
#endif

#include <Kernel/Prekernel/Prekernel.h>
#include <Kernel/Sections.h>

_Static_assert(sizeof(void*) == 4, "This test requires a 32-bit compiler");

#ifdef TEST_POWERPC_LAYOUT
_Static_assert(KERNEL_MAPPING_BASE == 0xC0000000u, "PowerPC kernel base must fit in a 32-bit pointer");
_Static_assert(KERNEL_HAS_EFI_BOOT == 0, "Pegasos 2 uses Open Firmware rather than EFI");
_Static_assert((unsigned long long)KERNEL_MAPPING_BASE + KERNEL_PD_SIZE < 0x100000000ull, "The kernel page-directory range must fit in 32 bits");
_Static_assert((unsigned long long)KERNEL_MAPPING_BASE + KERNEL_PT1024_OFFSET + 3ull * KERNEL_MAX_CPU_COUNT * 4096 < 0x100000000ull, "Quickmap must fit in 32 bits");
#else
_Static_assert(KERNEL_MAPPING_BASE == 0x2000000000ull, "Existing architectures must keep their kernel base");
_Static_assert(KERNEL_HAS_EFI_BOOT == 1, "Existing architectures must retain EFI boot information");
#endif
