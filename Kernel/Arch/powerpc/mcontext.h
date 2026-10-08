/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/API/POSIX/sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct __attribute__((packed)) __mcontext {
    uint32_t gpr[32];
    uint64_t fpr[32];
    uint32_t pc;
    uint32_t msr;
    uint32_t lr;
    uint32_t ctr;
    uint32_t cr;
    uint32_t xer;
    uint32_t fpscr;
};

#ifdef __cplusplus
}
#endif
