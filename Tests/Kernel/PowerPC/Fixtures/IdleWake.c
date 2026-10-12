/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <time.h>

extern unsigned syscall2(unsigned, unsigned, unsigned);

static void report(char const* message, unsigned size)
{
    if (syscall2(SYSCALL_DBGPUTSTR, (unsigned)message, size) != size)
        __builtin_trap();
}

void _start(void)
{
    static char const started[] = "PowerPC idle interval begin\n";
    static char const ended[] = "PowerPC idle interval ended\n";
    report(started, sizeof(started) - 1);

    struct timespec delay = { 3, 0 };
    if (nanosleep(&delay, 0) != 0)
        __builtin_trap();

    report(ended, sizeof(ended) - 1);
    for (;;)
        __asm__ volatile("" ::: "memory");
}
