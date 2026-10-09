/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibELF/Arch/powerpc/SecurePLT.h>
#include <LibELF/DynamicObject.h>

namespace ELF {

inline void install_plt_trampoline(DynamicObject& object, void (*trampoline)())
{
    FlatPtr got_virtual_address = 0;
    object.for_each_dynamic_entry([&](DynamicObject::DynamicEntry const& entry) {
        if (entry.tag() == DT_PPC_GOT)
            got_virtual_address = entry.ptr();
    });
    VERIFY(got_virtual_address);
    auto got_address = object.base_address().get() + got_virtual_address - object.elf_base_address().get();
    install_secure_plt_resolver(reinterpret_cast<FlatPtr*>(got_address), &object, trampoline);
}

}
