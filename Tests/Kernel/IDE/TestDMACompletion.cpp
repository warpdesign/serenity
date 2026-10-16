/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/Storage/IDE/DMACompletion.h>

using Kernel::ide_dma_has_completed;
using Kernel::ide_dma_irq_acknowledgement;
using Kernel::ide_dma_pin_interrupt_is_masked;
using Kernel::ide_drive_ready_after_reset;

static_assert(ide_dma_pin_interrupt_is_masked(1 << 10));
static_assert(!ide_dma_pin_interrupt_is_masked(0));
static_assert(!ide_dma_has_completed(0x58, 0x21)); // DRQ, active, no INTRQ.
static_assert(!ide_dma_has_completed(0x50, 0x20)); // Idle without INTRQ is not completion.
static_assert(ide_dma_has_completed(0x50, 0x24)); // INTRQ, all PRDs consumed.
static_assert(ide_dma_has_completed(0x50, 0x25)); // INTRQ can precede PRD exhaustion.
static_assert(ide_dma_has_completed(0x50, 0x20, true)); // IRQ handler acknowledged the latch.
static_assert(!ide_dma_has_completed(0x58, 0x20, true)); // Drive still requests data.
static_assert(!ide_dma_has_completed(0x50, 0x22, true)); // Bus-master error takes precedence.
static_assert(ide_dma_irq_acknowledgement(0x27) == 0x25); // Keep error latched while acknowledging IRQ.
static_assert(ide_drive_ready_after_reset(0x50));
static_assert(!ide_drive_ready_after_reset(0x58));
static_assert(!ide_drive_ready_after_reset(0x51));
static_assert(!ide_drive_ready_after_reset(0xff));
static_assert(!ide_drive_ready_after_reset(0x00));

int main(int argc, char**)
{
    if (ide_dma_has_completed(0x51, 0x24)) // Drive error.
        return 1;
    if (ide_dma_has_completed(0x50, 0x26)) // PCI bus-master error.
        return 1;
    if (argc > 1) {
        if (ide_dma_has_completed(0x58, 0x24)) // Data request still asserted.
            return 1;
        if (ide_dma_has_completed(0xd0, 0x24)) // Drive still busy.
            return 1;
    }
    return 0;
}
