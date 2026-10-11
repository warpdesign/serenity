/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/ExceptionState.h>
#include <Kernel/Arch/powerpc/InitialThreadContext.h>
#include <Kernel/Arch/powerpc/KernelContext.h>
#include <Kernel/Arch/powerpc/KernelContextOffsets.h>
#include <Kernel/Arch/powerpc/RegisterState.h>
#include <Kernel/Arch/powerpc/ThreadRegisters.h>
#include <Kernel/Arch/TrapFrame.h>
#include <setjmp.h>

static_assert(__builtin_offsetof(PowerPCExceptionState, gpr[1]) == PPC_CONTEXT_GPR1);
static_assert(__builtin_offsetof(PowerPCExceptionState, gpr[2]) == PPC_CONTEXT_GPR2);
static_assert(__builtin_offsetof(PowerPCExceptionState, gpr[13]) == PPC_CONTEXT_GPR13);
static_assert(__builtin_offsetof(PowerPCExceptionState, cr) == PPC_CONTEXT_CR);
static_assert(__builtin_offsetof(PowerPCExceptionState, ctr) == PPC_CONTEXT_CTR);
static_assert(__builtin_offsetof(PowerPCExceptionState, xer) == PPC_CONTEXT_XER);
static_assert(__builtin_offsetof(PowerPCExceptionState, lr) == PPC_CONTEXT_LR);
static_assert(__builtin_offsetof(PowerPCExceptionState, srr0) == PPC_CONTEXT_IP);
static_assert(__builtin_offsetof(PowerPCExceptionState, srr1) == PPC_CONTEXT_SRR1);

static Kernel::ThreadRegisters s_main_context {};
static Kernel::ThreadRegisters s_worker_context {};
static Kernel::ThreadRegisters s_initial_worker_context {};
static Kernel::ThreadRegisters s_assumed_context {};
alignas(16) static u8 s_worker_stack[4096] {};
alignas(16) static u8 s_initial_worker_stack[4096] {};
alignas(16) static u8 s_assumed_stack[4096] {};
static Kernel::TrapFrame* s_initial_trap;
static Kernel::TrapFrame* s_assumed_trap;
static jmp_buf s_assume_escape;
static volatile u32 s_assume_active;
static volatile u32 s_assume_prepares;
static volatile u32 s_assume_entries;
static volatile u32 s_assume_first_inits;
static u32 s_main_tag;
static u32 s_worker_tag;
static volatile u32 s_phase;
static volatile u32 s_worker_failed;
static volatile u32 s_transitions;
static volatile u32 s_first_init_calls;
static u32 s_original_msr;
extern "C" volatile u32 powerpc_assume_context_debug;
volatile u32 powerpc_assume_context_debug;

static Kernel::Thread* main_tag()
{
    return reinterpret_cast<Kernel::Thread*>(&s_main_tag);
}

static Kernel::Thread* worker_tag()
{
    return reinterpret_cast<Kernel::Thread*>(&s_worker_tag);
}

extern "C" void powerpc_enter_thread_context(Kernel::Thread* from, Kernel::Thread* to)
{
    FlatPtr stack_pointer;
    asm volatile("mr %0,1" : "=r"(stack_pointer));
    if (s_assume_active) {
        auto stack_start = reinterpret_cast<FlatPtr>(s_assumed_stack);
        if (from != worker_tag() || to != worker_tag()
            || stack_pointer < stack_start || stack_pointer >= stack_start + sizeof(s_assumed_stack))
            s_worker_failed = 1;
        s_assume_entries = s_assume_entries + 1;
        return;
    }
    auto worker_stack_start = reinterpret_cast<FlatPtr>(s_worker_stack);
    auto initial_worker_stack_start = reinterpret_cast<FlatPtr>(s_initial_worker_stack);
    bool on_worker_stack = (stack_pointer >= worker_stack_start && stack_pointer < worker_stack_start + sizeof(s_worker_stack))
        || (stack_pointer >= initial_worker_stack_start && stack_pointer < initial_worker_stack_start + sizeof(s_initial_worker_stack));
    bool going_to_worker = to == worker_tag();
    if (from != (going_to_worker ? main_tag() : worker_tag()) || on_worker_stack != going_to_worker)
        s_worker_failed = 1;
    s_transitions = s_transitions + 1;
}

