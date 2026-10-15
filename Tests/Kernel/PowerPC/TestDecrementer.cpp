/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/LegacyPIC.h>
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
    u32 original_sprg2;
    asm volatile("mfmsr %0; mfspr %1,272; mfspr %2,274" : "=r"(original_msr), "=r"(original_sprg0), "=r"(original_sprg2));
    if (original_msr & (0x4000u | 0x40u | 0x30u))
        return 0;

    // The boot probe has no device interrupt dispatcher yet. Mask the 8259s
    // while enabling EE for the CPU-local decrementer, then restore them.
    Pegasos2::LegacyPIC pic { reinterpret_cast<volatile u8*>(PEGASOS2_PCI_IO_PHYSICAL_BASE),
        reinterpret_cast<volatile u8*>(PEGASOS2_INTACK_PHYSICAL_BASE + PEGASOS2_INTACK_OFFSET) };
    auto master_mask = pic.master_mask();
    auto slave_mask = pic.slave_mask();
    pic.set_masks(0xff, 0xff);
    bool masks_set = pic.master_mask() == 0xff && pic.slave_mask() == 0xff;
    if (!masks_set) {
        pic.set_masks(master_mask, slave_mask);
        return 0;
    }

    u32 zero = 0;
    asm volatile("mtspr 272,%0; mtspr 274,%1" : : "r"(zero), "r"(interval));
    auto start = read_timebase();
    set_decrementer(interval);
    asm volatile("mtmsr %0; isync" : : "r"(original_msr | 0x8000u) : "memory");

    u32 interrupts = 0;
    do {
        asm volatile("mfspr %0,272" : "=r"(interrupts));
    } while (interrupts < 2 && read_timebase() - start < static_cast<u64>(timebase_frequency) / 2);

    auto elapsed = read_timebase() - start;
    asm volatile("mtmsr %0; isync" : : "r"(original_msr) : "memory");
    set_decrementer(0x7fffffff);
    asm volatile("mtspr 272,%0; mtspr 274,%1" : : "r"(original_sprg0), "r"(original_sprg2));
    pic.set_masks(master_mask, slave_mask);
    auto elapsed_nanoseconds = timebase_ticks_to_nanoseconds(elapsed, timebase_frequency);
    return interrupts >= 2 && elapsed >= static_cast<u64>(interval) * 2
        && elapsed_nanoseconds >= 18'000'000 && elapsed_nanoseconds < 500'000'000;
}
