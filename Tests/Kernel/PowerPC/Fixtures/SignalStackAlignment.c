/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <signal.h>
#include <unistd.h>

extern unsigned syscall2(unsigned, unsigned, unsigned);

static volatile sig_atomic_t signal_received;

static void on_signal(int signal_number)
{
    signal_received = signal_number;
}

void _start(void)
{
    struct sigaction action = { 0 };
    action.sa_handler = on_signal;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGUSR1, &action, 0) != 0)
        __builtin_trap();

    if (raise(SIGUSR1) != 0)
        __builtin_trap();
    if (signal_received != SIGUSR1)
        __builtin_trap();

    static char const message[] = "PowerPC signal handler ran\n";
    if (syscall2(SYSCALL_DBGPUTSTR, (unsigned)message, sizeof(message) - 1) != sizeof(message) - 1)
        __builtin_trap();
    for (;;)
        __asm__ volatile("" ::: "memory");
}
