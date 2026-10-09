/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/AtomicRefCounted.h>
#include <AK/Optional.h>
#include <AK/StringView.h>
#include <Kernel/Interrupts/Interrupts.h>

namespace Kernel {

class GenericInterruptHandler;

class IRQController : public AtomicRefCounted<IRQController> {
public:
    virtual ~IRQController() = default;

    virtual void enable(GenericInterruptHandler const&) = 0;
    virtual void disable(GenericInterruptHandler const&) = 0;
    virtual void eoi(GenericInterruptHandler const&) = 0;
    virtual Optional<InterruptNumber> pending_interrupt() const = 0;
    virtual StringView model() const = 0;

protected:
    IRQController() = default;
};

}
