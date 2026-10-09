/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel::Syscall {

inline uintptr_t invoke(Function function)
{
    register uintptr_t a7 asm("a7") = function;
    register uintptr_t result asm("a0");
    asm volatile("ecall"
        : "=r"(result)
        : "r"(a7)
        : "memory");
    return result;
}

template<typename T1>
inline uintptr_t invoke(Function function, T1 arg1)
{
    register uintptr_t a0 asm("a0") = arg1;
    register uintptr_t a7 asm("a7") = function;
    register uintptr_t result asm("a0");
    asm volatile("ecall"
        : "=r"(result)
        : "0"(a0), "r"(a7)
        : "memory");
    return result;
}

template<typename T1, typename T2>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2)
{
    register uintptr_t a0 asm("a0") = arg1;
    register uintptr_t a1 asm("a1") = arg2;
    register uintptr_t a7 asm("a7") = function;
    register uintptr_t result asm("a0");
    asm volatile("ecall"
        : "=r"(result)
        : "0"(a0), "r"(a1), "r"(a7)
        : "memory");
    return result;
}

template<typename T1, typename T2, typename T3>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2, T3 arg3)
{
    register uintptr_t a0 asm("a0") = arg1;
    register uintptr_t a1 asm("a1") = arg2;
    register uintptr_t a2 asm("a2") = arg3;
    register uintptr_t a7 asm("a7") = function;
    register uintptr_t result asm("a0");
    asm volatile("ecall"
        : "=r"(result)
        : "0"(a0), "r"(a1), "r"(a2), "r"(a7)
        : "memory");
    return result;
}

template<typename T1, typename T2, typename T3, typename T4>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2, T3 arg3, T4 arg4)
{
    register uintptr_t a0 asm("a0") = arg1;
    register uintptr_t a1 asm("a1") = arg2;
    register uintptr_t a2 asm("a2") = arg3;
    register uintptr_t a3 asm("a3") = arg4;
    register uintptr_t a7 asm("a7") = function;
    register uintptr_t result asm("a0");
    asm volatile("ecall"
        : "=r"(result)
        : "0"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a7)
        : "memory");
    return result;
}

}
