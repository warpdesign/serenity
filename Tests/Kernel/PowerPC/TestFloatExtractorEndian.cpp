/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Endian.h>
#include <AK/FloatingPoint.h>

static_assert(!AK::HostIsLittleEndian);
static_assert(sizeof(long double) == sizeof(double));

extern "C" int powerpc_float_extractor_endian_test()
{
    auto binary32 = AK::FloatExtractor<f32>::from_float(-1.5f);
    if (binary32.sign != 1 || binary32.exponent != 127 || binary32.mantissa != (1u << 22)
        || binary32.to_float() != -1.5f)
        return 0;

    auto binary64 = AK::FloatExtractor<f64>::from_float(-1.5);
    if (binary64.sign != 1 || binary64.exponent != 1023 || binary64.mantissa != (1ull << 51)
        || binary64.to_float() != -1.5)
        return 0;

    auto long_double = AK::FloatExtractor<long double>::from_float(1.5L);
    return long_double.sign == 0 && long_double.exponent == 1023
        && long_double.mantissa == (1ull << 51) && long_double.to_float() == 1.5L;
}
