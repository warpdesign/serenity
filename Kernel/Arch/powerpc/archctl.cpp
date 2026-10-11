/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Tasks/Process.h>

namespace Kernel {

ErrorOr<FlatPtr> Process::sys$archctl(int, FlatPtr, FlatPtr, FlatPtr)
{
    VERIFY_NO_PROCESS_BIG_LOCK(this);
    return ENOSYS;
}

}
