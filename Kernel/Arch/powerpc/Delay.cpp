/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Delay.h>
#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Arch/powerpc/Time/Timebase.h>
#include <Kernel/Boot/BootInfo.h>

namespace Kernel {

void microseconds_delay(u32 microseconds)
{
    auto frequency = PowerPC::boot_timebase_frequency(g_boot_info);
    VERIFY(frequency.has_value());
    PowerPC::delay_timebase_ticks(PowerPC::timebase_ticks_for_microseconds(microseconds, frequency.value()));
}

}
