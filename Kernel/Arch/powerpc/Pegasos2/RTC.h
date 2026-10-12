/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Time.h>

namespace Kernel::PowerPC::Pegasos2::RTC {

void initialize();
UnixDateTime boot_time();

}
