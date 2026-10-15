/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/LegacyPIC.h>
#include <Kernel/Arch/powerpc/Time/Timebase.h>

extern "C" int powerpc_legacy_pic_irq0_roundtrip(unsigned int timebase_frequency)
{
    using Kernel::PowerPC::Pegasos2::LegacyPIC;
    using Kernel::PowerPC::read_timebase;

    u32 msr;
    asm volatile("mfmsr %0" : "=r"(msr));
    if (msr & (0x8000u | 0x4000u | 0x30u))
        return 0;

    LegacyPIC pic { reinterpret_cast<volatile u8*>(PEGASOS2_PCI_IO_PHYSICAL_BASE),
        reinterpret_cast<volatile u8*>(PEGASOS2_INTACK_PHYSICAL_BASE + PEGASOS2_INTACK_OFFSET) };
    pic.initialize();
    if (pic.master_mask() != 0xff || pic.slave_mask() != 0xff
        || pic.enable(2) || pic.enable(16))
        return 0;

    // Route a short channel-0 PIT interval through the legacy PIC while EE
    // stays disabled. The intack window performs the actual PIC acknowledge.
    pic.write_port(0x43, 0x34);
    pic.write_port(0x40, 0xa9);
    pic.write_port(0x40, 0x04);
    if (!pic.enable(0) || pic.master_mask() != 0xfe)
        return 0;

    pic.write_port(0x20, 0x0a); // Poll the interrupt request register.
    auto start = read_timebase();
    while ((pic.read_port(0x20) & 1u) == 0
        && read_timebase() - start < static_cast<u64>(timebase_frequency) / 2) { }
    pic.write_port(0x20, 0x0b); // The acknowledge path reads the ISR.
    auto irq = pic.acknowledge();
    bool passed = irq.has_value() && irq.value() == 0 && !pic.is_level_triggered(0)
        && (pic.read_port(0x20) & 1u) == 0;

    passed &= pic.disable(0) && pic.master_mask() == 0xff;
    passed &= pic.enable(10) && pic.master_mask() == 0xfb && pic.slave_mask() == 0xfb;
    passed &= pic.disable(10) && pic.master_mask() == 0xff && pic.slave_mask() == 0xff;
    return passed;
}
