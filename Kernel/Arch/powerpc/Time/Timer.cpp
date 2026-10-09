/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Arch/powerpc/Time/Timebase.h>
#include <Kernel/Arch/powerpc/Time/Timer.h>
#include <Kernel/Boot/BootInfo.h>

namespace Kernel::PowerPC {

static Timer* s_the;

Timer::Timer(u32 timebase_frequency)
    : m_timebase_frequency(timebase_frequency)
    , m_interrupt_interval(decrementer_interval(timebase_frequency, default_decrementer_interrupts_per_second))
    , m_last_timebase_read(read_timebase())
{
    VERIFY(m_interrupt_interval);
    set_decrementer(m_interrupt_interval);
}

NonnullLockRefPtr<Timer> Timer::initialize()
{
    VERIFY(!s_the);
    auto frequency = boot_timebase_frequency(g_boot_info);
    VERIFY(frequency.has_value());
    auto timer = adopt_lock_ref(*new Timer(frequency.value()));
    s_the = timer.ptr();
    return timer;
}

Timer& Timer::the()
{
    VERIFY(s_the);
    return *s_the;
}

void Timer::handle_interrupt()
{
    // Rearm before the callback, which may schedule another thread.
    set_decrementer(m_enabled ? m_interrupt_interval : 0x7fffffff);
    if (m_enabled && m_callback)
        m_callback();
}

void Timer::disable()
{
    // The 750 has no DEC interrupt mask separate from MSR[EE]. Keep device
    // interrupts available and move the next DEC crossing far into the future.
    m_enabled = false;
    set_decrementer(0x7fffffff);
}

u64 Timer::update_time(u64& seconds_since_boot, u32& ticks_this_second, bool query_only)
{
    auto current_ticks = read_timebase();
    auto elapsed_ticks = current_ticks - m_last_timebase_read;
    auto ticks_since_last_second = static_cast<u64>(ticks_this_second) + elapsed_ticks;
    seconds_since_boot += ticks_since_last_second / m_timebase_frequency;
    ticks_this_second = ticks_since_last_second % m_timebase_frequency;

    if (!query_only)
        m_last_timebase_read = current_ticks;

    return timebase_ticks_to_nanoseconds(elapsed_ticks, m_timebase_frequency);
}

}
