/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/TrapFrame.h>
#include <Kernel/Arch/powerpc/InitialThreadContext.h>
#include <Kernel/Arch/powerpc/KernelContext.h>
#include <Kernel/Arch/powerpc/PageDirectory.h>
#include <Kernel/Arch/powerpc/RegisterState.h>
#include <Kernel/Interrupts/InterruptDisabler.h>
#include <Kernel/Security/Random.h>
#include <Kernel/Tasks/Process.h>
#include <Kernel/Tasks/Scheduler.h>
#include <Kernel/Tasks/Thread.h>

namespace Kernel {

extern "C" void context_first_init(Thread* from_thread, Thread* to_thread);
extern "C" void context_first_init(Thread* from_thread, Thread* to_thread)
{
    do_context_first_init(from_thread, to_thread);
}

extern "C" [[noreturn]] void powerpc_finish_first_thread_context(TrapFrame* trap);
extern "C" [[noreturn]] void powerpc_finish_first_thread_context(TrapFrame* trap)
{
    VERIFY(trap);
    VERIFY(trap->regs);
    VERIFY_INTERRUPTS_DISABLED();

    auto* registers = trap->regs;
    // User return also has to replace the high kernel VSIDs in real mode.
    VERIFY(registers->previous_mode() == ExecutionMode::Kernel);
    Processor::current().exit_trap(*trap);
    powerpc_restore_supervisor_exception_state(*registers);
}

FlatPtr ProcessorBase::init_context(Thread& thread, bool leave_crit)
{
    VERIFY(g_scheduler_lock.is_locked());
    if (leave_crit) {
        VERIFY(in_critical() == 2);
        m_in_critical = 1;
    }

    auto stack_top = thread.kernel_stack_top();
    stack_top -= round_up_to_power_of_two(get_fast_random<u8>(), 16);
    return prepare_initial_thread_context(thread.regs(), stack_top, thread.current_trap());
}

[[noreturn]] void ProcessorBase::initialize_context_switching(Thread& initial_thread)
{
    VERIFY(initial_thread.process().is_kernel_process());
    m_scheduler_initialized.set();
    Processor::set_current_in_scheduler(true);
    powerpc_start_initial_context(initial_thread.regs(), &initial_thread);
}

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
