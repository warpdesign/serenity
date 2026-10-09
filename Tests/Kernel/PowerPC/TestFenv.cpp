/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/BitCast.h>
#include <AK/Types.h>
#include <fenv.h>

[[gnu::noinline]] static u64 rounded_sum(volatile double& left, volatile double& right)
{
    volatile double result = left + right;
    double snapshot = result;
    return bit_cast<u64>(snapshot);
}

extern "C" int powerpc_fenv_test()
{
    fenv_t original {};
    if (fegetenv(&original) != 0)
        return 0;

    bool passed = fesetenv(FE_DFL_ENV) == 0 && fegetround() == FE_TONEAREST;
    volatile double one = 1.0;
    volatile double half_ulp = 0x1p-53;
    passed &= rounded_sum(one, half_ulp) == 0x3ff0000000000000ull;
    if (!passed) {
        fesetenv(&original);
        return 2;
    }

    passed &= fesetround(FE_UPWARD) == 0 && fegetround() == FE_UPWARD;
    passed &= rounded_sum(one, half_ulp) == 0x3ff0000000000001ull;
    if (!passed) {
        fesetenv(&original);
        return 3;
    }

    fenv_t upward_env {};
    passed &= fegetenv(&upward_env) == 0;
    passed &= fesetround(FE_DOWNWARD) == 0 && fegetround() == FE_DOWNWARD;
    passed &= rounded_sum(one, half_ulp) == 0x3ff0000000000000ull;
    passed &= fesetround(99) != 0 && fegetround() == FE_DOWNWARD;
    passed &= fesetenv(&upward_env) == 0 && fegetround() == FE_UPWARD;
    if (!passed) {
        fesetenv(&original);
        return 4;
    }

    passed &= feclearexcept(FE_ALL_EXCEPT) == 0 && fetestexcept(FE_ALL_EXCEPT) == 0;
    volatile double zero = 0.0;
    volatile double quotient = one / zero;
    (void)quotient;
    passed &= (fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) != 0;
    if (!passed) {
        fesetenv(&original);
        return 5;
    }

    fenv_t held_env {};
    passed &= feholdexcept(&held_env) == 0 && fetestexcept(FE_ALL_EXCEPT) == 0;
    passed &= fesetenv(&held_env) == 0 && (fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) != 0;
    passed &= feclearexcept(FE_DIVBYZERO) == 0 && fetestexcept(FE_DIVBYZERO) == 0;
    if (!passed) {
        fesetenv(&original);
        return 6;
    }
    passed &= feraiseexcept(FE_INVALID) == 0 && (fetestexcept(FE_INVALID) & FE_INVALID) != 0;
    fenv_t invalid_env {};
    passed &= fegetenv(&invalid_env) == 0 && (invalid_env.fpscr & 0x01f80700u) == 0x00000400u;

    passed &= feclearexcept(FE_ALL_EXCEPT) == 0;
    passed &= feraiseexcept(FE_DIVBYZERO) == 0;
    fexcept_t invalid_flag = FE_INVALID;
    passed &= fesetexceptflag(&invalid_flag, FE_INVALID) == 0;
    passed &= fegetenv(&invalid_env) == 0 && (invalid_env.fpscr & 0x01f80700u) == 0x00000400u;
    fexcept_t observed_flags = 0;
    passed &= fegetexceptflag(&observed_flags, FE_INVALID | FE_DIVBYZERO) == 0
        && observed_flags == (FE_INVALID | FE_DIVBYZERO);
    passed &= feclearexcept(FE_INVALID) == 0 && fetestexcept(FE_INVALID) == 0
        && (fetestexcept(FE_DIVBYZERO) & FE_DIVBYZERO) != 0;

    fesetenv(&original);
    return passed ? 1 : 7;
}
