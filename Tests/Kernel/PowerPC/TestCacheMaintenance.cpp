/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>

extern "C" int powerpc_cache_maintenance_test()
{
    // Put the two instructions on opposite sides of a 32-byte cache line.
    alignas(32) static volatile u32 code[16] {};
    code[7] = 0x38600007; // li r3, 7
    code[8] = 0x4e800020; // blr

    auto* instructions = reinterpret_cast<void*>(const_cast<u32*>(&code[7]));
    Kernel::Processor::flush_instruction_cache(VirtualAddress { reinterpret_cast<FlatPtr>(instructions) }, 8);
    auto function = reinterpret_cast<int (*)()>(instructions);
    if (function() != 7)
        return 0;

    code[7] = 0x38600009; // li r3, 9
    Kernel::Processor::flush_instruction_cache(VirtualAddress { reinterpret_cast<FlatPtr>(instructions) }, 8);
    if (function() != 9)
        return 0;

    volatile u32 payload = 0x12345678;
    Kernel::Processor::flush_data_cache(VirtualAddress { reinterpret_cast<FlatPtr>(&payload) }, sizeof(payload));
    return payload == 0x12345678;
}
