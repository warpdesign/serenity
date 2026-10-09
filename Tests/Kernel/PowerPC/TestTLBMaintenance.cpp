/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>

extern "C" int powerpc_hpt_processor_tlb_test(void (*flush_page)(u32), void (*flush_all)(void));

static void flush_page(u32 address)
{
    Kernel::Processor::flush_tlb_local(VirtualAddress { address }, 1);
}

static void flush_all()
{
    Kernel::Processor::flush_entire_tlb_local();
}

extern "C" int powerpc_tlb_maintenance_test()
{
    return powerpc_hpt_processor_tlb_test(flush_page, flush_all);
}