extern "C" void context_first_init(Kernel::Thread* from, Kernel::Thread* to)
{
    FlatPtr stack_pointer;
    asm volatile("mr %0,1" : "=r"(stack_pointer));
    if (s_assume_active) {
        auto stack_start = reinterpret_cast<FlatPtr>(s_assumed_stack);
        if (from != worker_tag() || to != worker_tag()
            || stack_pointer < stack_start || stack_pointer >= stack_start + sizeof(s_assumed_stack))
            s_worker_failed = 1;
        s_assume_first_inits = s_assume_first_inits + 1;
        return;
    }
    auto stack_start = reinterpret_cast<FlatPtr>(s_initial_worker_stack);
    if (from != main_tag() || to != worker_tag()
        || stack_pointer < stack_start || stack_pointer >= stack_start + sizeof(s_initial_worker_stack))
        s_worker_failed = 1;
    s_first_init_calls = s_first_init_calls + 1;
}

extern "C" [[noreturn]] void powerpc_finish_first_thread_context(Kernel::TrapFrame* trap)
{
    if (s_assume_active) {
        if (trap != s_assumed_trap || !trap->regs || trap->regs->previous_mode() != Kernel::ExecutionMode::Kernel)
            s_worker_failed = 1;
        Kernel::powerpc_restore_supervisor_exception_state(*trap->regs);
    }
    if (trap != s_initial_trap || !trap->regs || trap->regs->previous_mode() != Kernel::ExecutionMode::Kernel)
        s_worker_failed = 1;
    Kernel::powerpc_restore_supervisor_exception_state(*trap->regs);
}

extern "C" FlatPtr powerpc_prepare_assume_context(Kernel::Thread* thread, u32 state)
{
    if (!s_assume_active || thread != worker_tag() || state != 1)
        s_worker_failed = 1;
    s_assume_prepares = s_assume_prepares + 1;
    return s_assumed_context.sp();
}

extern "C" [[noreturn]] void powerpc_assumed_context_worker(u32 argument)
{
    u32 thread_pointer;
    u32 machine_state;
    asm volatile("mr %0,2; mfmsr %1" : "=r"(thread_pointer), "=r"(machine_state));
    if (argument != 0x56781234u || thread_pointer != 0x10293847u
        || machine_state != s_original_msr)
        powerpc_assume_context_debug = (argument != 0x56781234u ? 1u : 0u)
            | (thread_pointer != 0x10293847u ? 2u : 0u)
            | (machine_state != s_original_msr ? 4u : 0u);
    if (powerpc_assume_context_debug)
        s_worker_failed = 1;
    longjmp(s_assume_escape, 1);
}

extern "C" int powerpc_assume_context_test()
{
    s_assume_prepares = 0;
    s_assume_entries = 0;
    s_assume_first_inits = 0;
    powerpc_assume_context_debug = 0;
    auto stack_top = reinterpret_cast<FlatPtr>(s_assumed_stack + sizeof(s_assumed_stack));
    s_assumed_context.set_ip(FlatPtr(&powerpc_assumed_context_worker));
    s_assumed_context.set_sp(stack_top);
    s_assumed_context.gpr[2] = 0x10293847u;
    s_assumed_context.gpr[3] = 0x56781234u;
    asm volatile("mfmsr %0" : "=r"(s_original_msr));
    s_assumed_context.srr1 = s_original_msr;
    Kernel::prepare_initial_thread_context(s_assumed_context, stack_top, s_assumed_trap);
    int result = setjmp(s_assume_escape);
    if (result == 0) {
        s_assume_active = 1;
        Kernel::do_assume_context(worker_tag(), 1);
        return 0;
    }
    s_assume_active = 0;
    if (result != 1)
        powerpc_assume_context_debug = 0x100u | static_cast<u32>(result);
    else if (s_assume_prepares != 1)
        powerpc_assume_context_debug = 0x200u | s_assume_prepares;
    else if (s_assume_entries != 1)
        powerpc_assume_context_debug = 0x300u | s_assume_entries;
    else if (s_assume_first_inits != 1)
        powerpc_assume_context_debug = 0x400u | s_assume_first_inits;
    else if (s_worker_failed)
        powerpc_assume_context_debug = 0x500u | s_worker_failed;
    return result == 1 && s_assume_prepares == 1 && s_assume_entries == 1
        && s_assume_first_inits == 1 && !s_worker_failed;
}

