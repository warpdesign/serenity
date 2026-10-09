/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/FloatingPointDigits.h>

static_assert(!AK::HostIsLittleEndian);

extern "C" int powerpc_float_digit_packing_test()
{
    return AK::Detail::read_eight_digits("12345678") == 0x3837363534333231ull
        && AK::Detail::read_eight_digits("00000001") == 0x3130303030303030ull
        && AK::Detail::read_eight_digits("99999999") == 0x3939393939393939ull;
}
