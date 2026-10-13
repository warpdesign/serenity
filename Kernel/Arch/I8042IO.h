/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>

#if ARCH(X86_64)
#    include <Kernel/Arch/x86_64/ISABus/I8042IO.h>
#elif ARCH(POWERPC32)
#    include <Kernel/Arch/powerpc/Pegasos2/I8042IO.h>
#else
#    error "Unknown architecture"
#endif
