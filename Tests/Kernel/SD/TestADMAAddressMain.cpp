/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

extern "C" int sd_adma_address_test();

int main()
{
    return sd_adma_address_test() ? 0 : 1;
}
