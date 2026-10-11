/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/SegmentRegisters.h>
#include <Kernel/Arch/powerpc/VSIDAllocator.h>

int powerpc_user_vsid_base_valid(PowerPCSegmentWord base)
{
    return base >= POWERPC_FIRST_USER_VSID_BASE && base <= POWERPC_LAST_USER_VSID_BASE
        && (base & (POWERPC_VSID_BLOCK_SIZE - 1)) == 0;
}

PowerPCSegmentWord powerpc_read_segment_register(PowerPCSegmentWord segment)
{
    if (segment >= 16)
        __builtin_trap();

    PowerPCSegmentWord value;
    PowerPCSegmentWord effective_address = segment << 28;
    __asm__ volatile("mfsrin %0,%1" : "=r"(value) : "r"(effective_address) : "memory");
    return value;
}

void powerpc_write_segment_register(PowerPCSegmentWord segment, PowerPCSegmentWord value)
{
    if (segment >= 16)
        __builtin_trap();

    PowerPCSegmentWord effective_address = segment << 28;
    __asm__ volatile("mtsrin %0,%1; isync" : : "r"(value), "r"(effective_address) : "memory");
}

int powerpc_activate_user_segments(PowerPCSegmentWord base)
{
    if (!powerpc_user_vsid_base_valid(base))
        return 0;

    __asm__ volatile("sync" : : : "memory");
    for (PowerPCSegmentWord segment = 0; segment < POWERPC_USER_SEGMENT_COUNT; ++segment)
        powerpc_write_segment_register(segment, 0x20000000u | (base + segment));
    return 1;
}

int powerpc_activate_user_high_segments(PowerPCSegmentWord base)
{
    if (!powerpc_user_vsid_base_valid(base))
        return 0;

    PowerPCSegmentWord msr;
    __asm__ volatile("mfmsr %0" : "=r"(msr));
    if ((msr & 0x30u) != 0)
        return 0;

    // Call from the real-mode exception return path. No user HPT entry can
    // use these VSIDs because user mappings are limited to segments 0-11.
    __asm__ volatile("sync" : : : "memory");
    for (PowerPCSegmentWord segment = POWERPC_USER_SEGMENT_COUNT; segment < 16; ++segment)
        powerpc_write_segment_register(segment, 0x20000000u | (base + segment));
    return 1;
}

void powerpc_activate_kernel_high_segments(void)
{
    PowerPCSegmentWord msr;
    __asm__ volatile("mfmsr %0" : "=r"(msr));
    if ((msr & 0x30u) != 0)
        __builtin_trap();

    // Call from the real-mode exception entry path before using high kernel
    // addresses. The shared HPT reserves VSIDs 12-15 for the kernel.
    __asm__ volatile("sync" : : : "memory");
    for (PowerPCSegmentWord segment = POWERPC_USER_SEGMENT_COUNT; segment < 16; ++segment)
        powerpc_write_segment_register(segment, 0x20000000u | segment);
}

void powerpc_activate_kernel_segments(void)
{
    // Reserved VSIDs 0-11 have no user mappings. Kernel segments 12-15 use
    // fixed VSIDs in the shared HPT. Keep Kp=1 for all of these segments.
    __asm__ volatile("sync" : : : "memory");
    for (PowerPCSegmentWord segment = 0; segment < 16; ++segment)
        powerpc_write_segment_register(segment, 0x20000000u | segment);
}
