/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/Storage/SD/ADMAAddress.h>

extern "C" int sd_adma_address_test()
{
    auto below_4g = Kernel::SD::adma_address_words(0x0000'0000'1234'5000ull);
    if (below_4g.low != 0x1234'5000u || below_4g.high != 0)
        return 0;

    auto crossing_4g = Kernel::SD::adma_address_words(0x0000'0000'ffff'f000ull, 0x2000);
    if (crossing_4g.low != 0x1000u || crossing_4g.high != 1)
        return 0;

    auto first_descriptor = Kernel::SD::adma_address_words(0x0000'0001'2345'0000ull);
    auto second_descriptor = Kernel::SD::adma_address_words(0x0000'0001'2345'0000ull, 0x10000);
    if (first_descriptor.low != 0x2345'0000u || first_descriptor.high != 1
        || second_descriptor.low != 0x2346'0000u || second_descriptor.high != 1)
        return 0;

    auto above_4g = Kernel::SD::adma_address_words(0x1234'5678'9abc'd000ull, 0x2000);
    return above_4g.low == 0x9abc'f000u && above_4g.high == 0x1234'5678u;
}
