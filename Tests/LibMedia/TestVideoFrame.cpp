/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/NonnullOwnPtr.h>
#include <LibMedia/VideoFrame.h>
#include <LibTest/TestCase.h>
#include <errno.h>

TEST_CASE(subsampled_frame_preserves_plane_data)
{
    u8 y[] { 1, 2, 3, 4 };
    u8 u[] { 5, 6, 7, 8 };
    u8 v[] { 9, 10, 11, 12 };
    auto frame_or_error = Media::SubsampledYUVFrame::try_create_from_data(
        Duration::zero(), { 2, 2 }, 8, {}, {},
        { y, sizeof(y) }, { u, sizeof(u) }, { v, sizeof(v) });
    VERIFY(!frame_or_error.is_error());
    auto frame = frame_or_error.release_value();
    for (size_t i = 0; i < 4; ++i) {
        EXPECT_EQ(frame->get_raw_plane_data(0)[i], y[i]);
        EXPECT_EQ(frame->get_raw_plane_data(1)[i], u[i]);
        EXPECT_EQ(frame->get_raw_plane_data(2)[i], v[i]);
    }
}

TEST_CASE(subsampled_frame_rejects_overflowing_plane_size)
{
    auto frame_or_error = Media::SubsampledYUVFrame::try_create(
        Duration::zero(), { 0xffffffffu, 0xffffffffu }, 9, {}, {});
    EXPECT(frame_or_error.is_error());
    if (frame_or_error.is_error())
        EXPECT_EQ(frame_or_error.error().code(), EOVERFLOW);
}
