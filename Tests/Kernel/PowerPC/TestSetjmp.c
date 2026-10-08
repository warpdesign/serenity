/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <setjmp.h>

static sigset_t s_current_mask = 0x5a5aa5a5;
static unsigned s_sigprocmask_call_count;

extern int powerpc_verify_preserved_registers(void);

int sigprocmask(int how, sigset_t const* set, sigset_t* old_set)
{
    (void)how;
    if (old_set)
        *old_set = s_current_mask;
    if (set)
        s_current_mask = *set;
    ++s_sigprocmask_call_count;
    return 0;
}

int powerpc_setjmp_self_test(void)
{
    jmp_buf buffer;

    int result = setjmp(buffer);
    if (result == 0) {
        if (buffer[0].did_save_signal_mask != 0 || buffer[0].saved_signal_mask != 0)
            return 0;
        longjmp(buffer, 0);
    }
    if (result != 1)
        return 0;

    result = sigsetjmp(buffer, 1);
    if (result == 0) {
        if (buffer[0].did_save_signal_mask != 1 || buffer[0].saved_signal_mask != s_current_mask || s_sigprocmask_call_count != 1)
            return 0;
        longjmp(buffer, 7);
    }
    if (result != 7)
        return 0;

    result = _setjmp(buffer);
    if (result == 0)
        _longjmp(buffer, 9);
    return result == 9 && s_sigprocmask_call_count == 1 && powerpc_verify_preserved_registers();
}
