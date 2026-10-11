/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Arch/SafeMem.h>

#define SAFEMEM_ATOMIC_SECTION __attribute__((section(".text.safemem.atomic")))

extern "C" u8 start_of_safemem_text[];
extern "C" u8 end_of_safemem_text[];
extern "C" u8 start_of_safemem_atomic_text[];
extern "C" u8 end_of_safemem_atomic_text[];

extern "C" u8 safe_memcpy_ins_1[];
extern "C" u8 safe_memcpy_ins_2[];
extern "C" u8 safe_memcpy_faulted[];
extern "C" u8 safe_memset_ins[];
extern "C" u8 safe_memset_faulted[];
extern "C" u8 safe_strnlen_ins[];
extern "C" u8 safe_strnlen_faulted[];

extern "C" u8 safe_atomic_load_relaxed_ins[];
extern "C" u8 safe_atomic_load_relaxed_faulted[];
extern "C" u8 safe_atomic_store_relaxed_ins[];
extern "C" u8 safe_atomic_store_relaxed_faulted[];
extern "C" u8 safe_atomic_fetch_add_relaxed_ins_1[];
extern "C" u8 safe_atomic_fetch_add_relaxed_ins_2[];
extern "C" u8 safe_atomic_fetch_add_relaxed_faulted[];
extern "C" u8 safe_atomic_exchange_relaxed_ins_1[];
extern "C" u8 safe_atomic_exchange_relaxed_ins_2[];
extern "C" u8 safe_atomic_exchange_relaxed_faulted[];
extern "C" u8 safe_atomic_compare_exchange_relaxed_ins_1[];
extern "C" u8 safe_atomic_compare_exchange_relaxed_ins_2[];
extern "C" u8 safe_atomic_compare_exchange_relaxed_faulted[];

namespace Kernel {

extern "C" bool powerpc_safe_memcpy(void*, void const*, size_t, void*&);
extern "C" bool powerpc_safe_memset(void*, int, size_t, void*&);
extern "C" ssize_t powerpc_safe_strnlen(char const*, size_t, void*&);

NEVER_INLINE bool safe_memcpy(void* destination, void const* source, size_t count, void*& fault_at)
{
    return powerpc_safe_memcpy(destination, source, count, fault_at);
}

NEVER_INLINE bool safe_memset(void* destination, int value, size_t count, void*& fault_at)
{
    return powerpc_safe_memset(destination, value, count, fault_at);
}

NEVER_INLINE ssize_t safe_strnlen(char const* string, size_t maximum, void*& fault_at)
{
    return powerpc_safe_strnlen(string, maximum, fault_at);
}

SAFEMEM_ATOMIC_SECTION NEVER_INLINE FLATTEN Optional<u32> safe_atomic_load_relaxed(u32 volatile* var)
{
    u32 result;
    register FlatPtr error asm("r10") = 0;
    asm volatile(R"(
.global safe_atomic_load_relaxed_ins
safe_atomic_load_relaxed_ins:
    lwz %[result],0(%[var])
.global safe_atomic_load_relaxed_faulted
safe_atomic_load_relaxed_faulted:
)"
        : [result] "=r"(result), "+r"(error)
        : [var] "r"(var)
        : "memory");
    if (error)
        return {};
    return result;
}

SAFEMEM_ATOMIC_SECTION NEVER_INLINE FLATTEN bool safe_atomic_store_relaxed(u32 volatile* var, u32 value)
{
    register FlatPtr error asm("r10") = 0;
    asm volatile(R"(
.global safe_atomic_store_relaxed_ins
safe_atomic_store_relaxed_ins:
    stw %[value],0(%[var])
.global safe_atomic_store_relaxed_faulted
safe_atomic_store_relaxed_faulted:
)"
        : "+r"(error)
        : [var] "r"(var), [value] "r"(value)
        : "memory");
    return error == 0;
}

SAFEMEM_ATOMIC_SECTION NEVER_INLINE FLATTEN Optional<u32> safe_atomic_fetch_add_relaxed(u32 volatile* var, u32 value)
{
    u32 old_value;
    register FlatPtr error asm("r10") = 0;
    asm volatile(R"(
1:
.global safe_atomic_fetch_add_relaxed_ins_1
safe_atomic_fetch_add_relaxed_ins_1:
    lwarx %[old],0,%[var]
    add 11,%[old],%[value]
.global safe_atomic_fetch_add_relaxed_ins_2
safe_atomic_fetch_add_relaxed_ins_2:
    stwcx. 11,0,%[var]
    bne- 1b
.global safe_atomic_fetch_add_relaxed_faulted
safe_atomic_fetch_add_relaxed_faulted:
)"
        : [old] "=&r"(old_value), "+r"(error)
        : [var] "r"(var), [value] "r"(value)
        : "r11", "cc", "memory");
    if (error)
        return {};
    return old_value;
}

