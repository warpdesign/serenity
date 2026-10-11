/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/ExceptionSetup.h>
#include <Kernel/Arch/powerpc/ExceptionVectorInstaller.h>
#include <Kernel/Arch/powerpc/Time/Timebase.h>
#include <Kernel/Boot/BootInfo.h>

namespace Kernel::PowerPC {

extern "C" u8 powerpc_data_storage_vector_start[];
extern "C" u8 powerpc_data_storage_vector_end[];
extern "C" void powerpc_data_storage_high_handler();
extern "C" u8 powerpc_decrementer_vector_start[];
extern "C" u8 powerpc_decrementer_vector_end[];
extern "C" void powerpc_decrementer_high_handler();
extern "C" u8 powerpc_external_vector_start[];
extern "C" u8 powerpc_external_vector_end[];
extern "C" void powerpc_external_high_handler();
alignas(4096) static u8 s_data_storage_stack[16 * 1024];
alignas(4096) static u8 s_decrementer_stack[16 * 1024];

static u32 physical_stack_top(u8* start, size_t size, FlatPtr handler)
{
    auto virtual_top = reinterpret_cast<FlatPtr>(start + size);
    VERIFY(virtual_top >= g_boot_info.physical_to_virtual_offset);
    auto physical_top = virtual_top - g_boot_info.physical_to_virtual_offset;
    VERIFY(physical_top < 256 * MiB);
    *reinterpret_cast<u32*>(virtual_top - sizeof(u32)) = handler;
    return physical_top;
}

void initialize_exception_vectors()
{
    auto data_storage_top = physical_stack_top(s_data_storage_stack, sizeof(s_data_storage_stack),
        reinterpret_cast<FlatPtr>(powerpc_data_storage_high_handler));
    auto decrementer_top = physical_stack_top(s_decrementer_stack, sizeof(s_decrementer_stack),
        reinterpret_cast<FlatPtr>(powerpc_decrementer_high_handler));
    *reinterpret_cast<u32*>(reinterpret_cast<FlatPtr>(s_decrementer_stack + sizeof(s_decrementer_stack)) - 2 * sizeof(u32))
        = reinterpret_cast<FlatPtr>(powerpc_external_high_handler);
    asm volatile("mtspr 272,%0; mtspr 275,%1" : : "r"(data_storage_top), "r"(decrementer_top) : "memory");
    set_decrementer(0x7fffffffu);
    VERIFY(powerpc_install_exception_vector(0x300, powerpc_data_storage_vector_start, powerpc_data_storage_vector_end));
    VERIFY(powerpc_install_exception_vector(0x500, powerpc_external_vector_start, powerpc_external_vector_end));
    VERIFY(powerpc_install_exception_vector(0x900, powerpc_decrementer_vector_start, powerpc_decrementer_vector_end));
    u32 msr;
    asm volatile("mfmsr %0" : "=r"(msr));
    msr &= ~0x40u;
    asm volatile("sync; mtmsr %0; isync" : : "r"(msr) : "memory");
}

}
