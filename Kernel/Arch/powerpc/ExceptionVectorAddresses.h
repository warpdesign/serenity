/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

// The MPC750 vectors end at 0x1700. Keep shared real-mode code in the
// remaining space of the second physical page, outside firmware scratch.
#define POWERPC_USER_RETURN_TRAMPOLINE_ADDRESS 0x1800
#define POWERPC_EXCEPTION_CODE_LIMIT 0x2000
