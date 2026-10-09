/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void powerpc_flush_tlb_page(unsigned int effective_address);
void powerpc_flush_entire_tlb(void);

#ifdef __cplusplus
}
#endif
