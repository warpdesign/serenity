/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/SmapDisabler.h>
#include <Kernel/Arch/powerpc/CPU.h>

extern "C" int powerpc_smap_disabler_test()
{
    auto before = Kernel::PowerPC::read_msr();
    {
        Kernel::SmapDisabler outer;
        if (Kernel::PowerPC::read_msr() != before)
            return 0;
        Kernel::SmapDisabler inner;
        if (Kernel::PowerPC::read_msr() != before)
            return 0;
    }
    return Kernel::PowerPC::read_msr() == before;
}
