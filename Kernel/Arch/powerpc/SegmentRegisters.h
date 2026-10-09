/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int PowerPCSegmentWord;

enum {
    POWERPC_USER_SEGMENT_COUNT = 12,
};

// User address spaces use a 16-VSID block. Segments 12-15 retain the shared
// kernel mapping, while segments 0-11 receive base + segment number.
int powerpc_user_vsid_base_valid(PowerPCSegmentWord base);
int powerpc_activate_user_segments(PowerPCSegmentWord base);
void powerpc_activate_kernel_segments(void);
PowerPCSegmentWord powerpc_read_segment_register(PowerPCSegmentWord segment);
void powerpc_write_segment_register(PowerPCSegmentWord segment, PowerPCSegmentWord value);

#ifdef __cplusplus
}
#endif
