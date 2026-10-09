/*
 * Copyright (c) 2024-2026, Sönke Holz <soenke.holz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Arch/StackUnwinder.h>
#include <AK/Concepts.h>
#include <AK/Error.h>
#include <AK/Types.h>

namespace AK {

struct StackFrame {
    FlatPtr return_address;
    FlatPtr previous_frame_pointer;
};

// This function only returns errors if on_stack_frame returns an error.
// It doesn't return an error on failed memory reads, since the last frame record sometimes contains invalid addresses when using frame pointer-based unwinding.
ErrorOr<void> unwind_stack_from_frame_pointer(FlatPtr frame_pointer, CallableAs<ErrorOr<FlatPtr>, FlatPtr> auto read_memory, CallableAs<ErrorOr<IterationDecision>, StackFrame> auto on_stack_frame)
{
    FlatPtr current_frame_pointer = frame_pointer;

    while (current_frame_pointer != 0) {
        StackFrame stack_frame;

        FlatPtr previous_frame_pointer = 0;
        FlatPtr return_address_base = current_frame_pointer;
        if constexpr (StackFrameLayout::return_address_in_previous_frame) {
            auto maybe_previous_frame_pointer = read_memory(current_frame_pointer + StackFrameLayout::previous_frame_pointer_offset);
            if (maybe_previous_frame_pointer.is_error())
                return {};
            previous_frame_pointer = maybe_previous_frame_pointer.value();
            if (previous_frame_pointer <= current_frame_pointer)
                return {};
            return_address_base = previous_frame_pointer;
        }

        auto maybe_return_address = read_memory(return_address_base + StackFrameLayout::return_address_offset);
        if (maybe_return_address.is_error())
            return {};
        stack_frame.return_address = maybe_return_address.value();

        if (stack_frame.return_address == 0)
            return {};

        if constexpr (!StackFrameLayout::return_address_in_previous_frame) {
            auto maybe_previous_frame_pointer = read_memory(current_frame_pointer + StackFrameLayout::previous_frame_pointer_offset);
            if (maybe_previous_frame_pointer.is_error())
                return {};
            previous_frame_pointer = maybe_previous_frame_pointer.value();
        }
        stack_frame.previous_frame_pointer = previous_frame_pointer;

        if (TRY(on_stack_frame(stack_frame)) == IterationDecision::Break)
            return {};

        current_frame_pointer = previous_frame_pointer;
    }

    return {};
}

}
