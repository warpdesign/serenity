/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Platform.h>
#include <Kernel/Arch/CPU.h>
#include <Kernel/Arch/powerpc/Time/Timebase.h>

VALIDATE_IS_POWERPC32()

namespace Kernel {

// Pegasos 2 bring-up currently runs one processor. The CPU ID and scheduler
// bookkeeping can become per-CPU when secondary processors are supported.
extern class Processor* g_current_processor;
constexpr size_t MAX_CPU_COUNT = 1;

class Processor final : public ProcessorBase {
public:
    template<IteratorFunction<Processor&> Callback>
    static inline IterationDecision for_each(Callback callback)
    {
        return callback(*g_current_processor);
    }

    template<VoidFunction<Processor&> Callback>
    static inline IterationDecision for_each(Callback callback)
    {
        callback(*g_current_processor);
        return IterationDecision::Continue;
    }
};

ALWAYS_INLINE bool ProcessorBase::is_initialized()
{
    return g_current_processor != nullptr;
}

ALWAYS_INLINE Processor& ProcessorBase::current()
{
    return *g_current_processor;
}

ALWAYS_INLINE u32 ProcessorBase::current_id()
{
    return 0;
}

ALWAYS_INLINE Thread* ProcessorBase::current_thread()
{
    return current().m_current_thread;
}

ALWAYS_INLINE Thread* ProcessorBase::idle_thread()
{
    return current().m_idle_thread;
}

ALWAYS_INLINE void ProcessorBase::set_current_thread(Thread& thread)
{
    current().m_current_thread = &thread;
}

ALWAYS_INLINE u32 ProcessorBase::in_critical()
{
    return current().m_in_critical;
}

ALWAYS_INLINE void ProcessorBase::enter_critical()
{
    auto& processor = current();
    processor.m_in_critical = processor.m_in_critical + 1;
}

ALWAYS_INLINE void ProcessorBase::restore_critical(u32 previous)
{
    current().m_in_critical = previous;
}

ALWAYS_INLINE bool ProcessorBase::are_interrupts_enabled()
{
    return (PowerPC::read_msr() & 0x8000u) != 0;
}

ALWAYS_INLINE void ProcessorBase::enable_interrupts()
{
    PowerPC::write_msr(PowerPC::read_msr() | 0x8000u);
}

ALWAYS_INLINE void ProcessorBase::disable_interrupts()
{
    PowerPC::write_msr(PowerPC::read_msr() & ~0x8000u);
}

ALWAYS_INLINE FlatPtr ProcessorBase::current_in_irq()
{
    return current().m_in_irq;
}

ALWAYS_INLINE bool ProcessorBase::current_in_scheduler()
{
    return current().m_in_scheduler != 0;
}

ALWAYS_INLINE void ProcessorBase::set_current_in_scheduler(bool value)
{
    current().m_in_scheduler = value;
}

ALWAYS_INLINE bool ProcessorBase::has_self_snooping() const
{
    return false;
}

ALWAYS_INLINE bool ProcessorBase::has_nx() const
{
    return false;
}

ALWAYS_INLINE void ProcessorBase::pause()
{
    asm volatile("nop" : : : "memory");
}

ALWAYS_INLINE void ProcessorBase::wait_check()
{
    pause();
}

ALWAYS_INLINE Optional<u64> ProcessorBase::read_cycle_count()
{
    return PowerPC::read_timebase();
}

}
