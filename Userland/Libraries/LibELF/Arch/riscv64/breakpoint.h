/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace ELF {

inline void breakpoint_trap()
{
    asm volatile("ebreak");
}

}
