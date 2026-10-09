/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Library/NonnullLockRefPtr.h>
#include <Kernel/Time/HardwareTimer.h>

namespace Kernel::PowerPC {

class Timer final : public HardwareTimerBase {
public:
    static NonnullLockRefPtr<Timer> initialize();
    static Timer& the();

    virtual HardwareTimerType timer_type() const override { return HardwareTimerType::PowerPCDecrementer; }
    virtual StringView model() const override { return "PowerPC decrementer"sv; }
    virtual size_t ticks_per_second() const override { return m_timebase_frequency; }

    virtual bool is_periodic() const override { return false; }
    virtual bool is_periodic_capable() const override { return false; }
    virtual void set_periodic() override { }
    virtual void set_non_periodic() override { }
    virtual void disable() override;

    virtual void reset_to_default_ticks_per_second() override { }
    virtual bool try_to_set_frequency(size_t frequency) override { return frequency == m_timebase_frequency; }
    virtual bool is_capable_of_frequency(size_t frequency) const override { return frequency == m_timebase_frequency; }
    virtual size_t calculate_nearest_possible_frequency(size_t) const override { return m_timebase_frequency; }

    virtual void will_be_destroyed() override { }
    virtual Function<void()> set_callback(Function<void()> callback) override
    {
        auto previous_callback = move(m_callback);
        m_callback = move(callback);
        return previous_callback;
    }

    u64 update_time(u64& seconds_since_boot, u32& ticks_this_second, bool query_only);
    void handle_interrupt();

private:
    explicit Timer(u32 timebase_frequency);

    Function<void()> m_callback;
    u32 m_timebase_frequency { 0 };
    u32 m_interrupt_interval { 0 };
    u64 m_last_timebase_read { 0 };
    bool m_enabled { true };
};

}
