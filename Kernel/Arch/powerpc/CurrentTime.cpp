/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/CurrentTime.h>

namespace Kernel {

fptr optional_current_time()
{
    // The timebase is monotonic. Wall time still comes from TimeManagement's
    // configured clock source rather than treating timebase zero as an epoch.
    return nullptr;
}

}
