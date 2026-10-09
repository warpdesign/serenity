/*
 * Copyright (c) 2022-2026, David Tuin <davidot@serenityos.org>
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Endian.h>

namespace AK::Detail {

// The decimal fast path consumes the first character in the least significant
// byte, regardless of the host's byte order.
ALWAYS_INLINE constexpr u64 read_eight_digits(char const* string)
{
    u64 value;
    __builtin_memcpy(&value, string, sizeof(value));
    return convert_between_host_and_little_endian(value);
}

}
