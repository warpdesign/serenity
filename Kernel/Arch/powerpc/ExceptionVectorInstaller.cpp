/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/ExceptionVectorInstaller.h>

enum {
    VectorSlotSize = 0x100,
    CacheLineSize = 32,
};

int powerpc_install_exception_vector(unsigned vector_address, void const* template_start, void const* template_end)
{
    __UINTPTR_TYPE__ start = (__UINTPTR_TYPE__)template_start;
    __UINTPTR_TYPE__ end = (__UINTPTR_TYPE__)template_end;
    if (vector_address < 0x100 || vector_address > 0x1700 || (vector_address & (VectorSlotSize - 1)) != 0
        || end <= start || (start & 3u) != 0 || (end & 3u) != 0 || end - start > VectorSlotSize)
        return 0;

    unsigned machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    if ((machine_state & 0x4000u) != 0)
        return 0;

    unsigned const* source = (unsigned const*)template_start;
    volatile unsigned* destination = (volatile unsigned*)(__UINTPTR_TYPE__)vector_address;
    for (unsigned index = 0; index < (end - start) / sizeof(unsigned); ++index)
        destination[index] = source[index];

    for (unsigned address = vector_address; address < vector_address + end - start; address += CacheLineSize)
        __asm__ volatile("dcbst 0,%0" : : "r"(address) : "memory");
    __asm__ volatile("sync" : : : "memory");
    for (unsigned address = vector_address; address < vector_address + end - start; address += CacheLineSize)
        __asm__ volatile("icbi 0,%0" : : "r"(address) : "memory");
    __asm__ volatile("sync; isync" : : : "memory");
    return 1;
}
