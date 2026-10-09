/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Function.h>
#include <AK/Platform.h>
#include <AK/Vector.h>
#include <Kernel/Arch/powerpc/IRQController.h>
#include <Kernel/Library/LockRefPtr.h>

VALIDATE_IS_POWERPC32()

namespace Kernel {

class InterruptManagement {
public:
    static InterruptManagement& the();
    static void initialize();
    static bool initialized();

    static ErrorOr<void> register_interrupt_controller(NonnullLockRefPtr<IRQController>);
    static InterruptNumber acquire_mapped_interrupt_number(InterruptNumber original_irq);

    Vector<NonnullLockRefPtr<IRQController>> const& controllers();
    NonnullLockRefPtr<IRQController> get_responsible_irq_controller(InterruptNumber);
    void enumerate_interrupt_handlers(Function<void(GenericInterruptHandler&)>);

private:
    InterruptManagement() = default;
};

}
