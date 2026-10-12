/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/InterruptController.h>
#include <Kernel/Interrupts/GenericInterruptHandler.h>

namespace Kernel::PowerPC::Pegasos2 {

InterruptController::InterruptController(Memory::TypedMapping<volatile u8[]> io_ports, Memory::TypedMapping<volatile u8> intack)
    : m_io_ports(move(io_ports))
    , m_intack(move(intack))
    , m_pic(m_io_ports.ptr(), m_intack.ptr())
{
    m_pic.initialize();
}

ErrorOr<NonnullLockRefPtr<InterruptController>> InterruptController::create()
{
    auto io_ports = TRY(Memory::map_typed<volatile u8[]>(PhysicalAddress { LegacyPIC::io_physical_base },
        LegacyPIC::io_window_size, Memory::Region::Access::ReadWrite));
    auto intack = TRY(Memory::map_typed<volatile u8>(PhysicalAddress { LegacyPIC::intack_physical_base + LegacyPIC::intack_offset }));
    return TRY(adopt_nonnull_lock_ref_or_enomem(new (nothrow) InterruptController(move(io_ports), move(intack))));
}

bool InterruptController::configure_pci_interrupts(Array<u8, 4> const& pirq_lines)
{
    u16 level_triggered_irqs = 0;
    for (auto line : pirq_lines) {
        if (line == 0 || line == 0xff)
            continue;
        if (line >= 16)
            return false;
        level_triggered_irqs |= static_cast<u16>(1u << line);
    }
    SpinlockLocker locker(m_lock);
    return m_pic.set_level_triggered(level_triggered_irqs);
}

void InterruptController::enable(GenericInterruptHandler const& handler)
{
    SpinlockLocker locker(m_lock);
    VERIFY(m_pic.enable(handler.interrupt_number().value()));
}

void InterruptController::disable(GenericInterruptHandler const& handler)
{
    SpinlockLocker locker(m_lock);
    VERIFY(m_pic.disable(handler.interrupt_number().value()));
}

void InterruptController::eoi(GenericInterruptHandler const& handler)
{
    SpinlockLocker locker(m_lock);
    m_pic.end_of_interrupt(handler.interrupt_number().value());
}

Optional<InterruptNumber> InterruptController::pending_interrupt() const
{
    SpinlockLocker locker(m_lock);
    auto irq = m_pic.acknowledge();
    if (!irq.has_value())
        return {};
    return irq.value();
}

}
