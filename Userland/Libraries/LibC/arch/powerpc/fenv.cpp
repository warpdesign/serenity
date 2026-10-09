/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/BitCast.h>
#include <AK/Types.h>
#include <fenv.h>

static_assert(sizeof(fenv_t) == sizeof(u32));

namespace {

constexpr u32 fpscr_fx = 0x80000000;
constexpr u32 fpscr_fex = 0x40000000;
constexpr u32 fpscr_vx = 0x20000000;
constexpr u32 fpscr_ox = 0x10000000;
constexpr u32 fpscr_ux = 0x08000000;
constexpr u32 fpscr_zx = 0x04000000;
constexpr u32 fpscr_xx = 0x02000000;
constexpr u32 fpscr_invalid_causes = 0x01f80700;
constexpr u32 fpscr_vxsoft = 0x00000400;
constexpr u32 fpscr_ve = 0x00000080;
constexpr u32 fpscr_oe = 0x00000040;
constexpr u32 fpscr_ue = 0x00000020;
constexpr u32 fpscr_ze = 0x00000010;
constexpr u32 fpscr_xe = 0x00000008;
constexpr u32 fpscr_enable_mask = fpscr_ve | fpscr_oe | fpscr_ue | fpscr_ze | fpscr_xe;
constexpr u32 fpscr_status_mask = fpscr_fx | fpscr_fex | fpscr_vx | fpscr_ox | fpscr_ux | fpscr_zx | fpscr_xx | fpscr_invalid_causes;
constexpr u32 fpscr_rounding_mask = 3;

u32 read_fpscr()
{
    double bits;
    asm volatile("mffs %0" : "=f"(bits));
    return static_cast<u32>(bit_cast<u64>(bits));
}

void write_fpscr(u32 fpscr)
{
    double bits = bit_cast<double>(static_cast<u64>(fpscr));
    asm volatile("mtfsf 255,%0" : : "f"(bits) : "memory");
}

u32 raw_exception_bits(int exceptions)
{
    u32 mask = 0;
    if (exceptions & FE_INVALID)
        mask |= fpscr_vx | fpscr_vxsoft;
    if (exceptions & FE_DIVBYZERO)
        mask |= fpscr_zx;
    if (exceptions & FE_OVERFLOW)
        mask |= fpscr_ox;
    if (exceptions & FE_UNDERFLOW)
        mask |= fpscr_ux;
    if (exceptions & FE_INEXACT)
        mask |= fpscr_xx;
    return mask;
}

u32 raw_exception_clear_mask(int exceptions)
{
    u32 mask = raw_exception_bits(exceptions);
    if (exceptions & FE_INVALID)
        mask |= fpscr_invalid_causes;
    return mask;
}

void refresh_exception_summaries(u32& fpscr)
{
    if (fpscr & fpscr_invalid_causes)
        fpscr |= fpscr_vx;
    else
        fpscr &= ~fpscr_vx;

    if (fpscr & (fpscr_vx | fpscr_ox | fpscr_ux | fpscr_zx | fpscr_xx))
        fpscr |= fpscr_fx;
    else
        fpscr &= ~fpscr_fx;

    if (((fpscr & fpscr_vx) && (fpscr & fpscr_ve))
        || ((fpscr & fpscr_ox) && (fpscr & fpscr_oe))
        || ((fpscr & fpscr_ux) && (fpscr & fpscr_ue))
        || ((fpscr & fpscr_zx) && (fpscr & fpscr_ze))
        || ((fpscr & fpscr_xx) && (fpscr & fpscr_xe)))
        fpscr |= fpscr_fex;
    else
        fpscr &= ~fpscr_fex;
}

}

extern "C" {

int fegetenv(fenv_t* env)
{
    if (!env)
        return 1;
    env->fpscr = read_fpscr();
    return 0;
}

int fesetenv(fenv_t const* env)
{
    if (!env)
        return 1;
    write_fpscr(env == FE_DFL_ENV ? 0 : env->fpscr);
    return 0;
}

int feholdexcept(fenv_t* env)
{
    if (fegetenv(env) != 0)
        return 1;
    write_fpscr(env->fpscr & ~(fpscr_status_mask | fpscr_enable_mask));
    return 0;
}

int fesetexceptflag(fexcept_t const* except, int exceptions)
{
    if (!except)
        return 1;
    exceptions &= FE_ALL_EXCEPT;
    u32 fpscr = read_fpscr();
    fpscr &= ~raw_exception_clear_mask(exceptions);
    int flags_to_set = *except & exceptions;
    fpscr |= raw_exception_bits(flags_to_set);
    refresh_exception_summaries(fpscr);
    write_fpscr(fpscr);
    return 0;
}

int fegetround()
{
    switch (read_fpscr() & fpscr_rounding_mask) {
    case 0:
        return FE_TONEAREST;
    case 1:
        return FE_TOWARDZERO;
    case 2:
        return FE_UPWARD;
    case 3:
        return FE_DOWNWARD;
    }
    __builtin_unreachable();
}

int fesetround(int rounding_mode)
{
    u32 raw_rounding_mode;
    switch (rounding_mode) {
    case FE_TONEAREST:
        raw_rounding_mode = 0;
        break;
    case FE_TOWARDZERO:
        raw_rounding_mode = 1;
        break;
    case FE_UPWARD:
        raw_rounding_mode = 2;
        break;
    case FE_DOWNWARD:
        raw_rounding_mode = 3;
        break;
    default:
        return 1;
    }
    write_fpscr((read_fpscr() & ~fpscr_rounding_mask) | raw_rounding_mode);
    return 0;
}

int feclearexcept(int exceptions)
{
    u32 fpscr = read_fpscr();
    fpscr &= ~raw_exception_clear_mask(exceptions & FE_ALL_EXCEPT);
    refresh_exception_summaries(fpscr);
    write_fpscr(fpscr);
    return 0;
}

int fetestexcept(int exceptions)
{
    u32 fpscr = read_fpscr();
    int flags = 0;
    if (fpscr & (fpscr_vx | fpscr_invalid_causes))
        flags |= FE_INVALID;
    if (fpscr & fpscr_zx)
        flags |= FE_DIVBYZERO;
    if (fpscr & fpscr_ox)
        flags |= FE_OVERFLOW;
    if (fpscr & fpscr_ux)
        flags |= FE_UNDERFLOW;
    if (fpscr & fpscr_xx)
        flags |= FE_INEXACT;
    return flags & exceptions & FE_ALL_EXCEPT;
}

int feraiseexcept(int exceptions)
{
    exceptions &= FE_ALL_EXCEPT;
    u32 fpscr = read_fpscr();
    fpscr |= raw_exception_bits(exceptions);
    refresh_exception_summaries(fpscr);
    write_fpscr(fpscr);
    return 0;
}

}
