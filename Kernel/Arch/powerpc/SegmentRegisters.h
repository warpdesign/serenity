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

// User address spaces use a 16-VSID block. While executing in the kernel,
// segments 12-15 retain the shared kernel mapping. Before returning to user
// mode, switch those segments to the unused high VSIDs in the user block.
int powerpc_user_vsid_base_valid(PowerPCSegmentWord base);
int powerpc_activate_user_segments(PowerPCSegmentWord base);
int powerpc_activate_user_high_segments(PowerPCSegmentWord base);
void powerpc_activate_kernel_high_segments(void);
void powerpc_activate_kernel_segments(void);
PowerPCSegmentWord powerpc_read_segment_register(PowerPCSegmentWord segment);
void powerpc_write_segment_register(PowerPCSegmentWord segment, PowerPCSegmentWord value);

#ifdef __cplusplus
}
#endif
