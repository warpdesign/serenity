/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Memory/PhysicalAddress.h>

namespace Kernel::SD {

struct ADMAAddressWords {
    u32 low;
    u32 high;
};

constexpr ADMAAddressWords adma_address_words(PhysicalPtr base, u32 offset = 0)
{
    auto address = base + offset;
    return { static_cast<u32>(address), static_cast<u32>(address >> 32) };
}

}
