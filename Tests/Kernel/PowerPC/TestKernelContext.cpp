/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/ExceptionState.h>
#include <Kernel/Arch/powerpc/KernelContext.h>
#include <Kernel/Arch/powerpc/KernelContextOffsets.h>
#include <Kernel/Arch/powerpc/ThreadRegisters.h>

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
alignas(16) static u8 s_worker_stack[4096] {};
static u32 s_main_tag;
static u32 s_worker_tag;
static volatile u32 s_phase;
static volatile u32 s_worker_failed;
static volatile u32 s_transitions;

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
    auto worker_stack_start = reinterpret_cast<FlatPtr>(s_worker_stack);
    bool on_worker_stack = stack_pointer >= worker_stack_start && stack_pointer < worker_stack_start + sizeof(s_worker_stack);
    bool going_to_worker = to == worker_tag();
    if (from != (going_to_worker ? main_tag() : worker_tag()) || on_worker_stack != going_to_worker)
        s_worker_failed = 1;
    s_transitions = s_transitions + 1;
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
    return second_from == worker_tag() && s_phase == 2 && s_transitions == 4 && !s_worker_failed
        && resumed_thread_pointer == original_thread_pointer;
}
