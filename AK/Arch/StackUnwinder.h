/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>
#include <AK/Types.h>

#if ARCH(AARCH64)
#    include <AK/Arch/aarch64/StackUnwinder.h>
#elif ARCH(X86_64)
#    include <AK/Arch/x86_64/StackUnwinder.h>
#elif ARCH(RISCV64)
#    include <AK/Arch/riscv64/StackUnwinder.h>
#elif ARCH(POWERPC32)
#    include <AK/Arch/powerpc/StackUnwinder.h>
#else
#    error Unsupported stack unwinder architecture
#endif
