/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace WindowServer {

// Collect several frames before reporting so serial logging does not dominate
// the operation being measured.
class CompositionProfileStats {
public:
    struct Report {
        u64 frames { 0 };
        u64 composition_us { 0 };
        u64 upload_us { 0 };
        u64 upload_bytes { 0 };
        u64 longest_frame_us { 0 };
        u64 longest_upload_us { 0 };
    };

    void record(u64 now_us, u64 composition_us, u64 upload_us, u64 upload_bytes)
    {
        if (m_interval_start_us == 0)
            m_interval_start_us = now_us;
        ++m_report.frames;
        m_report.composition_us += composition_us;
        m_report.upload_us += upload_us;
        m_report.upload_bytes += upload_bytes;
        if (composition_us + upload_us > m_report.longest_frame_us)
            m_report.longest_frame_us = composition_us + upload_us;
        if (upload_us > m_report.longest_upload_us)
            m_report.longest_upload_us = upload_us;
    }

    bool take_report_if_due(u64 now_us, Report& report)
    {
        if (m_report.frames == 0 || now_us - m_interval_start_us < 2'000'000)
            return false;
        report = m_report;
        m_report = {};
        m_interval_start_us = now_us;
        return true;
    }

private:
    Report m_report;
    u64 m_interval_start_us { 0 };
};

}
