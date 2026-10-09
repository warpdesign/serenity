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
    asm volatile("syscall"
        : "=a"(result)
        : "a"(function)
        : "rcx", "r11", "memory");
    return result;
}

template<typename T1>
inline uintptr_t invoke(Function function, T1 arg1)
{
    uintptr_t result;
    asm volatile("syscall"
        : "=a"(result)
        : "a"(function), "d"((uintptr_t)arg1)
        : "rcx", "r11", "memory");
    return result;
}

template<typename T1, typename T2>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2)
{
    uintptr_t result;
    asm volatile("syscall"
        : "=a"(result)
        : "a"(function), "d"((uintptr_t)arg1), "D"((uintptr_t)arg2)
        : "rcx", "r11", "memory");
    return result;
}

template<typename T1, typename T2, typename T3>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2, T3 arg3)
{
    uintptr_t result;
    asm volatile("syscall"
        : "=a"(result)
        : "a"(function), "d"((uintptr_t)arg1), "D"((uintptr_t)arg2), "b"((uintptr_t)arg3)
        : "rcx", "r11", "memory");
    return result;
}

template<typename T1, typename T2, typename T3, typename T4>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2, T3 arg3, T4 arg4)
{
    uintptr_t result;
    asm volatile("syscall"
        : "=a"(result)
        : "a"(function), "d"((uintptr_t)arg1), "D"((uintptr_t)arg2), "b"((uintptr_t)arg3), "S"((uintptr_t)arg4)
        : "rcx", "r11", "memory");
    return result;
}

}
