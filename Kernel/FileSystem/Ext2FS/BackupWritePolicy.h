/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>
#include <AK/IntegralMath.h>

namespace Kernel {

enum class Ext2BackupWritePolicy {
    PrimaryOnly,
    UpdateBackups,
};

template<typename Callback>
ErrorOr<void> for_each_ext2_backup_group(unsigned group_count, bool sparse_superblock, Ext2BackupWritePolicy policy, Callback callback)
{
    if (policy == Ext2BackupWritePolicy::PrimaryOnly)
        return {};
    for (unsigned group = 2; group <= group_count; ++group) {
        if (!sparse_superblock || group == 2 || AK::is_power_of<3>(group - 1)
            || AK::is_power_of<5>(group - 1) || AK::is_power_of<7>(group - 1))
            TRY(callback(group));
    }
    return {};
}

}
