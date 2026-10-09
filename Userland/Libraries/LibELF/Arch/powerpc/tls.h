/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/BitCast.h>
#include <AK/StdLibExtraDetails.h>
#include <AK/Types.h>

namespace ELF {

struct ThreadControlBlock {
    void* dynamic_thread_vector;
    FlatPtr padding;
};

static_assert(sizeof(ThreadControlBlock) == 8);

static constexpr size_t TLS_VARIANT = 1;
static constexpr size_t TLS_DTV_OFFSET = 0x8000;
static constexpr FlatPtr TLS_TP_STATIC_TLS_BLOCK_OFFSET = static_cast<FlatPtr>(-0x7000);
static constexpr size_t TLS_TP_BIAS = 0x7000;
static constexpr size_t TLS_MINIMUM_BLOCK_OFFSET = 16;

// [padding][8-byte TCB][static TLS...]
//                     ^first block            r2 = first block + 0x7000
inline size_t calculate_static_tls_region_size(size_t tls_template_size, size_t tls_alignment)
{
    return align_up_to(TLS_MINIMUM_BLOCK_OFFSET, tls_alignment) + tls_template_size;
}

inline FlatPtr calculate_tp_value_from_static_tls_region_address(FlatPtr static_tls_region_address, size_t tls_template_size, size_t tls_alignment)
{
    (void)tls_template_size;
    return static_tls_region_address + align_up_to(TLS_MINIMUM_BLOCK_OFFSET, tls_alignment) + TLS_TP_BIAS;
}

inline ThreadControlBlock* get_tcb_pointer_from_thread_pointer(FlatPtr thread_pointer)
{
    return bit_cast<ThreadControlBlock*>(thread_pointer - TLS_TP_BIAS - sizeof(ThreadControlBlock));
}

inline void* get_pointer_to_first_static_tls_block_from_thread_pointer(FlatPtr thread_pointer, size_t tls_template_size, size_t tls_alignment)
{
    (void)tls_template_size;
    (void)tls_alignment;
    return bit_cast<void*>(thread_pointer - TLS_TP_BIAS);
}

inline void* get_pointer_to_static_tls_region_from_thread_pointer(FlatPtr thread_pointer, size_t tls_template_size, size_t tls_alignment)
{
    (void)tls_template_size;
    return bit_cast<void*>(thread_pointer - TLS_TP_BIAS - align_up_to(TLS_MINIMUM_BLOCK_OFFSET, tls_alignment));
}

}
