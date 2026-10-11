/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

struct Base {
    virtual int kind() const { return 1; }
};

struct Derived final : Base {
    int kind() const override { return 2; }
};

extern "C" unsigned syscall2(unsigned, unsigned, unsigned);

extern "C" [[noreturn]] void _start()
{
    Derived object;
    Base* volatile selected = &object;
    if (dynamic_cast<Derived*>(selected) != &object)
        __builtin_trap();

    constexpr char message[] = "PowerPC RTTI runtime passed\n";
    if (syscall2(SYSCALL_DBGPUTSTR, reinterpret_cast<unsigned>(message), sizeof(message) - 1) != sizeof(message) - 1)
        __builtin_trap();

    for (;;)
        asm volatile("" ::: "memory");
}
