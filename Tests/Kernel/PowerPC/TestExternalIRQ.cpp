/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/ExceptionState.h>
#include <Kernel/Arch/powerpc/Pegasos2/LegacyPIC.h>
#include <Kernel/Arch/powerpc/Time/Timebase.h>

extern "C" int powerpc_install_external_interrupt_probe_vector();

static volatile u32 s_interrupt_count;
static volatile u32 s_irq;
static volatile u32 s_saved_srr0;
static volatile u32 s_saved_srr1;
static volatile u32 s_saved_stack;

extern "C" void powerpc_external_irq_probe_dispatch(PowerPCExceptionState const* state)
{
    using Kernel::PowerPC::Pegasos2::LegacyPIC;
    LegacyPIC pic { reinterpret_cast<volatile u8*>(LegacyPIC::io_physical_base),
        reinterpret_cast<volatile u8*>(LegacyPIC::intack_physical_base + LegacyPIC::intack_offset) };
    auto irq = pic.acknowledge();
    if (irq.has_value()) {
        s_irq = irq.value();
        s_saved_srr0 = state->srr0;
        s_saved_srr1 = state->srr1;
        s_saved_stack = state->gpr[1];
        s_interrupt_count = s_interrupt_count + 1;
    }
    pic.set_masks(0xff, 0xff);
}

static int run_external_irq_roundtrip(u32 timebase_frequency, u32 expected_translation)
{
    using Kernel::PowerPC::Pegasos2::LegacyPIC;
    using Kernel::PowerPC::read_timebase;

    u32 original_msr;
    asm volatile("mfmsr %0" : "=r"(original_msr));
    if ((original_msr & (0x8000u | 0x4000u)) || (original_msr & 0x30u) != expected_translation || !timebase_frequency
        || !powerpc_install_external_interrupt_probe_vector())
        return 0;

    LegacyPIC pic { reinterpret_cast<volatile u8*>(LegacyPIC::io_physical_base),
        reinterpret_cast<volatile u8*>(LegacyPIC::intack_physical_base + LegacyPIC::intack_offset) };
    pic.initialize();
    s_interrupt_count = 0;
    s_irq = 0xff;
    s_saved_srr0 = 0;
    s_saved_srr1 = 0;
    s_saved_stack = 0;

    pic.write_port(0x43, 0x34);
    pic.write_port(0x40, 0xa9);
    pic.write_port(0x40, 0x04);
    if (!pic.enable(0))
        return 0;

    register u32 guard_r14 asm("r14") = 0x13579bdf;
    register u32 guard_r15 asm("r15") = 0x2468ace0;
    asm volatile("" : "+r"(guard_r14), "+r"(guard_r15));
    auto start = read_timebase();
    asm volatile("mtmsr %0; isync" : : "r"(original_msr | 0x8000u) : "memory");
    while (!s_interrupt_count && read_timebase() - start < static_cast<u64>(timebase_frequency) / 2) { }
    asm volatile("mtmsr %0; isync" : : "r"(original_msr) : "memory");
    pic.set_masks(0xff, 0xff);
    asm volatile("" : "+r"(guard_r14), "+r"(guard_r15));

    u32 current_stack;
    asm volatile("mr %0,1" : "=r"(current_stack));
    return s_interrupt_count == 1 && s_irq == 0
        && guard_r14 == 0x13579bdf && guard_r15 == 0x2468ace0
        && (s_saved_srr1 & 0x8000u) != 0 && (s_saved_srr1 & 0x30u) == expected_translation
        && s_saved_srr0 >= 0x200000u && s_saved_srr0 < 0x1000000u
        && s_saved_stack >= current_stack && s_saved_stack - current_stack < 512
        && (expected_translation == 0 || (s_saved_stack >= 0xc0000000u && s_saved_stack < 0xc1000000u));
}

extern "C" int powerpc_external_irq_roundtrip(u32 timebase_frequency)
{
    return run_external_irq_roundtrip(timebase_frequency, 0);
}

extern "C" int powerpc_external_irq_high_stack_body(u32 timebase_frequency)
{
    return run_external_irq_roundtrip(timebase_frequency, 0x30u);
}
