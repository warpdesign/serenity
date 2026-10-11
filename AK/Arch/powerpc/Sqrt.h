/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/BitCast.h>
#include <AK/Concepts.h>
#include <AK/Types.h>

namespace AK::PowerPC {

struct IntegerSquareRoot {
    u64 root;
    u64 remainder;
};

// Compute the floor of the square root without 128-bit arithmetic. Each step
// consumes two radicand bits; the remainder fits in 56 bits for binary64.
template<unsigned SignificandBits>
ALWAYS_INLINE IntegerSquareRoot integer_square_root(u64 high, u64 low)
{
    u64 root = 0;
    u64 remainder = 0;
    for (int pair_index = SignificandBits - 1; pair_index >= 0; --pair_index) {
        u64 pair = pair_index >= 32
            ? (high >> (2 * (pair_index - 32))) & 3
            : (low >> (2 * pair_index)) & 3;
        remainder = (remainder << 2) | pair;
        u64 trial = (root << 2) | 1;
        root <<= 1;
        if (remainder >= trial) {
            remainder -= trial;
            ++root;
        }
    }
    return { root, remainder };
}

ALWAYS_INLINE unsigned rounding_mode()
{
#if ARCH(POWERPC32)
    double fpscr;
    asm volatile("mffs %0" : "=f"(fpscr));
    return bit_cast<u64>(fpscr) & 3;
#else
    return 0;
#endif
}

ALWAYS_INLINE bool should_round_up(u64 root, u64 remainder)
{
    if (remainder == 0)
        return false;
    switch (rounding_mode()) {
    case 0: // Round to nearest.
        return remainder > root;
    case 2: // Round toward positive infinity.
        return true;
    default: // Round toward zero or negative infinity.
        return false;
    }
}

ALWAYS_INLINE double sqrt_double(double value)
{
    u64 bits = bit_cast<u64>(value);
    u64 fraction = bits & ((1ull << 52) - 1);
    unsigned biased_exponent = (bits >> 52) & 0x7ff;
    if ((bits << 1) == 0)
        return value; // Preserve both signs of zero.
    if (biased_exponent == 0x7ff && fraction != 0)
        return value + value; // Preserve and quiet NaNs.
    if (bits >> 63) {
        volatile double zero = 0.0;
        return zero / zero;
    }
    if (biased_exponent == 0x7ff)
        return value;

    int exponent;
    u64 significand;
    if (biased_exponent == 0) {
        unsigned normalization_shift = __builtin_clzll(fraction) - 11;
        significand = fraction << normalization_shift;
        exponent = -1022 - static_cast<int>(normalization_shift);
    } else {
        significand = (1ull << 52) | fraction;
        exponent = static_cast<int>(biased_exponent) - 1023;
    }

    int result_exponent = exponent / 2;
    if (exponent < 0 && (exponent & 1))
        --result_exponent;
    unsigned parity = exponent - 2 * result_exponent;
    unsigned shift = 52 + parity;
    auto [root, remainder] = integer_square_root<53>(significand >> (64 - shift), significand << shift);
    if (should_round_up(root, remainder))
        ++root;
    if (root == (1ull << 53)) {
        root >>= 1;
        ++result_exponent;
    }
    return bit_cast<double>((static_cast<u64>(result_exponent + 1023) << 52) | (root & ((1ull << 52) - 1)));
}

ALWAYS_INLINE float sqrt_float(float value)
{
    u32 bits = bit_cast<u32>(value);
    u32 fraction = bits & ((1u << 23) - 1);
    unsigned biased_exponent = (bits >> 23) & 0xff;
    if ((bits << 1) == 0)
        return value;
    if (biased_exponent == 0xff && fraction != 0)
        return value + value;
    if (bits >> 31) {
        volatile float zero = 0.0f;
        return zero / zero;
    }
    if (biased_exponent == 0xff)
        return value;

    int exponent;
    u32 significand;
    if (biased_exponent == 0) {
        unsigned normalization_shift = __builtin_clz(fraction) - 8;
        significand = fraction << normalization_shift;
        exponent = -126 - static_cast<int>(normalization_shift);
    } else {
        significand = (1u << 23) | fraction;
        exponent = static_cast<int>(biased_exponent) - 127;
    }

    int result_exponent = exponent / 2;
    if (exponent < 0 && (exponent & 1))
        --result_exponent;
    unsigned parity = exponent - 2 * result_exponent;
    u64 radicand = static_cast<u64>(significand) << (23 + parity);
    auto [root, remainder] = integer_square_root<24>(0, radicand);
    if (should_round_up(root, remainder))
        ++root;
    if (root == (1u << 24)) {
        root >>= 1;
        ++result_exponent;
    }
    return bit_cast<float>((static_cast<u32>(result_exponent + 127) << 23) | static_cast<u32>(root & ((1u << 23) - 1)));
}

template<FloatingPoint T>
ALWAYS_INLINE T sqrt(T value)
{
    if constexpr (IsSame<T, float>)
        return sqrt_float(value);
    return static_cast<T>(sqrt_double(static_cast<double>(value)));
}

}
