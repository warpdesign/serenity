/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Devices/Storage/IDE/WriteCacheState.h>

using Kernel::IDEWriteCacheState;

int main(int argc, char**)
{
    IDEWriteCacheState state;
    unsigned flush_commands = 0;
    auto sync = [&] {
        if (!state.needs_flush())
            return;
        ++flush_commands;
        state.flush_completed();
    };

    sync();
    if (flush_commands != 0)
        return 1;

    for (unsigned block = 0; block < 16; ++block)
        state.write_completed();
    sync();
    sync();
    if (flush_commands != 1 || state.needs_flush() || state.pending_writes() != 0)
        return 1;

    if (argc > 1) {
        state.write_completed();
        // A failed device flush must leave the write pending for retry.
        ++flush_commands;
        if (!state.needs_flush() || state.pending_writes() != 1)
            return 1;
        sync();
        if (flush_commands != 3 || state.needs_flush())
            return 1;
    }
    return 0;
}
