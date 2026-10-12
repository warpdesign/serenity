/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Memory/VirtualAddress.h>

namespace Kernel::Memory {

bool try_refill_powerpc_hash_page(VirtualAddress);

}
