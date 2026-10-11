/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Arch/powerpc/Sqrt.h>
#include <AK/BitCast.h>
#include <LibTest/Macros.h>
#include <LibTest/TestCase.h>
#include <cmath>

TEST_CASE(powerpc_sqrt_special_values)
{
    EXPECT_EQ(AK::bit_cast<u64>(AK::PowerPC::sqrt_double(-0.0)), 0x8000000000000000ull);
    EXPECT_EQ(AK::bit_cast<u32>(AK::PowerPC::sqrt_float(-0.0f)), 0x80000000u);
    EXPECT_EQ(AK::bit_cast<u64>(AK::PowerPC::sqrt_double(2.0)), 0x3ff6a09e667f3bcdull);
    EXPECT_EQ(AK::bit_cast<u32>(AK::PowerPC::sqrt_float(2.0f)), 0x3fb504f3u);
    EXPECT_EQ(AK::bit_cast<u64>(AK::PowerPC::sqrt_double(AK::bit_cast<double>(1ull))), 0x1e60000000000000ull);
    EXPECT(std::isnan(AK::PowerPC::sqrt_double(-1.0)));
    EXPECT_EQ(AK::PowerPC::sqrt_double(INFINITY), INFINITY);
}

TEST_CASE(powerpc_sqrt_matches_host)
{
    u64 state = 0x12345678abcdefull;
    for (int i = 0; i < 100'000; ++i) {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;
        auto double_input = AK::bit_cast<double>(state & 0x7fffffffffffffffull);
        if (std::isfinite(double_input))
            EXPECT_EQ(AK::bit_cast<u64>(AK::PowerPC::sqrt_double(double_input)), AK::bit_cast<u64>(std::sqrt(double_input)));

        auto float_input = AK::bit_cast<float>(static_cast<u32>(state) & 0x7fffffffu);
        if (std::isfinite(float_input))
            EXPECT_EQ(AK::bit_cast<u32>(AK::PowerPC::sqrt_float(float_input)), AK::bit_cast<u32>(std::sqrt(float_input)));
    }
}
