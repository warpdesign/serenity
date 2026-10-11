/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>
#include <Kernel/Memory/MemoryType.h>
#include <Kernel/Memory/PhysicalAddress.h>

namespace Kernel::Memory {

// The complete state to install for one virtual page. Architectures apply
// this as one mapping operation rather than exposing a mutable hardware PTE.
struct PageMappingRequest {
    PhysicalPtr physical_page_base { 0 };
    MemoryType memory_type { MemoryType::Normal };
    bool writable { false };
    bool user_allowed { false };
    bool execute_disabled { false };
    bool global { false };
};

struct PageMappingInfo {
    PhysicalPtr physical_page_base { 0 };
    bool present { false };
    bool writable { false };
    bool user_allowed { false };
    bool execute_disabled { false };
};

}
