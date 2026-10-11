/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>

extern "C" int powerpc_processor_test()
{
    if (Kernel::Processor::is_initialized())
        return 0;

    Kernel::Processor processor {};
    processor.early_initialize(0);
    bool passed = Kernel::Processor::is_initialized() && &Kernel::Processor::current() == &processor
        && Kernel::Processor::current_id() == 0
        && processor.has_feature(Kernel::CPUFeature::FPU)
        && processor.has_feature(Kernel::CPUFeature::HashedMMU)
        && Kernel::Processor::platform_string() == "powerpc"sv
        && !processor.has_nx()
        && Kernel::Processor::in_critical() == 0;

    Kernel::Processor::enter_critical();
    passed &= Kernel::Processor::in_critical() == 1;
    Kernel::Processor::restore_critical(0);
    passed &= Kernel::Processor::in_critical() == 0;

    auto before = Kernel::Processor::read_cycle_count();
    for (unsigned index = 0; index < 10000; ++index)
        Kernel::Processor::pause();
    auto after = Kernel::Processor::read_cycle_count();
    passed &= before.has_value() && after.has_value() && after.value() > before.value();

    auto msr = Kernel::PowerPC::read_msr();
    passed &= Kernel::Processor::are_interrupts_enabled() == ((msr & 0x8000u) != 0);
    Kernel::Processor::disable_interrupts();
    passed &= !Kernel::Processor::are_interrupts_enabled();
    Kernel::PowerPC::write_msr(msr);
    passed &= Kernel::Processor::are_interrupts_enabled() == ((msr & 0x8000u) != 0);
    Kernel::g_current_processor = nullptr;
    return passed;
}
