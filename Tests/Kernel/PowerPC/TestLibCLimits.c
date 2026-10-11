/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <limits.h>

_Static_assert(sizeof(long) == 4, "PowerPC32 long must be 32 bits");
_Static_assert(LONG_MAX == 2147483647L, "LONG_MAX must match the ABI");
_Static_assert(LONG_MIN == (-2147483647L - 1L), "LONG_MIN must match the ABI");
_Static_assert(ULONG_MAX == 4294967295UL, "ULONG_MAX must match the ABI");
_Static_assert(LONG_WIDTH == 32 && ULONG_WIDTH == 32, "long width must match the ABI");
_Static_assert(SSIZE_MAX == LONG_MAX, "ssize_t must fit in long");