SAFEMEM_ATOMIC_SECTION NEVER_INLINE FLATTEN Optional<u32> safe_atomic_exchange_relaxed(u32 volatile* var, u32 value)
{
    u32 old_value;
    register FlatPtr error asm("r10") = 0;
    asm volatile(R"(
1:
.global safe_atomic_exchange_relaxed_ins_1
safe_atomic_exchange_relaxed_ins_1:
    lwarx %[old],0,%[var]
.global safe_atomic_exchange_relaxed_ins_2
safe_atomic_exchange_relaxed_ins_2:
    stwcx. %[value],0,%[var]
    bne- 1b
.global safe_atomic_exchange_relaxed_faulted
safe_atomic_exchange_relaxed_faulted:
)"
        : [old] "=&r"(old_value), "+r"(error)
        : [var] "r"(var), [value] "r"(value)
        : "cc", "memory");
    if (error)
        return {};
    return old_value;
}

SAFEMEM_ATOMIC_SECTION NEVER_INLINE FLATTEN Optional<bool> safe_atomic_compare_exchange_relaxed(u32 volatile* var, u32& expected, u32 desired)
{
    u32 observed;
    FlatPtr exchanged = 0;
    register FlatPtr error asm("r10") = 0;
    asm volatile(R"(
1:
.global safe_atomic_compare_exchange_relaxed_ins_1
safe_atomic_compare_exchange_relaxed_ins_1:
    lwarx %[observed],0,%[var]
    cmpw %[observed],%[expected]
    bne 2f
.global safe_atomic_compare_exchange_relaxed_ins_2
safe_atomic_compare_exchange_relaxed_ins_2:
    stwcx. %[desired],0,%[var]
    bne- 1b
    li %[exchanged],1
2:
.global safe_atomic_compare_exchange_relaxed_faulted
safe_atomic_compare_exchange_relaxed_faulted:
)"
        : [observed] "=&r"(observed), [exchanged] "+&r"(exchanged), "+r"(error)
        : [var] "r"(var), [expected] "r"(expected), [desired] "r"(desired)
        : "cc", "memory");
    if (error)
        return {};
    if (!exchanged)
        expected = observed;
    return exchanged != 0;
}

bool handle_safe_access_fault(RegisterState& regs, FlatPtr fault_address)
{
    auto pc = regs.ip();
    if (pc >= reinterpret_cast<FlatPtr>(start_of_safemem_text) && pc < reinterpret_cast<FlatPtr>(end_of_safemem_text)) {
        FlatPtr continuation;
        FlatPtr fault_pointer;
        FlatPtr result;
        if (pc == reinterpret_cast<FlatPtr>(safe_memcpy_ins_1) || pc == reinterpret_cast<FlatPtr>(safe_memcpy_ins_2)) {
            continuation = reinterpret_cast<FlatPtr>(safe_memcpy_faulted);
            fault_pointer = regs.gpr[6];
            result = 0;
        } else if (pc == reinterpret_cast<FlatPtr>(safe_memset_ins)) {
            continuation = reinterpret_cast<FlatPtr>(safe_memset_faulted);
            fault_pointer = regs.gpr[6];
            result = 0;
        } else if (pc == reinterpret_cast<FlatPtr>(safe_strnlen_ins)) {
            continuation = reinterpret_cast<FlatPtr>(safe_strnlen_faulted);
            fault_pointer = regs.gpr[5];
            result = static_cast<FlatPtr>(-1);
        } else {
            return false;
        }
        *reinterpret_cast<FlatPtr*>(fault_pointer) = fault_address;
        regs.gpr[3] = result;
        regs.set_ip(continuation);
        return true;
    }
    if (pc < reinterpret_cast<FlatPtr>(start_of_safemem_atomic_text) || pc >= reinterpret_cast<FlatPtr>(end_of_safemem_atomic_text))
        return false;

    FlatPtr continuation;
    if (pc == reinterpret_cast<FlatPtr>(safe_atomic_load_relaxed_ins))
        continuation = reinterpret_cast<FlatPtr>(safe_atomic_load_relaxed_faulted);
    else if (pc == reinterpret_cast<FlatPtr>(safe_atomic_store_relaxed_ins))
        continuation = reinterpret_cast<FlatPtr>(safe_atomic_store_relaxed_faulted);
    else if (pc == reinterpret_cast<FlatPtr>(safe_atomic_fetch_add_relaxed_ins_1) || pc == reinterpret_cast<FlatPtr>(safe_atomic_fetch_add_relaxed_ins_2))
        continuation = reinterpret_cast<FlatPtr>(safe_atomic_fetch_add_relaxed_faulted);
    else if (pc == reinterpret_cast<FlatPtr>(safe_atomic_exchange_relaxed_ins_1) || pc == reinterpret_cast<FlatPtr>(safe_atomic_exchange_relaxed_ins_2))
        continuation = reinterpret_cast<FlatPtr>(safe_atomic_exchange_relaxed_faulted);
    else if (pc == reinterpret_cast<FlatPtr>(safe_atomic_compare_exchange_relaxed_ins_1) || pc == reinterpret_cast<FlatPtr>(safe_atomic_compare_exchange_relaxed_ins_2))
        continuation = reinterpret_cast<FlatPtr>(safe_atomic_compare_exchange_relaxed_faulted);
    else
        return false;

    regs.gpr[10] = 1;
    regs.set_ip(continuation);
    return true;
}

}
