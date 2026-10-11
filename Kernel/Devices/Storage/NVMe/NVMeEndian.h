/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Concepts.h>
#include <AK/Endian.h>

namespace Kernel {

// Use these only for raw MMIO and DMA doorbell scalars. Submission and
// completion fields already use LittleEndian<T> and convert on assignment.
template<Integral T>
ALWAYS_INLINE T read_nvme_register(volatile T const& register_value)
{
    T value = register_value;
    return AK::convert_between_host_and_little_endian(value);
}

template<Integral T>
ALWAYS_INLINE void write_nvme_register(volatile T& register_value, T value)
{
    register_value = AK::convert_between_host_and_little_endian(value);
}

}
