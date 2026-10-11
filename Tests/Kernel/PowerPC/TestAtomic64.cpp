/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>
#include <Kernel/Arch/powerpc/CPU.h>

extern "C" int powerpc_atomic64_test()
{
    alignas(8) volatile u64 value = 0x00000001ffffffffull;
    auto previous_msr = Kernel::PowerPC::read_msr();

    if (__atomic_load_n(&value, __ATOMIC_SEQ_CST) != 0x00000001ffffffffull)
        return 0;
    if (__atomic_fetch_add(&value, 2ull, __ATOMIC_SEQ_CST) != 0x00000001ffffffffull
        || __atomic_load_n(&value, __ATOMIC_RELAXED) != 0x0000000200000001ull)
        return 0;
    if (__atomic_fetch_add(&value, 0xffffffffffffffffull, __ATOMIC_RELAXED) != 0x0000000200000001ull
        || __atomic_load_n(&value, __ATOMIC_SEQ_CST) != 0x0000000200000000ull)
        return 0;

    auto const* bytes = reinterpret_cast<volatile u8 const*>(&value);
    return bytes[0] == 0x00 && bytes[1] == 0x00 && bytes[2] == 0x00 && bytes[3] == 0x02
        && bytes[4] == 0x00 && bytes[5] == 0x00 && bytes[6] == 0x00 && bytes[7] == 0x00
        && Kernel::PowerPC::read_msr() == previous_msr;
}
