/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Arch/powerpc/DynamicTag.h>

static_assert(ELF::handled_dynamic_tag_name(DT_PPC_GOT) != nullptr);
static_assert(ELF::handled_dynamic_tag_name(DT_PPC_GOT)[0] == 'P');
static_assert(ELF::handled_dynamic_tag_name(DT_NEEDED) == nullptr);

int main()
{
    auto* name = ELF::handled_dynamic_tag_name(DT_PPC_GOT);
    return name[0] == 'P' && name[4] == 'G' ? 0 : 1;
}
