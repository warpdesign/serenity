/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/FileSystem/Ext2FS/BackupWritePolicy.h>
#include <cerrno>
#include <cstdlib>
#include <vector>

extern "C" void ak_verification_failed(char const*)
{
    std::abort();
}

using Kernel::Ext2BackupWritePolicy;
using Kernel::for_each_ext2_backup_group;

int main(int argc, char**)
{
    std::vector<unsigned> groups;
    auto record = [&](unsigned group) -> ErrorOr<void> {
        groups.push_back(group);
        return {};
    };
    if (for_each_ext2_backup_group(8, true, Ext2BackupWritePolicy::PrimaryOnly, record).is_error() || !groups.empty())
        return 1;
    if (for_each_ext2_backup_group(7, false, Ext2BackupWritePolicy::PrimaryOnly, record).is_error() || !groups.empty())
        return 1;
    if (for_each_ext2_backup_group(8, true, Ext2BackupWritePolicy::UpdateBackups, record).is_error()
        || groups != std::vector<unsigned> { 2, 4, 6, 8 })
        return 1;

    if (argc > 1) {
        groups.clear();
        if (for_each_ext2_backup_group(7, false, Ext2BackupWritePolicy::UpdateBackups, record).is_error()
            || groups != std::vector<unsigned> { 2, 3, 4, 5, 6, 7 })
            return 1;
        groups.clear();
        auto failed = for_each_ext2_backup_group(8, true, Ext2BackupWritePolicy::UpdateBackups,
            [&](unsigned group) -> ErrorOr<void> {
                groups.push_back(group);
                if (group == 4)
                    return Error::from_errno(EIO);
                return {};
            });
        if (!failed.is_error() || groups != std::vector<unsigned> { 2, 4 })
            return 1;
    }
    return 0;
}
