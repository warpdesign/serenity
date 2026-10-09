/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <stdint.h>
#include <sys/cdefs.h>

#if (!defined(__powerpc__) && !defined(_ARCH_PPC)) || __SIZEOF_POINTER__ != 4
#    error This floating point environment requires PowerPC32
#endif

__BEGIN_DECLS

typedef struct fenv_t {
    uint32_t fpscr;
} fenv_t;

__END_DECLS
