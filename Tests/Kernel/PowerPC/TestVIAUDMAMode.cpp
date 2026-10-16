/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/Storage/IDE/VIAUDMAMode.h>

using Kernel::choose_via_udma_mode;

static_assert(choose_via_udma_mode(0x3f, 0, true).value() == 2);
static_assert(choose_via_udma_mode(0x3f, 1u << 5, true).value() == 5);
static_assert(choose_via_udma_mode(0x3f, 1u << 5, false).value() == 2);
static_assert(choose_via_udma_mode(0x3f, 1u << 0, true).value() == 0);
static_assert(!choose_via_udma_mode(0, 0, true).has_value());

int main(int argc, char**)
{
    if (choose_via_udma_mode(0x3f, 0, true, 0).value() != 0)
        return 1;
    if (choose_via_udma_mode(0x3f, 0, true, 1).value() != 1)
        return 1;
    if (choose_via_udma_mode(0x3f, 0, true, 5).value() != 5)
        return 1;
    if (argc > 1) {
        if (choose_via_udma_mode(0b101, 0, true).value() != 2)
            return 1;
        if (choose_via_udma_mode(0b101, 0, true, 1).has_value())
            return 1;
        if (choose_via_udma_mode(0x3f, 0, false, 5).has_value())
            return 1;
    }
    return 0;
}
