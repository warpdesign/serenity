/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

extern "C" int nvme_endian_test();

int main()
{
    return nvme_endian_test() ? 0 : 1;
}
