/*
 * Copyright (c) 2020-2026, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/FileSystem/FileSystem.h>
#include <Kernel/Sections.h>
#include <Kernel/Tasks/Process.h>
#include <Kernel/Tasks/SyncTask.h>
#include <Kernel/Time/TimeManagement.h>

namespace Kernel {

UNMAP_AFTER_INIT void SyncTask::spawn()
{
    MUST(Process::create_kernel_process("VFS Sync Task"sv, [] {
        dbgln("VFS SyncTask is running");
        while (!Process::current().is_dying()) {
            auto started = TimeManagement::the().current_time(CLOCK_MONOTONIC);
            FileSystem::sync();
            auto elapsed = TimeManagement::the().current_time(CLOCK_MONOTONIC) - started;
            if (elapsed >= Duration::from_milliseconds(250))
                dbgln("VFS SyncTask: sync took {} ms", elapsed.to_milliseconds());
            (void)Thread::current()->sleep(Duration::from_seconds(1));
        }
        Process::current().sys$exit(0);
        VERIFY_NOT_REACHED();
    }));
}

}
