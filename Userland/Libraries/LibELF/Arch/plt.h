/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>

#if ARCH(AARCH64)
#    include <LibELF/Arch/aarch64/plt.h>
#elif ARCH(RISCV64)
#    include <LibELF/Arch/riscv64/plt.h>
#elif ARCH(X86_64)
#    include <LibELF/Arch/x86_64/plt.h>
#elif ARCH(POWERPC32)
#    include <LibELF/Arch/powerpc/plt.h>
#else
#    error Unknown architecture
#endif
