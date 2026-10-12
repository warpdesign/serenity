/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static volatile uint64_t value;

void _start(void)
{
    if (__atomic_is_lock_free(sizeof(value), &value))
        exit(1);

    __atomic_store_n(&value, 0x1020304050607080ull, __ATOMIC_SEQ_CST);
    if (__atomic_load_n(&value, __ATOMIC_SEQ_CST) != 0x1020304050607080ull)
        exit(2);
    if (__atomic_exchange_n(&value, 4, __ATOMIC_SEQ_CST) != 0x1020304050607080ull)
        exit(3);
    if (__atomic_fetch_add(&value, 3, __ATOMIC_SEQ_CST) != 4)
        exit(4);
    if (__atomic_fetch_sub(&value, 2, __ATOMIC_SEQ_CST) != 7)
        exit(5);
    if (__atomic_fetch_or(&value, 0x10, __ATOMIC_SEQ_CST) != 5)
        exit(6);
    if (__atomic_fetch_and(&value, 0x10, __ATOMIC_SEQ_CST) != 0x15)
        exit(7);
    if (__atomic_fetch_xor(&value, 0x20, __ATOMIC_SEQ_CST) != 0x10)
        exit(8);
    uint64_t expected = 0;
    if (__atomic_compare_exchange_n(&value, &expected, 99, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        exit(9);
    if (expected != 0x30 || !__atomic_compare_exchange_n(&value, &expected, 99, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
        exit(10);
    if (__atomic_load_n(&value, __ATOMIC_SEQ_CST) != 99)
        exit(11);

    puts("atomic64=ok");
    exit(0);
}
