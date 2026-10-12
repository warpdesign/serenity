/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Arch/powerpc/VSIDAllocator.h>

#ifdef __cplusplus
extern "C" {
#endif

// All address spaces share SDR1's HPT. Each user directory owns a unique
// block of VSIDs; callers serialize updates to the shared HPT.
struct PowerPCPageDirectoryBackend {
    struct PowerPCHashPageTable const* table;
    PowerPCVSIDWord user_vsid_base;
};

int powerpc_page_directory_backend_initialize(struct PowerPCPageDirectoryBackend*, struct PowerPCHashPageTable const*, struct PowerPCVSIDAllocator*);
PowerPCVSIDWord powerpc_page_directory_backend_vsid_for(struct PowerPCPageDirectoryBackend const*, PowerPCHPTWord effective_address);
int powerpc_page_directory_backend_map(struct PowerPCPageDirectoryBackend const*, PowerPCHPTWord effective_address, PowerPCHPTWord physical_address, PowerPCHPTWord lower_flags);
int powerpc_page_directory_backend_remap(struct PowerPCPageDirectoryBackend const*, PowerPCHPTWord effective_address, PowerPCHPTWord physical_address, PowerPCHPTWord lower_flags);
int powerpc_page_directory_backend_unmap(struct PowerPCPageDirectoryBackend const*, PowerPCHPTWord effective_address);
int powerpc_page_directory_backend_purge(struct PowerPCPageDirectoryBackend const*);
int powerpc_page_directory_backend_activate(struct PowerPCPageDirectoryBackend const*);
int powerpc_set_kernel_page_writable(struct PowerPCHashPageTable const*, PowerPCHPTWord effective_address, int writable);
int powerpc_protect_kernel_page_range(struct PowerPCHashPageTable const*, PowerPCHPTWord start, PowerPCHPTWord end);
int powerpc_unmap_kernel_page_range(struct PowerPCHashPageTable const*, PowerPCHPTWord start, PowerPCHPTWord end);

#ifdef __cplusplus
}

#    include <AK/Optional.h>
#    include <Kernel/Memory/PageMapping.h>

bool powerpc_page_directory_backend_apply_mapping(PowerPCPageDirectoryBackend const*, PowerPCHPTWord effective_address, Kernel::Memory::PageMappingRequest const&);
Optional<Kernel::Memory::PageMappingInfo> powerpc_page_directory_backend_query_mapping(PowerPCPageDirectoryBackend const*, PowerPCHPTWord effective_address);
bool powerpc_apply_kernel_mapping(PowerPCHashPageTable const*, PowerPCHPTWord effective_address, Kernel::Memory::PageMappingRequest const&);
PowerPCHPTWord powerpc_page_mapping_lower_flags(Kernel::Memory::PageMappingRequest const&, bool user_allowed);
#endif
