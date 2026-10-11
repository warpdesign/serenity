/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Error.h>

namespace Kernel {

class KBufferBuilder;

ErrorOr<void> try_generate_cpu_info(KBufferBuilder&);

}
