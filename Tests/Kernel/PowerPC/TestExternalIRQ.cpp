/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/ExceptionState.h>
#include <Kernel/Arch/powerpc/Boot/ProbeAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/LegacyPIC.h>
#include <Kernel/Arch/powerpc/Time/Timebase.h>

extern "C" int powerpc_install_external_interrupt_probe_vector();
extern "C" volatile u32 powerpc_probe_use_high_irq_handler;
extern "C" {
volatile u32 powerpc_irq_high_debug;
volatile u32 powerpc_probe_user_irq_signal_address;
}

static volatile u32 s_interrupt_count;
static volatile u32 s_irq;
static volatile u32 s_saved_srr0;
static volatile u32 s_saved_srr1;
static volatile u32 s_saved_stack;
static volatile u32 s_dispatch_caller_pc;

extern "C" void powerpc_external_irq_probe_dispatch(PowerPCExceptionState const* state)
{
    using Kernel::PowerPC::Pegasos2::LegacyPIC;
    bool high = powerpc_probe_use_high_irq_handler != 0;
    LegacyPIC pic { reinterpret_cast<volatile u8*>(high ? POWERPC_HPT_PIC_IO_ADDRESS : PEGASOS2_PCI_IO_PHYSICAL_BASE),
        reinterpret_cast<volatile u8*>(high ? POWERPC_HPT_INTACK_ADDRESS + PEGASOS2_INTACK_OFFSET
                                             : PEGASOS2_INTACK_PHYSICAL_BASE + PEGASOS2_INTACK_OFFSET) };
    auto irq = pic.acknowledge();
    if (irq.has_value()) {
        s_dispatch_caller_pc = reinterpret_cast<u32>(__builtin_return_address(0));
        s_irq = irq.value();
        s_saved_srr0 = state->srr0;
        s_saved_srr1 = state->srr1;
        s_saved_stack = state->gpr[1];
        s_interrupt_count = s_interrupt_count + 1;
        if ((state->srr1 & 0x4000u) && powerpc_probe_user_irq_signal_address)
            *reinterpret_cast<volatile u32*>(powerpc_probe_user_irq_signal_address) = 1;
    }
    pic.set_masks(0xff, 0xff);
}

extern "C" void powerpc_irq_probe_reset()
{
    s_interrupt_count = 0;
    s_irq = 0xff;
    s_saved_srr0 = 0;
    s_saved_srr1 = 0;
    s_saved_stack = 0;
    s_dispatch_caller_pc = 0;
}

extern "C" int powerpc_irq_prepare_user_probe()
{
    using Kernel::PowerPC::Pegasos2::LegacyPIC;
    LegacyPIC pic { reinterpret_cast<volatile u8*>(PEGASOS2_PCI_IO_PHYSICAL_BASE),
        reinterpret_cast<volatile u8*>(PEGASOS2_INTACK_PHYSICAL_BASE + PEGASOS2_INTACK_OFFSET) };
    pic.initialize();
    pic.write_port(0x43, 0x34);
    pic.write_port(0x40, 0xa9);
    pic.write_port(0x40, 0x04);
    return pic.enable(0);
}

extern "C" void powerpc_irq_finish_user_probe()
{
    using Kernel::PowerPC::Pegasos2::LegacyPIC;
    LegacyPIC pic { reinterpret_cast<volatile u8*>(PEGASOS2_PCI_IO_PHYSICAL_BASE),
        reinterpret_cast<volatile u8*>(PEGASOS2_INTACK_PHYSICAL_BASE + PEGASOS2_INTACK_OFFSET) };
    pic.set_masks(0xff, 0xff);
}

extern "C" u32 powerpc_irq_probe_count()
{
    return s_interrupt_count;
}

extern "C" u32 powerpc_irq_probe_number()
{
    return s_irq;
}

extern "C" u32 powerpc_irq_probe_saved_srr0()
{
    return s_saved_srr0;
}

extern "C" u32 powerpc_irq_probe_saved_srr1()
{
    return s_saved_srr1;
}

extern "C" u32 powerpc_irq_probe_saved_stack()
{
    return s_saved_stack;
}

extern "C" u32 powerpc_irq_probe_dispatch_caller()
{
    return s_dispatch_caller_pc;
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

    LegacyPIC pic { reinterpret_cast<volatile u8*>(powerpc_probe_use_high_irq_handler ? POWERPC_HPT_PIC_IO_ADDRESS : PEGASOS2_PCI_IO_PHYSICAL_BASE),
        reinterpret_cast<volatile u8*>(PEGASOS2_INTACK_PHYSICAL_BASE + PEGASOS2_INTACK_OFFSET) };
    pic.initialize();
    powerpc_irq_probe_reset();

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
    u32 checks = 0;
    checks |= (s_interrupt_count == 1) << 0;
    checks |= (s_irq == 0) << 1;
    checks |= (guard_r14 == 0x13579bdf && guard_r15 == 0x2468ace0) << 2;
    checks |= ((s_saved_srr1 & 0x8000u) != 0) << 3;
    checks |= ((s_saved_srr1 & 0x30u) == expected_translation) << 4;
    checks |= (s_saved_srr0 >= 0x200000u && s_saved_srr0 < 0x1000000u) << 5;
    checks |= (s_saved_stack >= current_stack && s_saved_stack - current_stack < 512) << 6;
    checks |= (expected_translation == 0 || s_saved_stack >= 0xc0000000u) << 7;
    checks |= (powerpc_probe_use_high_irq_handler
            ? (s_dispatch_caller_pc >= 0xc0000000u && s_dispatch_caller_pc < 0xc1000000u)
            : (s_dispatch_caller_pc >= 0x200000u && s_dispatch_caller_pc < 0x1000000u)) << 8;
    powerpc_irq_high_debug = checks;
    return checks == 0x1ffu;
}

extern "C" int powerpc_external_irq_roundtrip(u32 timebase_frequency)
{
    return run_external_irq_roundtrip(timebase_frequency, 0);
}

extern "C" int powerpc_external_irq_high_stack_body(u32 timebase_frequency)
{
    return run_external_irq_roundtrip(timebase_frequency, 0x30u);
}
