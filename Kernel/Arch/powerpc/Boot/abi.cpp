/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>
#include <Kernel/Arch/powerpc/MemoryLayout.h>

#ifndef AK_OS_SERENITY
#    error The PowerPC boot image must be built with a SerenityOS target compiler
#endif

static_assert(ARCH(POWERPC32));
static_assert(sizeof(void*) == 4);
static_assert(sizeof(FlatPtr) == 4);
static_assert(sizeof(size_t) == 4);
static_assert(__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__);

extern "C" u32 powerpc_kernel_mapping_base()
{
    return KERNEL_MAPPING_BASE;
}
