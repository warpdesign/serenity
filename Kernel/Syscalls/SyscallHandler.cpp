/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2022-2026, the SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StdLibExtraDetails.h>
#include <Kernel/API/Syscall.h>
#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Arch/SyscallEntry.h>
#include <Kernel/Arch/TrapFrame.h>
#include <Kernel/Library/Panic.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Sections.h>
#include <Kernel/Security/Random.h>
#include <Kernel/Tasks/PerformanceManager.h>
#include <Kernel/Tasks/Process.h>
#include <Kernel/Tasks/Scheduler.h>
#include <Kernel/Tasks/ThreadTracer.h>

namespace Kernel {

extern bool g_in_system_shutdown;

namespace Syscall {

using Handler = ErrorOr<FlatPtr> (*)(Process&, RegisterState&, FlatPtr, FlatPtr, FlatPtr, FlatPtr);

// Calling a typed syscall method through a raw-integer member-function pointer
// is not ABI-safe for wrapper arguments such as Userspace<T>.
template<typename T>
static decltype(auto) decode_argument(FlatPtr value, RegisterState& regs)
{
    if constexpr (IsSame<T, RegisterState&>)
        return (regs);
    else if constexpr (IsPointer<T>)
        return reinterpret_cast<T>(value);
    else if constexpr (requires { T { value }; })
        return T { value };
    else
        return static_cast<T>(value);
}

template<typename>
struct MethodInvoker;

template<typename Result, typename... Args>
struct MethodInvoker<Result (Process::*)(Args...)> {
    template<auto method, size_t... indices>
    static ErrorOr<FlatPtr> call_impl(Process& process, RegisterState& regs, FlatPtr const* values, IndexSequence<indices...>)
    {
        if constexpr (IsSame<Result, void>) {
            (process.*method)(decode_argument<Args>(values[indices], regs)...);
            VERIFY_NOT_REACHED();
        } else {
            return (process.*method)(decode_argument<Args>(values[indices], regs)...);
        }
    }

