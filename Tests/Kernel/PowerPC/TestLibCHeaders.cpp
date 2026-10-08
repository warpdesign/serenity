/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <setjmp.h>
#include <sys/arch/regs.h>
#include <ucontext.h>

static_assert(sizeof(void*) == 4);
static_assert(sizeof(mcontext_t) == 412);
static_assert(sizeof(PtraceRegisters) == sizeof(mcontext_t));
static_assert(sizeof(jmp_buf) == 248);
