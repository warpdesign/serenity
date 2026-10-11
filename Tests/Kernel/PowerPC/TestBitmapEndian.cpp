/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/BitmapView.h>

extern "C" int powerpc_bitmap_endian_test()
{
    alignas(4) u8 bytes[16] {};
    AK::BitmapView bitmap { bytes, 128 };

    bytes[0] = 1;
    if (bitmap.find_one_anywhere_set().value_or(128) != 0
        || bitmap.find_one_anywhere_unset().value_or(128) != 1
        || bitmap.find_first_fit(1).value_or(128) != 1)
        return 0;

    bytes[0] = 0xff;
    bytes[1] = 0xff;
    bytes[2] = 0xff;
    bytes[3] = 0xff;
    bytes[4] = 1;
    return bitmap.find_one_anywhere_unset().value_or(128) == 33
        && bitmap.find_first_fit(1).value_or(128) == 33;
}
