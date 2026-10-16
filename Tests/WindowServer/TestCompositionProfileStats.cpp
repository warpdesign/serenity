/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Userland/Services/WindowServer/CompositionProfileStats.h>

using WindowServer::CompositionProfileStats;

int main(int argc, char**)
{
    CompositionProfileStats stats;
    CompositionProfileStats::Report report;
    stats.record(1'000'000, 100, 400, 4096);
    stats.record(1'050'000, 60, 20, 256);
    if (stats.take_report_if_due(2'999'999, report))
        return 1;
    if (!stats.take_report_if_due(3'000'000, report))
        return 2;
    if (report.frames != 2 || report.composition_us != 160 || report.upload_us != 420
        || report.upload_bytes != 4352 || report.longest_frame_us != 500 || report.longest_upload_us != 400)
        return 3;
    if (stats.take_report_if_due(3'000'001, report))
        return 4;

    if (argc > 1) {
        stats.record(3'100'000, 5, 10, 128);
        if (!stats.take_report_if_due(5'000'000, report))
            return 5;
        if (report.frames != 1 || report.composition_us != 5 || report.upload_us != 10
            || report.upload_bytes != 128 || report.longest_frame_us != 15)
            return 6;
    }
    return 0;
}
