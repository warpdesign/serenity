/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/RegisterState.h>
#include <Kernel/Arch/SafeMem.h>

extern "C" u8 safe_memcpy_ins_1[];
extern "C" u8 safe_memcpy_faulted[];
extern "C" u8 safe_strnlen_ins[];
extern "C" u8 safe_strnlen_faulted[];
extern "C" u8 safe_atomic_fetch_add_relaxed_ins_2[];
extern "C" u8 safe_atomic_fetch_add_relaxed_faulted[];

extern "C" int powerpc_safe_mem_test()
{
    char source[] { 'A', 'B', 'C', 0 };
    char destination[4] {};
    void* fault_at = nullptr;
    if (!Kernel::safe_memcpy(destination, source, sizeof(source), fault_at)
        || destination[0] != 'A' || destination[1] != 'B' || destination[2] != 'C' || fault_at)
        return 0;
    if (!Kernel::safe_memset(destination + 1, 'x', 2, fault_at)
        || destination[0] != 'A' || destination[1] != 'x' || destination[2] != 'x' || fault_at)
        return 0;
    if (Kernel::safe_strnlen(source, sizeof(source), fault_at) != 3
        || Kernel::safe_strnlen(source, 2, fault_at) != 2
        || Kernel::safe_strnlen(source, 0, fault_at) != 0 || fault_at)
        return 0;

    volatile u32 value = 7;
    auto loaded = Kernel::safe_atomic_load_relaxed(&value);
    auto added = Kernel::safe_atomic_fetch_add_relaxed(&value, 5);
    auto exchanged = Kernel::safe_atomic_exchange_relaxed(&value, 20);
    u32 expected = 20;
    auto succeeded = Kernel::safe_atomic_compare_exchange_relaxed(&value, expected, 25);
    expected = 99;
    auto failed = Kernel::safe_atomic_compare_exchange_relaxed(&value, expected, 30);
    if (!loaded.has_value() || loaded.value() != 7
        || !added.has_value() || added.value() != 7
        || !exchanged.has_value() || exchanged.value() != 12
        || !succeeded.has_value() || !succeeded.value()
        || !failed.has_value() || failed.value() || expected != 25 || value != 25)
        return 0;
    if (!Kernel::safe_atomic_store_relaxed(&value, 4) || value != 4)
        return 0;

    Kernel::RegisterState registers {};
    registers.gpr[6] = reinterpret_cast<FlatPtr>(&fault_at);
    registers.set_ip(reinterpret_cast<FlatPtr>(safe_memcpy_ins_1));
    if (!Kernel::handle_safe_access_fault(registers, 0x10045000u)
        || registers.ip() != reinterpret_cast<FlatPtr>(safe_memcpy_faulted)
        || registers.gpr[3] != 0 || fault_at != reinterpret_cast<void*>(0x10045000u))
        return 0;

    fault_at = nullptr;
    registers.gpr[5] = reinterpret_cast<FlatPtr>(&fault_at);
    registers.set_ip(reinterpret_cast<FlatPtr>(safe_strnlen_ins));
    if (!Kernel::handle_safe_access_fault(registers, 0x10046000u)
        || registers.ip() != reinterpret_cast<FlatPtr>(safe_strnlen_faulted)
        || registers.gpr[3] != static_cast<FlatPtr>(-1)
        || fault_at != reinterpret_cast<void*>(0x10046000u))
        return 0;

    registers.gpr[10] = 0;
    registers.set_ip(reinterpret_cast<FlatPtr>(safe_atomic_fetch_add_relaxed_ins_2));
    if (!Kernel::handle_safe_access_fault(registers, 0x10047000u)
        || registers.ip() != reinterpret_cast<FlatPtr>(safe_atomic_fetch_add_relaxed_faulted)
        || registers.gpr[10] != 1)
        return 0;
    registers.set_ip(0x10048000u);
    return !Kernel::handle_safe_access_fault(registers, 0x10049000u);
}
