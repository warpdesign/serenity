/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>
#include <Kernel/Boot/BootInfo.h>

static_assert(ARCH(POWERPC32));
static_assert(sizeof(FlatPtr) == 4);
static_assert(sizeof(decltype(Kernel::BootInfo::kernel_mapping_base)) == 4);
static_assert(sizeof(decltype(Kernel::ArchSpecificBootInfo::open_firmware_entry)) == 4);
