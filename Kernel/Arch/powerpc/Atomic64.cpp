/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Types.h>
#include <Kernel/Arch/powerpc/CPU.h>

// The 32-bit 750 cannot atomically update a pair of words. Serialize 64-bit
// operations with a global reservation lock, and mask local interrupts while
// holding it so an interrupt on this CPU cannot deadlock against itself.
static volatile u32 s_atomic64_lock;
using AtomicWord = u32 __attribute__((may_alias));

static u32 lock_atomic64()
{
    auto previous_msr = Kernel::PowerPC::read_msr();
    Kernel::PowerPC::write_msr(previous_msr & ~0x8000u);
    while (__atomic_exchange_n(&s_atomic64_lock, 1u, __ATOMIC_ACQUIRE) != 0)
        asm volatile("nop" : : : "memory");
    return previous_msr;
}

static void unlock_atomic64(u32 previous_msr)
{
    __atomic_store_n(&s_atomic64_lock, 0u, __ATOMIC_RELEASE);
    Kernel::PowerPC::write_msr(previous_msr);
}

static u64 read_locked(void const volatile* pointer)
{
    auto const* words = reinterpret_cast<AtomicWord const volatile*>(pointer);
    return (static_cast<u64>(words[0]) << 32) | words[1];
}

static void write_locked(void volatile* pointer, u64 value)
{
    auto* words = reinterpret_cast<AtomicWord volatile*>(pointer);
    words[0] = static_cast<u32>(value >> 32);
    words[1] = static_cast<u32>(value);
}

extern "C" unsigned long long __atomic_load_8(void const volatile* pointer, int)
{
    auto previous_msr = lock_atomic64();
    auto value = read_locked(pointer);
    unlock_atomic64(previous_msr);
    return value;
}

extern "C" unsigned long long __atomic_fetch_add_8(void volatile* pointer, unsigned long long increment, int)
{
    auto previous_msr = lock_atomic64();
    auto old_value = read_locked(pointer);
    write_locked(pointer, old_value + increment);
    unlock_atomic64(previous_msr);
    return old_value;
}
