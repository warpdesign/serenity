/*
 * Copyright (c) 2023-2026, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>

#if ARCH(X86_64)
#    include <Kernel/Arch/x86_64/CPUID.h>
#elif ARCH(AARCH64)
#    include <Kernel/Arch/aarch64/CPUID.h>
#elif ARCH(RISCV64)
#    include <Kernel/Arch/riscv64/CPUID.h>
#elif ARCH(POWERPC32)
#    include <Kernel/Arch/powerpc/CPUID.h>
#else
#    error "Unknown architecture"
#endif
