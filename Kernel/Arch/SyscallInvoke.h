/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

// Included from Kernel/API/Syscall.h after the syscall types are declared.
#if ARCH(X86_64)
#    include <Kernel/Arch/x86_64/SyscallInvoke.h>
#elif ARCH(AARCH64)
#    include <Kernel/Arch/aarch64/SyscallInvoke.h>
#elif ARCH(RISCV64)
#    include <Kernel/Arch/riscv64/SyscallInvoke.h>
#elif ARCH(POWERPC32)
#    include <Kernel/Arch/powerpc/SyscallInvoke.h>
#else
#    error Unsupported syscall architecture
#endif
