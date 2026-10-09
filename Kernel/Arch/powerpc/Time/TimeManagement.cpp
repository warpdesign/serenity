/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Time/Timebase.h>
#include <Kernel/Arch/powerpc/Time/Timer.h>
#include <Kernel/Sections.h>
#include <Kernel/Time/TimeManagement.h>

namespace Kernel {

UNMAP_AFTER_INIT bool TimeManagement::probe_and_set_powerpc_hardware_timers()
{
    static_assert(PowerPC::default_decrementer_interrupts_per_second == OPTIMAL_TICKS_PER_SECOND_RATE);
    m_system_timer = PowerPC::Timer::initialize();
    m_time_ticks_per_second = m_system_timer->ticks_per_second();
    m_read_time_from_hardware = [this](auto&&... args) {
        return static_cast<PowerPC::Timer*>(m_system_timer.ptr())->update_time(args...);
    };
    m_system_timer->set_callback([this]() { system_timer_callback(); });
    m_can_query_precise_time.set();
    m_time_keeper_timer = m_system_timer;
    return true;
}

}