    template<auto method>
    static ErrorOr<FlatPtr> call(Process& process, RegisterState& regs, FlatPtr arg1, FlatPtr arg2, FlatPtr arg3, FlatPtr arg4)
    {
        static_assert(sizeof...(Args) <= 4);
        FlatPtr values[] { arg1, arg2, arg3, arg4 };
        return call_impl<method>(process, regs, values, MakeIndexSequence<sizeof...(Args)> {});
    }
};

template<auto method>
static ErrorOr<FlatPtr> invoke_syscall(Process& process, RegisterState& regs, FlatPtr arg1, FlatPtr arg2, FlatPtr arg3, FlatPtr arg4)
{
    return MethodInvoker<decltype(method)>::template call<method>(process, regs, arg1, arg2, arg3, arg4);
}

struct HandlerMetadata {
    Handler handler;
    NeedsBigProcessLock needs_lock;
};

#define __ENUMERATE_SYSCALL(sys_call, needs_lock) { &invoke_syscall<&Process::sys$##sys_call>, needs_lock },
static HandlerMetadata const s_syscall_table[] = {
    ENUMERATE_SYSCALLS(__ENUMERATE_SYSCALL)
};
#undef __ENUMERATE_SYSCALL

ErrorOr<FlatPtr> handle(RegisterState& regs, FlatPtr function, FlatPtr arg1, FlatPtr arg2, FlatPtr arg3, FlatPtr arg4)
{
    VERIFY_INTERRUPTS_ENABLED();
    auto* current_thread = Thread::current();
    auto& process = current_thread->process();
    current_thread->did_syscall();

    PerformanceManager::add_syscall_event(*current_thread, regs);

    if (g_in_system_shutdown)
        return ENOSYS;

    if (function >= Function::__Count) {
        dbgln("Unknown syscall {} requested ({:p}, {:p}, {:p}, {:p})", function, arg1, arg2, arg3, arg4);
        return ENOSYS;
    }

    auto const syscall_metadata = s_syscall_table[function];
    if (syscall_metadata.handler == nullptr) {
        dbgln("Null syscall {} requested, you probably need to rebuild this program!", function);
        return ENOSYS;
    }

    MutexLocker mutex_locker;
    auto const needs_big_lock = syscall_metadata.needs_lock == NeedsBigProcessLock::Yes;
    if (needs_big_lock) {
        mutex_locker.attach_and_lock(process.big_lock());
    };

    if (function == SC_exit || function == SC_exit_thread) {
        // These syscalls need special handling since they never return to the caller.
        // In these cases the process big lock will get released on the exit of the thread.

        if (auto* tracer = process.tracer(); tracer && tracer->is_tracing_syscalls()) {
            regs.set_return_reg(0);
            tracer->set_trace_syscalls(false);
            process.tracer_trap(*current_thread, regs); // this triggers SIGTRAP and stops the thread!
        }

        switch (function) {
        case SC_exit:
            process.sys$exit(arg1);
        case SC_exit_thread:
            process.sys$exit_thread(arg1, arg2, arg3);
        default:
            VERIFY_NOT_REACHED();
        }
    }

    ErrorOr<FlatPtr> result { FlatPtr(nullptr) };
    result = syscall_metadata.handler(process, regs, arg1, arg2, arg3, arg4);

    return result;
}

}

extern "C" NEVER_INLINE void syscall_handler(TrapFrame* trap);
NEVER_INLINE void syscall_handler(TrapFrame* trap)
{
    prepare_syscall_entry();

    auto& regs = *trap->regs;
    auto* current_thread = Thread::current();
    VERIFY(current_thread->previous_mode() == ExecutionMode::User);
    auto& process = current_thread->process();
    if (process.is_dying()) {
        // It's possible this thread is just about to make a syscall while another is
        // is killing our process.
        current_thread->die_if_needed();
        return;
    }

    if (auto* tracer = process.tracer(); tracer && tracer->is_tracing_syscalls()) {
        tracer->set_trace_syscalls(false);
        process.tracer_trap(*current_thread, regs); // this triggers SIGTRAP and stops the thread!
    }

    current_thread->yield_if_should_be_stopped();

    // Apply a random offset in the range 0-255 to the stack pointer,
    // to make kernel stacks a bit less deterministic.
    auto* ptr = static_cast<char*>(__builtin_alloca(get_fast_random<u8>()));
    AK::taint_for_optimizer(ptr);

    validate_syscall_entry_registers(regs);

    Memory::MemoryManager::validate_syscall_preconditions(process, regs);

    FlatPtr function;
    FlatPtr arg1;
    FlatPtr arg2;
    FlatPtr arg3;
    FlatPtr arg4;
    regs.capture_syscall_params(function, arg1, arg2, arg3, arg4);

    auto result = Syscall::handle(regs, function, arg1, arg2, arg3, arg4);

    if (result.is_error()) {
        regs.set_return_reg(-result.error().code());
    } else {
        regs.set_return_reg(result.value());
    }

    if (auto* tracer = process.tracer(); tracer && tracer->is_tracing_syscalls()) {
        tracer->set_trace_syscalls(false);
        process.tracer_trap(*current_thread, regs); // this triggers SIGTRAP and stops the thread!
    }

    current_thread->yield_if_should_be_stopped();

    current_thread->check_dispatch_pending_signal();

    // If the previous mode somehow changed something is seriously messed up...
    VERIFY(current_thread->previous_mode() == ExecutionMode::User);

    // Check if we're supposed to return to userspace or just die.
    current_thread->die_if_needed();

    // Crash any processes which have committed a promise violation during syscall handling.
    if (result.is_error() && result.error().code() == EPROMISEVIOLATION) {
        VERIFY(current_thread->is_promise_violation_pending());
        current_thread->set_promise_violation_pending(false);
        process.crash(SIGABRT, {});
    } else {
        VERIFY(!current_thread->is_promise_violation_pending());
    }

    VERIFY(!g_scheduler_lock.is_locked_by_current_processor());
}

}
