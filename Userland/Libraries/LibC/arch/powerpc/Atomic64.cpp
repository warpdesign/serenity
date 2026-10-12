/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>

// The 750 has word reservations but no atomic 64-bit memory operation. A lock
// serializes the GCC atomic runtime entry points; acquire/release also gives
// the stronger ordering needed by callers requesting sequential consistency.
static volatile u32 s_atomic64_lock;

static void lock_atomic64()
{
    while (__atomic_exchange_n(&s_atomic64_lock, 1u, __ATOMIC_ACQUIRE) != 0)
        asm volatile("nop" : : : "memory");
}

static void unlock_atomic64()
{
    __atomic_store_n(&s_atomic64_lock, 0u, __ATOMIC_RELEASE);
}

static u64 read_atomic64(void const volatile* pointer)
{
    auto const* bytes = reinterpret_cast<u8 const volatile*>(pointer);
    u64 value = 0;
    for (size_t i = 0; i < sizeof(u64); ++i)
        value = (value << 8) | bytes[i];
    return value;
}

static void write_atomic64(void volatile* pointer, u64 value)
{
    auto* bytes = reinterpret_cast<u8 volatile*>(pointer);
    for (size_t i = sizeof(u64); i > 0; --i) {
        bytes[i - 1] = static_cast<u8>(value);
        value >>= 8;
    }
}

extern "C" bool powerpc_atomic_is_lock_free(size_t, void const*) asm("__atomic_is_lock_free");
extern "C" bool powerpc_atomic_is_lock_free(size_t size, void const* pointer)
{
    if (size != 1 && size != 2 && size != 4)
        return false;
    return pointer == nullptr || (reinterpret_cast<FlatPtr>(pointer) & (size - 1)) == 0;
}

extern "C" u64 __atomic_load_8(void const volatile* pointer, int)
{
    lock_atomic64();
    auto value = read_atomic64(pointer);
    unlock_atomic64();
    return value;
}

extern "C" void __atomic_store_8(void volatile* pointer, u64 value, int)
{
    lock_atomic64();
    write_atomic64(pointer, value);
    unlock_atomic64();
}

extern "C" u64 __atomic_exchange_8(void volatile* pointer, u64 value, int)
{
    lock_atomic64();
    auto old_value = read_atomic64(pointer);
    write_atomic64(pointer, value);
    unlock_atomic64();
    return old_value;
}

extern "C" bool powerpc_atomic_compare_exchange_8(void volatile*, void*, u64, int, int) asm("__atomic_compare_exchange_8");
extern "C" bool powerpc_atomic_compare_exchange_8(void volatile* pointer, void* expected_pointer, u64 desired, int, int)
{
    lock_atomic64();
    auto old_value = read_atomic64(pointer);
    auto expected = read_atomic64(expected_pointer);
    bool exchanged = old_value == expected;
    if (exchanged)
        write_atomic64(pointer, desired);
    else
        write_atomic64(expected_pointer, old_value);
    unlock_atomic64();
    return exchanged;
}

#define DEFINE_FETCH_OPERATION(name, expression)                                      \
    extern "C" u64 __atomic_fetch_##name##_8(void volatile* pointer, u64 operand, int) \
    {                                                                                 \
        lock_atomic64();                                                              \
        auto old_value = read_atomic64(pointer);                                      \
        write_atomic64(pointer, (expression));                                        \
        unlock_atomic64();                                                            \
        return old_value;                                                             \
    }

DEFINE_FETCH_OPERATION(add, old_value + operand)
DEFINE_FETCH_OPERATION(sub, old_value - operand)
DEFINE_FETCH_OPERATION(and, old_value & operand)
DEFINE_FETCH_OPERATION(or, old_value | operand)
DEFINE_FETCH_OPERATION(xor, old_value ^ operand)
DEFINE_FETCH_OPERATION(nand, ~(old_value & operand))

#undef DEFINE_FETCH_OPERATION
