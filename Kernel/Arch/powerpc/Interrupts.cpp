/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Array.h>
#include <Kernel/API/POSIX/errno.h>
#include <Kernel/Arch/Interrupts.h>
#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/TrapFrame.h>
#include <Kernel/Arch/powerpc/InterruptManagement.h>
#include <Kernel/Arch/powerpc/CPU.h>
#include <Kernel/Arch/powerpc/KernelContext.h>
#include <Kernel/Interrupts/GenericInterruptHandler.h>
#include <Kernel/Interrupts/SharedIRQHandler.h>
#include <Kernel/Interrupts/UnhandledInterruptHandler.h>
#include <Kernel/Security/Random.h>
#include <Kernel/Tasks/Scheduler.h>

namespace Kernel {

static Array<GenericInterruptHandler*, GENERIC_INTERRUPT_HANDLERS_COUNT> s_interrupt_handlers;
static EntropySource s_external_interrupt_entropy { EntropySource::Static::Interrupts };

extern "C" [[noreturn]] void powerpc_external_exception_handler(RegisterState*);

extern "C" [[noreturn]] void powerpc_external_exception_handler(RegisterState* registers)
{
    VERIFY(registers);
    registers->srr1 &= ~PowerPC::MSR_POW;
    auto previous_mode = registers->previous_mode();
    TrapFrame trap { registers };
    Processor::current().enter_trap(trap, true);
    for (auto& controller : InterruptManagement::the().controllers()) {
        for (;;) {
            auto irq = controller->pending_interrupt();
            if (!irq.has_value())
                break;
            if (Scheduler::is_initialized())
                s_external_interrupt_entropy.add_random_event(irq->value());
            auto& handler = get_interrupt_handler(irq.value());
            handler.increment_call_count();
            handler.handle_interrupt();
            handler.eoi();
        }
    }
    Processor::current().exit_trap(trap);
    if (previous_mode == ExecutionMode::User)
        powerpc_return_to_user_from_high(*registers);
    powerpc_restore_supervisor_exception_state(*registers);
}

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
