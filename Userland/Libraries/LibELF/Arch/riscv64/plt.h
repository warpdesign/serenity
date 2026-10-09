/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibELF/DynamicObject.h>

namespace ELF {

inline void install_plt_trampoline(DynamicObject& object, void (*trampoline)())
{
    auto* got = reinterpret_cast<FlatPtr*>(object.plt_got_base_address().as_ptr());
    got[0] = reinterpret_cast<FlatPtr>(trampoline);
    got[1] = reinterpret_cast<FlatPtr>(&object);
}

}
