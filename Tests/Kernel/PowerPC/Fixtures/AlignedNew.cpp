/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <new>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct alignas(64) Overaligned {
    int value;
};

extern "C" void _start()
{
    auto* single = new Overaligned { 42 };
    volatile uintptr_t single_address = reinterpret_cast<uintptr_t>(single);
    if (single_address % alignof(Overaligned) != 0 || single->value != 42)
        exit(1);
    delete single;

    auto* array = new Overaligned[2];
    volatile uintptr_t array_address = reinterpret_cast<uintptr_t>(array);
    if (array_address % alignof(Overaligned) != 0)
        exit(2);
    array[1].value = 99;
    if (array[1].value != 99)
        exit(3);
    delete[] array;

    auto* nothrow = new (std::nothrow) Overaligned { 7 };
    if (!nothrow)
        exit(4);
    volatile uintptr_t nothrow_address = reinterpret_cast<uintptr_t>(nothrow);
    if (nothrow_address % alignof(Overaligned) != 0 || nothrow->value != 7)
        exit(5);
    delete nothrow;

    puts("aligned-new=ok");
    exit(0);
}
