/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/powerpc/KernelContext.h>
#include <Kernel/Arch/powerpc/PageDirectory.h>
#include <Kernel/Tasks/Process.h>
#include <Kernel/Tasks/Thread.h>

namespace Kernel {

void ProcessorBase::switch_context(Thread*& from_thread, Thread*& to_thread)
{
    VERIFY(!m_in_irq);
    VERIFY(m_in_critical == 1);
    VERIFY(from_thread == Thread::current());
    VERIFY(from_thread != to_thread);

    from_thread->save_critical(m_in_critical);
    auto* actual_from_thread = powerpc_switch_kernel_context(from_thread->regs(), to_thread->regs(), from_thread, to_thread);

    // A saved scheduler frame can resume after a different thread switched
    // into it. Return the actual pair to Scheduler::context_switch().
    from_thread = actual_from_thread;
    to_thread = Thread::current();
}

extern "C" void powerpc_enter_thread_context(Thread* from_thread, Thread* to_thread)
{
    VERIFY(from_thread == to_thread || from_thread->state() != Thread::State::Running);
    VERIFY(to_thread->state() == Thread::State::Running);
    VERIFY(!Processor::are_interrupts_enabled());

    Processor::set_current_thread(*to_thread);
    Processor::store_fpu_state(from_thread->fpu_state());

    auto const* from_directory = from_thread->regs().page_directory;
    auto const* to_directory = to_thread->regs().page_directory;
    VERIFY(from_directory);
    VERIFY(to_directory);
    if (from_directory != to_directory) {
        if (to_thread->process().is_kernel_process())
            Memory::activate_kernel_page_directory(*to_directory);
        else
            Memory::activate_page_directory(*to_directory, to_thread);
    }

    to_thread->set_cpu(Processor::current().id());

    auto in_critical = to_thread->saved_critical();
    VERIFY(in_critical > 0);
    Processor::restore_critical(in_critical);

    Processor::load_fpu_state(to_thread->fpu_state());
}

}
