/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibELF/Arch/GenericDynamicRelocationType.h>
#include <LibELF/Arch/tls.h>
#include <sys/internals.h>

static_assert(static_cast<unsigned>(ELF::GenericDynamicRelocationType::TLS_DTPMOD) == 68);
static_assert(static_cast<unsigned>(ELF::GenericDynamicRelocationType::TLS_TPREL) == 73);
static_assert(static_cast<unsigned>(ELF::GenericDynamicRelocationType::TLS_DTPREL) == 78);
static_assert(static_cast<unsigned>(ELF::GenericDynamicRelocationType::IRELATIVE) == 248);

extern "C" int powerpc_static_tls_lookup_test()
{
    alignas(16) static unsigned char region[80] {};
    auto base = reinterpret_cast<FlatPtr>(region);
    auto thread_pointer = ELF::calculate_tp_value_from_static_tls_region_address(base, 64, 16);
    auto* tls_byte = region + 16 + 32;
    auto* second_module_byte = region + 16 + 48;
    *tls_byte = 0xa5;
    *second_module_byte = 0x5a;

    // The loader's static-TLS shortcut stores the block offset in ti_module.
    // Its DTPREL adjustment compensates for both the DTV and TP biases.
    __tls_index index { 0, 32 - ELF::TLS_DTV_OFFSET + ELF::TLS_TP_STATIC_TLS_BLOCK_OFFSET };
    __tls_index second_module_index { 16, 32 - ELF::TLS_DTV_OFFSET + ELF::TLS_TP_STATIC_TLS_BLOCK_OFFSET };
    FlatPtr original_thread_pointer;
    __asm__ volatile("mr %0,2" : "=r"(original_thread_pointer));
    ELF::set_thread_pointer_register(thread_pointer);
    auto* resolved = __tls_get_addr(&index);
    auto* second_module_resolved = __tls_get_addr(&second_module_index);
    ELF::set_thread_pointer_register(original_thread_pointer);
    return resolved == tls_byte && *static_cast<unsigned char*>(resolved) == 0xa5
        && second_module_resolved == second_module_byte && *static_cast<unsigned char*>(second_module_resolved) == 0x5a;
}
