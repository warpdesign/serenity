/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/NeverDestroyed.h>
#include <Kernel/Arch/Interrupts.h>
#include <Kernel/Arch/powerpc/InterruptManagement.h>
#include <Kernel/Arch/powerpc/Pegasos2/InterruptController.h>
#include <Kernel/Interrupts/GenericInterruptHandler.h>
#include <Kernel/Interrupts/SharedIRQHandler.h>

namespace Kernel {

static NeverDestroyed<Vector<NonnullLockRefPtr<IRQController>>> s_interrupt_controllers;
static InterruptManagement* s_interrupt_management;

bool InterruptManagement::initialized()
{
    return s_interrupt_management != nullptr;
}

InterruptManagement& InterruptManagement::the()
{
    VERIFY(initialized());
    return *s_interrupt_management;
}

void InterruptManagement::initialize()
{
    VERIFY(!initialized());
    auto controller = MUST(PowerPC::Pegasos2::InterruptController::create());
    MUST(register_interrupt_controller(move(controller)));
    s_interrupt_management = new InterruptManagement;
    initialize_interrupts();
}

ErrorOr<void> InterruptManagement::register_interrupt_controller(NonnullLockRefPtr<IRQController> controller)
{
    VERIFY(!initialized());
    return s_interrupt_controllers->try_append(move(controller));
}

InterruptNumber InterruptManagement::acquire_mapped_interrupt_number(InterruptNumber irq)
{
    return irq;
}

Vector<NonnullLockRefPtr<IRQController>> const& InterruptManagement::controllers()
{
    return *s_interrupt_controllers;
}

NonnullLockRefPtr<IRQController> InterruptManagement::get_responsible_irq_controller(InterruptNumber irq)
{
    VERIFY(irq.value() < 16);
    VERIFY(s_interrupt_controllers->size() == 1);
    return (*s_interrupt_controllers)[0];
}

void InterruptManagement::enumerate_interrupt_handlers(Function<void(GenericInterruptHandler&)> callback)
{
    for (size_t index = 0; index < GENERIC_INTERRUPT_HANDLERS_COUNT; ++index) {
        auto& handler = get_interrupt_handler(index);
        if (handler.type() == HandlerType::SharedIRQHandler) {
            static_cast<SharedIRQHandler&>(handler).enumerate_handlers(callback);
            continue;
        }
        if (handler.type() != HandlerType::UnhandledInterruptHandler)
            callback(handler);
    }
}

}
