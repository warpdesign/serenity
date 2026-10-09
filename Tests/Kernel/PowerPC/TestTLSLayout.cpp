/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Arch/tls.h>
#include <stddef.h>

static_assert(sizeof(ELF::ThreadControlBlock) == 8);
static_assert(offsetof(ELF::ThreadControlBlock, dynamic_thread_vector) == 0);
static_assert(ELF::TLS_VARIANT == 1);
static_assert(ELF::TLS_DTV_OFFSET == 0x8000);

extern "C" int powerpc_tls_layout_test()
{
    alignas(16) static unsigned char region[80] {};
    FlatPtr base = reinterpret_cast<FlatPtr>(region);
    constexpr size_t template_size = 64;
    constexpr size_t alignment = 16;
    if (ELF::calculate_static_tls_region_size(template_size, alignment) != sizeof(region))
        return 0;

    FlatPtr thread_pointer = ELF::calculate_tp_value_from_static_tls_region_address(base, template_size, alignment);
    auto* tcb = ELF::get_tcb_pointer_from_thread_pointer(thread_pointer);
    auto* first_block = ELF::get_pointer_to_first_static_tls_block_from_thread_pointer(thread_pointer, template_size, alignment);
    auto* recovered_region = ELF::get_pointer_to_static_tls_region_from_thread_pointer(thread_pointer, template_size, alignment);
    if (thread_pointer != base + 16 + 0x7000
        || reinterpret_cast<FlatPtr>(tcb) != base + 8
        || reinterpret_cast<FlatPtr>(first_block) != base + 16
        || reinterpret_cast<FlatPtr>(recovered_region) != base)
        return 0;

    alignas(32) static unsigned char wider_aligned_region[96] {};
    FlatPtr wider_base = reinterpret_cast<FlatPtr>(wider_aligned_region);
    FlatPtr wider_thread_pointer = ELF::calculate_tp_value_from_static_tls_region_address(wider_base, template_size, 32);
    if (ELF::calculate_static_tls_region_size(template_size, 32) != sizeof(wider_aligned_region)
        || wider_thread_pointer != wider_base + 32 + 0x7000
        || reinterpret_cast<FlatPtr>(ELF::get_tcb_pointer_from_thread_pointer(wider_thread_pointer)) != wider_base + 24
        || reinterpret_cast<FlatPtr>(ELF::get_pointer_to_first_static_tls_block_from_thread_pointer(wider_thread_pointer, template_size, 32)) != wider_base + 32
        || reinterpret_cast<FlatPtr>(ELF::get_pointer_to_static_tls_region_from_thread_pointer(wider_thread_pointer, template_size, 32)) != wider_base)
        return 0;

    tcb->dynamic_thread_vector = nullptr;
    *static_cast<unsigned char*>(first_block) = 0x5a;
    FlatPtr original_thread_pointer;
    __asm__ volatile("mr %0,2" : "=r"(original_thread_pointer));
    ELF::set_thread_pointer_register(thread_pointer);
    FlatPtr observed_thread_pointer;
    __asm__ volatile("mr %0,2" : "=r"(observed_thread_pointer));
    ELF::set_thread_pointer_register(original_thread_pointer);
    return observed_thread_pointer == thread_pointer && region[16] == 0x5a;
}
