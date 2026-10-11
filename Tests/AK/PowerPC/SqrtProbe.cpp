/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Math/Sqrt.h>
#include <fenv.h>
#include <math.h>

extern "C" unsigned syscall2(unsigned, unsigned, unsigned);

extern "C" [[noreturn]] void _start()
{
    volatile float float_input = 9.0f;
    volatile double double_input = 25.0;
    if (AK::sqrt(float_input) != 3.0f || AK::sqrt(double_input) != 5.0)
        __builtin_trap();
    volatile double non_square = 2.0;
    volatile float float_non_square = 2.0f;
    if (AK::bit_cast<unsigned long long>(AK::sqrt(non_square)) != 0x3ff6a09e667f3bcdull
        || AK::bit_cast<unsigned>(AK::sqrt(float_non_square)) != 0x3fb504f3u)
        __builtin_trap();
    if (AK::bit_cast<unsigned long long>(AK::sqrt(-0.0)) != 0x8000000000000000ull)
        __builtin_trap();
    if (fesetround(FE_DOWNWARD) != 0
        || AK::bit_cast<unsigned long long>(AK::sqrt(non_square)) != 0x3ff6a09e667f3bccull
        || AK::bit_cast<unsigned>(AK::sqrt(float_non_square)) != 0x3fb504f3u)
        __builtin_trap();
    if (fesetround(FE_UPWARD) != 0
        || AK::bit_cast<unsigned long long>(AK::sqrt(non_square)) != 0x3ff6a09e667f3bcdull
        || AK::bit_cast<unsigned>(AK::sqrt(float_non_square)) != 0x3fb504f4u)
        __builtin_trap();
    if (fesetround(FE_TONEAREST) != 0)
        __builtin_trap();
    double (*volatile libc_sqrt)(double) = sqrt;
    float (*volatile libc_sqrtf)(float) = sqrtf;
    if (libc_sqrt(double_input) != 5.0 || libc_sqrtf(float_input) != 3.0f
        || AK::bit_cast<unsigned long long>(libc_sqrt(non_square)) != 0x3ff6a09e667f3bcdull)
        __builtin_trap();

    constexpr char message[] = "PowerPC AK sqrt runtime passed\n";
    if (syscall2(SYSCALL_DBGPUTSTR, reinterpret_cast<unsigned>(message), sizeof(message) - 1) != sizeof(message) - 1)
        __builtin_trap();
    for (;;)
        asm volatile("" ::: "memory");
}
