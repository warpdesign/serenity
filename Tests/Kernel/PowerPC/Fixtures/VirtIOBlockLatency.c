/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <fcntl.h>
#include <unistd.h>

extern unsigned syscall2(unsigned, unsigned, unsigned);

static void report(char const* message, unsigned size)
{
    if (syscall2(SYSCALL_DBGPUTSTR, (unsigned)message, size) != size)
        __builtin_trap();
}

void _start(void)
{
    int fd = open("/data", O_RDONLY);
    if (fd < 0)
        __builtin_trap();

    static char const started[] = "PowerPC block reads begin\n";
    static char const ended[] = "PowerPC block reads ended\n";
    report(started, sizeof(started) - 1);

    for (unsigned index = 0; index < 97; ++index) {
        unsigned block = index * 37 % 97;
        unsigned char value = 0;
        if (pread(fd, &value, 1, (off_t)block * 4096) != 1 || value != block)
            __builtin_trap();
    }

    report(ended, sizeof(ended) - 1);
    for (;;)
        __asm__ volatile("" ::: "memory");
}
