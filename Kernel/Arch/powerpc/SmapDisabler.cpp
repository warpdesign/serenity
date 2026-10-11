/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/SmapDisabler.h>

namespace Kernel {

SmapDisabler::SmapDisabler()
    : m_flags(0)
{
}

SmapDisabler::~SmapDisabler() = default;

}
