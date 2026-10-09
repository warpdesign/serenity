/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/StackUnwinder.h>
#include <errno.h>

extern "C" char __stack_end[];
extern "C" char powerpc_unwind_level1_resume[];
extern "C" char powerpc_unwind_level2_resume[];

[[gnu::noinline]] static void force_nested_call()
{
    asm volatile("" : : : "memory");
}

[[gnu::noinline]] static int unwind_level3()
{
    force_nested_call();
    FlatPtr stack_pointer;
    asm volatile("mr %0,1" : "=r"(stack_pointer));

    FlatPtr return_addresses[2] {};
    unsigned frame_count = 0;
    auto result = AK::unwind_stack_from_frame_pointer(
        stack_pointer,
        [stack_pointer](FlatPtr address) -> ErrorOr<FlatPtr> {
            if (address < stack_pointer || address > reinterpret_cast<FlatPtr>(__stack_end) - sizeof(FlatPtr))
                return Error::from_errno(EFAULT);
            return *reinterpret_cast<FlatPtr const*>(address);
        },
        [&return_addresses, &frame_count](AK::StackFrame frame) -> ErrorOr<IterationDecision> {
            return_addresses[frame_count++] = frame.return_address;
            return frame_count == 2 ? IterationDecision::Break : IterationDecision::Continue;
        });
    return !result.is_error() && frame_count == 2
        && return_addresses[0] == reinterpret_cast<FlatPtr>(powerpc_unwind_level2_resume)
        && return_addresses[1] == reinterpret_cast<FlatPtr>(powerpc_unwind_level1_resume);
}

[[gnu::noinline]] static int unwind_level2()
{
    int result = unwind_level3();
    asm volatile(".global powerpc_unwind_level2_resume\npowerpc_unwind_level2_resume:" : : : "memory");
    return result;
}

extern "C" int powerpc_stack_unwinder_test()
{
    int result = unwind_level2();
    asm volatile(".global powerpc_unwind_level1_resume\npowerpc_unwind_level1_resume:" : : : "memory");
    return result;
}
