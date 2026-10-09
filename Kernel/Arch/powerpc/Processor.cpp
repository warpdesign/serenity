/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/powerpc/CPUID.h>

namespace Kernel {

Processor* g_current_processor;

void ProcessorBase::early_initialize(u32 cpu)
{
    VERIFY(cpu == 0);
    VERIFY(g_current_processor == nullptr);

    m_self = static_cast<Processor*>(this);
    m_cpu = cpu;
    m_features = detect_cpu_features(PowerPC::read_pvr());
    VERIFY(m_features.has_flag(CPUFeature::FPU));
    VERIFY(m_features.has_flag(CPUFeature::HashedMMU));
    m_physical_address_bit_width = 32;
    m_virtual_address_bit_width = 32;
    m_in_critical = 0;
    m_in_irq = 0;
    m_in_scheduler = 0;
    m_invoke_scheduler_async = 0;
    m_current_thread = nullptr;
    m_idle_thread = nullptr;

    g_current_processor = static_cast<Processor*>(this);
}

void ProcessorBase::store_fpu_state(FPUState& state)
{
    ::Kernel::store_fpu_state(&state);
}

void ProcessorBase::load_fpu_state(FPUState const& state)
{
    ::Kernel::load_fpu_state(&state);
}

}
