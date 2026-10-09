/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Arch/powerpc/IRQController.h>
#include <Kernel/Arch/powerpc/Pegasos2/LegacyPIC.h>
#include <Kernel/Locking/Spinlock.h>
#include <Kernel/Memory/TypedMapping.h>

VALIDATE_IS_POWERPC32()

namespace Kernel::PowerPC::Pegasos2 {

class InterruptController final : public IRQController {
public:
    static ErrorOr<NonnullLockRefPtr<InterruptController>> create();

    virtual void enable(GenericInterruptHandler const&) override;
    virtual void disable(GenericInterruptHandler const&) override;
    virtual void eoi(GenericInterruptHandler const&) override;
    virtual Optional<InterruptNumber> pending_interrupt() const override;
    virtual StringView model() const override { return "Pegasos 2 legacy 8259"sv; }

private:
    InterruptController(Memory::TypedMapping<volatile u8[]> io_ports, Memory::TypedMapping<volatile u8> intack);

    Memory::TypedMapping<volatile u8[]> m_io_ports;
    Memory::TypedMapping<volatile u8> m_intack;
    mutable LegacyPIC m_pic;
    mutable Spinlock<LockRank::None> m_lock;
};

}
