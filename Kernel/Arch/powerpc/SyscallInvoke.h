/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel::Syscall {

// r0 carries the syscall number, r3-r6 carry up to four arguments, and r3
// carries the return value.
inline uintptr_t invoke(Function function)
{
    register uintptr_t number asm("r0") = static_cast<uintptr_t>(function);
    register uintptr_t result asm("r3");
    asm volatile("sc" : "=r"(result) : "r"(number) : "cr0", "memory");
    return result;
}

template<typename T1>
inline uintptr_t invoke(Function function, T1 arg1)
{
    register uintptr_t number asm("r0") = static_cast<uintptr_t>(function);
    register uintptr_t result asm("r3") = static_cast<uintptr_t>(arg1);
    asm volatile("sc" : "+r"(result) : "r"(number) : "cr0", "memory");
    return result;
}

template<typename T1, typename T2>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2)
{
    register uintptr_t number asm("r0") = static_cast<uintptr_t>(function);
    register uintptr_t result asm("r3") = static_cast<uintptr_t>(arg1);
    register uintptr_t r4 asm("r4") = static_cast<uintptr_t>(arg2);
    asm volatile("sc" : "+r"(result) : "r"(number), "r"(r4) : "cr0", "memory");
    return result;
}

template<typename T1, typename T2, typename T3>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2, T3 arg3)
{
    register uintptr_t number asm("r0") = static_cast<uintptr_t>(function);
    register uintptr_t result asm("r3") = static_cast<uintptr_t>(arg1);
    register uintptr_t r4 asm("r4") = static_cast<uintptr_t>(arg2);
    register uintptr_t r5 asm("r5") = static_cast<uintptr_t>(arg3);
    asm volatile("sc" : "+r"(result) : "r"(number), "r"(r4), "r"(r5) : "cr0", "memory");
    return result;
}

template<typename T1, typename T2, typename T3, typename T4>
inline uintptr_t invoke(Function function, T1 arg1, T2 arg2, T3 arg3, T4 arg4)
{
    register uintptr_t number asm("r0") = static_cast<uintptr_t>(function);
    register uintptr_t result asm("r3") = static_cast<uintptr_t>(arg1);
    register uintptr_t r4 asm("r4") = static_cast<uintptr_t>(arg2);
    register uintptr_t r5 asm("r5") = static_cast<uintptr_t>(arg3);
    register uintptr_t r6 asm("r6") = static_cast<uintptr_t>(arg4);
    asm volatile("sc" : "+r"(result) : "r"(number), "r"(r4), "r"(r5), "r"(r6) : "cr0", "memory");
    return result;
}

}