extern "C" [[noreturn]] void powerpc_initial_context_worker(u32 argument)
{
    u32 thread_pointer;
    u32 machine_state;
    asm volatile("mr %0,2" : "=r"(thread_pointer));
    asm volatile("mfmsr %0" : "=r"(machine_state));
    if (argument != 0x34567890u || thread_pointer != 0x10293847u || s_first_init_calls != 1
        || (machine_state & 0xe030u) != 0xa030u)
        s_worker_failed = 1;
    s_phase = 3;
    asm volatile("mtmsr %0; isync" : : "r"(s_original_msr) : "memory");
    (void)powerpc_switch_kernel_context(s_initial_worker_context, s_main_context, worker_tag(), main_tag());
    for (;;)
        asm volatile("nop" : : : "memory");
}

extern "C" [[noreturn]] void powerpc_kernel_context_worker(Kernel::Thread* from, Kernel::Thread* to)
{
    u32 thread_pointer;
    asm volatile("mr %0,2" : "=r"(thread_pointer));
    if (from != main_tag() || to != worker_tag() || thread_pointer != 0x13572468u)
        s_worker_failed = 1;

    register u32 preserved asm("r14") = 0x24681357u;
    asm volatile("" : "+r"(preserved));
    s_phase = 1;
    auto* resumed_from = powerpc_switch_kernel_context(s_worker_context, s_main_context, worker_tag(), main_tag());
    asm volatile("" : "+r"(preserved));
    if (resumed_from != main_tag() || preserved != 0x24681357u)
        s_worker_failed = 1;
    asm volatile("mr %0,2" : "=r"(thread_pointer));
    if (thread_pointer != 0x13572468u)
        s_worker_failed = 1;

    s_phase = 2;
    (void)powerpc_switch_kernel_context(s_worker_context, s_main_context, worker_tag(), main_tag());
    for (;;)
        asm volatile("nop" : : : "memory");
}

extern "C" int powerpc_kernel_context_test()
{
    auto stack_top = (reinterpret_cast<FlatPtr>(s_worker_stack + sizeof(s_worker_stack)) & ~static_cast<FlatPtr>(15)) - 16;
    *reinterpret_cast<FlatPtr*>(stack_top) = 0;
    s_worker_context.set_sp(stack_top);
    s_worker_context.set_ip(reinterpret_cast<FlatPtr>(&powerpc_kernel_context_worker));
    s_worker_context.gpr[2] = 0x13572468u;

    u32 original_thread_pointer;
    asm volatile("mr %0,2" : "=r"(original_thread_pointer));
    auto* first_from = powerpc_switch_kernel_context(s_main_context, s_worker_context, main_tag(), worker_tag());
    if (first_from != worker_tag() || s_phase != 1 || s_transitions != 2 || s_worker_failed)
        return 0;
    u32 resumed_thread_pointer;
    asm volatile("mr %0,2" : "=r"(resumed_thread_pointer));
    if (resumed_thread_pointer != original_thread_pointer)
        return 0;

    auto* second_from = powerpc_switch_kernel_context(s_main_context, s_worker_context, main_tag(), worker_tag());
    asm volatile("mr %0,2" : "=r"(resumed_thread_pointer));
    if (second_from != worker_tag() || s_phase != 2 || s_transitions != 4 || s_worker_failed
        || resumed_thread_pointer != original_thread_pointer)
        return 0;

    auto initial_stack_top = reinterpret_cast<FlatPtr>(s_initial_worker_stack + sizeof(s_initial_worker_stack));
    s_initial_worker_context.set_ip(FlatPtr(&powerpc_initial_context_worker));
    s_initial_worker_context.set_sp(initial_stack_top);
    s_initial_worker_context.gpr[2] = 0x10293847u;
    s_initial_worker_context.gpr[3] = 0x34567890u;
    asm volatile("mfmsr %0" : "=r"(s_original_msr));
    s_initial_worker_context.srr1 = 0xa030u;
    Kernel::prepare_initial_thread_context(s_initial_worker_context, initial_stack_top, s_initial_trap);
    auto* initial_from = powerpc_switch_kernel_context(s_main_context, s_initial_worker_context, main_tag(), worker_tag());
    asm volatile("mr %0,2" : "=r"(resumed_thread_pointer));
    return initial_from == worker_tag() && s_phase == 3 && s_first_init_calls == 1
        && s_transitions == 6 && !s_worker_failed && resumed_thread_pointer == original_thread_pointer;
}
