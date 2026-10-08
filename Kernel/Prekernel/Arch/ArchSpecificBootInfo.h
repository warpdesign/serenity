/*
 * Copyright (c) 2024-2026, Sönke Holz <soenke.holz@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Platform.h>

#if ARCH(X86_64)
#    include <Kernel/Prekernel/Arch/x86_64/ArchSpecificBootInfo.h>
#elif ARCH(AARCH64)
#    include <Kernel/Prekernel/Arch/aarch64/ArchSpecificBootInfo.h>
#elif ARCH(RISCV64)
#    include <Kernel/Prekernel/Arch/riscv64/ArchSpecificBootInfo.h>
#elif ARCH(POWERPC32)
#    include <Kernel/Prekernel/Arch/powerpc/ArchSpecificBootInfo.h>
#else
#    error Unknown architecture
#endif
