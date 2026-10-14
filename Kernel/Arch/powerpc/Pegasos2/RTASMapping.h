/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Arch/powerpc/Pegasos2/RTAS.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Memory/TypedMapping.h>

namespace Kernel::PowerPC::Pegasos2 {

inline ErrorOr<Memory::TypedMapping<volatile PowerPCRtasArguments>> map_rtas_arguments(PhysicalAddress physical_address)
{
    if (physical_address.offset_in_page() != 0)
        return EINVAL;
    Memory::TypedMapping<volatile PowerPCRtasArguments> mapping;
    mapping.region = TRY(MM.allocate_mmio_kernel_region(physical_address, PAGE_SIZE, {},
        Memory::Region::Access::ReadWrite, Memory::MemoryType::Normal));
    mapping.paddr = physical_address;
    mapping.length = sizeof(PowerPCRtasArguments);
    return mapping;
}

}
