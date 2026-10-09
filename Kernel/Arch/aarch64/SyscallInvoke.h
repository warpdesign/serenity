/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel::Syscall {

inline uintptr_t invoke(Function function)
{
    uintptr_t result;
    register uintptr_t x0 asm("x0");
    register uintptr_t x8 asm("x8") = function;
    asm volatile("svc #0"
        : "=r"(x0)
        : "r"(x8)
        : "memory");
    result = x0;
    return result;
}

template<typename T1>
inline uintptr_t invoke(Function function, T1 arg1)
{
    uintptr_t result;
    register uintptr_t x0 asm("x0");
    register uintptr_t x1 asm("x1") = arg1;
    register uintptr_t x8 asm("x8") = function;
    asm volatile("svc #0"
        : "=r"(x0)
        : "r"(x1), "r"(x8)
        : "memory");
    result = x0;
    return result;
}

template<typename T1, typename T2>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2)
{
    uintptr_t result;
    register uintptr_t x0 asm("x0");
    register uintptr_t x1 asm("x1") = arg1;
    register uintptr_t x2 asm("x2") = arg2;
    register uintptr_t x8 asm("x8") = function;
    asm volatile("svc #0"
        : "=r"(x0)
        : "r"(x1), "r"(x2), "r"(x8)
        : "memory");
    result = x0;
    return result;
}

template<typename T1, typename T2, typename T3>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2, T3 arg3)
{
    uintptr_t result;
    register uintptr_t x0 asm("x0");
    register uintptr_t x1 asm("x1") = arg1;
    register uintptr_t x2 asm("x2") = arg2;
    register uintptr_t x3 asm("x3") = arg3;
    register uintptr_t x8 asm("x8") = function;
    asm volatile("svc #0"
        : "=r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x8)
        : "memory");
    result = x0;
    return result;
}

template<typename T1, typename T2, typename T3, typename T4>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2, T3 arg3, T4 arg4)
{
    uintptr_t result;
    register uintptr_t x0 asm("x0");
    register uintptr_t x1 asm("x1") = arg1;
    register uintptr_t x2 asm("x2") = arg2;
    register uintptr_t x3 asm("x3") = arg3;
    register uintptr_t x4 asm("x4") = arg4;
    register uintptr_t x8 asm("x8") = function;
    asm volatile("svc #0"
        : "=r"(x0)
        : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x8)
        : "memory");
    result = x0;
    return result;
}

}
