/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Time/Timebase.h>

extern "C" int powerpc_install_decrementer_probe_vector();

extern "C" int powerpc_decrementer_exception_roundtrip(unsigned int timebase_frequency)
{
    using namespace Kernel::PowerPC;
    auto interval = decrementer_interval(timebase_frequency, 100);
    if (!interval || !powerpc_install_decrementer_probe_vector())
        return 0;

    u32 original_msr;
    u32 original_sprg0;
    asm volatile("mfmsr %0; mfspr %1,272" : "=r"(original_msr), "=r"(original_sprg0));
    if (original_msr & (0x4000u | 0x40u | 0x30u))
        return 0;

    u32 zero = 0;
    asm volatile("mtspr 272,%0" : : "r"(zero));
    auto start = read_timebase();
    set_decrementer(interval);
    asm volatile("mtmsr %0; isync" : : "r"(original_msr | 0x8000u) : "memory");

    u32 interrupts = 0;
    do {
        asm volatile("mfspr %0,272" : "=r"(interrupts));
    } while (!interrupts && read_timebase() - start < static_cast<u64>(timebase_frequency) / 2);

    auto elapsed = read_timebase() - start;
    asm volatile("mtmsr %0; isync" : : "r"(original_msr) : "memory");
    set_decrementer(0x7fffffff);
    asm volatile("mtspr 272,%0" : : "r"(original_sprg0));
    return interrupts == 1 && elapsed >= interval && elapsed < static_cast<u64>(timebase_frequency) / 2;
}
