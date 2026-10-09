/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/FloatingPointStringConversions.h>

static bool parses_as(char const* text, unsigned int length, double expected)
{
    auto value = AK::parse_floating_point_completely<double>(text, text + length);
    return value.has_value() && value.value() == expected;
}

extern "C" int powerpc_float_parser_test()
{
    constexpr char fractional[] = "12345678.12500000";
    constexpr char leading_zeroes[] = "0000000012345678";
    constexpr char all_nines[] = "99999999";
    return parses_as(fractional, sizeof(fractional) - 1, 12345678.125)
        && parses_as(leading_zeroes, sizeof(leading_zeroes) - 1, 12345678.0)
        && parses_as(all_nines, sizeof(all_nines) - 1, 99999999.0);
}
