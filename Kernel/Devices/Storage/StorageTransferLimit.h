/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/StdLibExtras.h>
#include <AK/Types.h>

namespace Kernel {

struct StorageTransferPlan {
    size_t whole_blocks;
    size_t remaining_bytes;
};

// The caller submits one request at a time. A partial trailing block belongs
// to the next request when full blocks have already reached the device limit.
constexpr StorageTransferPlan plan_storage_transfer(size_t requested_bytes, size_t block_size, size_t max_transfer_bytes)
{
    auto max_blocks = max_transfer_bytes / block_size;
    auto whole_blocks = min(requested_bytes / block_size, max_blocks);
    return { whole_blocks, whole_blocks == max_blocks ? 0 : requested_bytes % block_size };
}

}
