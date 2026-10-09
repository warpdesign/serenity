/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Array.h>
#include <Kernel/API/POSIX/errno.h>
#include <Kernel/Arch/Interrupts.h>
#include <Kernel/Interrupts/GenericInterruptHandler.h>
#include <Kernel/Interrupts/SharedIRQHandler.h>
#include <Kernel/Interrupts/UnhandledInterruptHandler.h>

namespace Kernel {

static Array<GenericInterruptHandler*, GENERIC_INTERRUPT_HANDLERS_COUNT> s_interrupt_handlers;

GenericInterruptHandler& get_interrupt_handler(InterruptNumber number)
{
    VERIFY(number.value() < s_interrupt_handlers.size());
    auto* handler = s_interrupt_handlers[number.value()];
    VERIFY(handler);
    return *handler;
}

ErrorOr<InterruptNumber> reserve_interrupt_handlers(size_t)
{
    // Pegasos 2 uses the sixteen legacy 8259 lines; PCI MSI is not routed yet.
    return ENOTSUP;
}

static void revert_to_unhandled(InterruptNumber number)
{
    auto* handler = new UnhandledInterruptHandler(number);
    handler->register_interrupt_handler();
}

void register_generic_interrupt_handler(InterruptNumber number, GenericInterruptHandler& handler)
{
    VERIFY(number.value() < s_interrupt_handlers.size());
    auto*& slot = s_interrupt_handlers[number.value()];
    if (!slot) {
        slot = &handler;
        return;
    }
    if (slot->type() == HandlerType::UnhandledInterruptHandler) {
        auto* unhandled = static_cast<UnhandledInterruptHandler*>(slot);
        unhandled->unregister_interrupt_handler();
        delete unhandled;
        slot = &handler;
        return;
    }
    if (slot->is_shared_handler()) {
        VERIFY(slot->type() == HandlerType::SharedIRQHandler);
        static_cast<SharedIRQHandler*>(slot)->register_handler(handler);
        return;
    }
    VERIFY(slot->type() == HandlerType::IRQHandler);
    slot = SharedIRQHandler::initialize(number, *slot, handler);
}

void unregister_generic_interrupt_handler(InterruptNumber number, GenericInterruptHandler& handler)
{
    VERIFY(number.value() < s_interrupt_handlers.size());
    auto*& slot = s_interrupt_handlers[number.value()];
    VERIFY(slot);
    if (slot->type() == HandlerType::UnhandledInterruptHandler)
        return;
    if (slot->is_shared_handler()) {
        VERIFY(slot->type() == HandlerType::SharedIRQHandler);
        auto* shared = static_cast<SharedIRQHandler*>(slot);
        shared->unregister_handler(handler);
        if (shared->sharing_devices_count() == 0) {
            slot = nullptr;
            revert_to_unhandled(number);
        }
        return;
    }
    VERIFY(slot->type() == HandlerType::IRQHandler);
    slot = nullptr;
    revert_to_unhandled(number);
}

void initialize_interrupts()
{
    for (size_t index = 0; index < s_interrupt_handlers.size(); ++index) {
        VERIFY(!s_interrupt_handlers[index]);
        auto* handler = new UnhandledInterruptHandler(index);
        handler->register_interrupt_handler();
    }
}

}
