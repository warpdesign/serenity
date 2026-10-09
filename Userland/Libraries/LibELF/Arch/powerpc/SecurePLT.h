/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace ELF {

inline void install_secure_plt_resolver(FlatPtr* got, void* object, void (*trampoline)())
{
    got[1] = reinterpret_cast<FlatPtr>(trampoline);
    got[2] = reinterpret_cast<FlatPtr>(object);
}

}
