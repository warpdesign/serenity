/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <cxxabi.h>
#include <stdlib.h>
#include <string.h>

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

    int demangle_status = -1;
    char* demangled_name = abi::__cxa_demangle("_Z3foov", nullptr, nullptr, &demangle_status);
    if (demangle_status != 0 || !demangled_name || strcmp(demangled_name, "foo()") != 0)
        __builtin_trap();
    free(demangled_name);

    constexpr char message[] = "PowerPC RTTI and demangle runtime passed\n";
    if (syscall2(SYSCALL_DBGPUTSTR, reinterpret_cast<unsigned>(message), sizeof(message) - 1) != sizeof(message) - 1)
        __builtin_trap();

    for (;;)
        asm volatile("" ::: "memory");
}
