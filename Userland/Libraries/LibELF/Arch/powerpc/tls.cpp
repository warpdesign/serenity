/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Arch/tls.h>

namespace ELF {

void set_thread_pointer_register(FlatPtr value)
{
    __asm__ volatile("mr 2,%0" : : "r"(value) : "r2", "memory");
}

}
