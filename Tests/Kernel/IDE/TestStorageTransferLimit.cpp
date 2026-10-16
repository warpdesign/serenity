/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/Storage/StorageTransferLimit.h>

using Kernel::plan_storage_transfer;

static_assert(plan_storage_transfer(128 * 1024, 512, 4096).whole_blocks == 8);
static_assert(plan_storage_transfer(128 * 1024, 512, 64 * 1024).whole_blocks == 128);
static_assert(plan_storage_transfer(128 * 1024, 512, 64 * 1024).remaining_bytes == 0);
static_assert(plan_storage_transfer(4097, 512, 64 * 1024).remaining_bytes == 1);
static_assert(plan_storage_transfer(64 * 1024 + 1, 512, 64 * 1024).remaining_bytes == 0);

static size_t requests_for_one_mebibyte(size_t transfer_limit)
{
    size_t remaining = 1024 * 1024;
    size_t requests = 0;
    while (remaining > 0) {
        auto plan = plan_storage_transfer(remaining, 512, transfer_limit);
        auto bytes = plan.whole_blocks * 512 + plan.remaining_bytes;
        if (bytes == 0)
            return 0;
        remaining -= bytes;
        ++requests;
    }
    return requests;
}

int main(int argc, char**)
{
    if (requests_for_one_mebibyte(4096) != 256)
        return 1;
    if (requests_for_one_mebibyte(64 * 1024) != 16)
        return 1;
    if (argc > 1 && plan_storage_transfer(768, 512, 4096).remaining_bytes != 256)
        return 1;
    return 0;
}
