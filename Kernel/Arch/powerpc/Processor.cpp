/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/powerpc/CPUID.h>

namespace Kernel {

Processor* g_current_processor;

void ProcessorBase::early_initialize(u32 cpu)
{
    VERIFY(cpu == 0);
    VERIFY(g_current_processor == nullptr);

    m_self = static_cast<Processor*>(this);
    m_cpu = cpu;
    m_features = detect_cpu_features(PowerPC::read_pvr());
    VERIFY(m_features.has_flag(CPUFeature::FPU));
    VERIFY(m_features.has_flag(CPUFeature::HashedMMU));
    m_physical_address_bit_width = 32;
    m_virtual_address_bit_width = 32;
    m_in_critical = 0;
    m_in_irq = 0;
    m_in_scheduler = 0;
    m_invoke_scheduler_async = 0;
    m_current_thread = nullptr;
    m_idle_thread = nullptr;

    g_current_processor = static_cast<Processor*>(this);
}

void ProcessorBase::store_fpu_state(FPUState& state)
{
    ::Kernel::store_fpu_state(&state);
}

void ProcessorBase::load_fpu_state(FPUState const& state)
{
    ::Kernel::load_fpu_state(&state);
}

void ProcessorBase::flush_data_cache(VirtualAddress vaddr, size_t byte_count)
{
    if (byte_count == 0)
        return;

    constexpr u64 cache_block_size = 32;
    u64 end = static_cast<u64>(vaddr.get()) + byte_count;
    VERIFY(end <= 0x1'0000'0000ull);
    u64 last_block = (end - 1) & ~(cache_block_size - 1);
    for (u64 block = vaddr.get() & ~(cache_block_size - 1);; block += cache_block_size) {
        auto address = static_cast<FlatPtr>(block);
        asm volatile("dcbst 0,%0" : : "r"(address) : "memory");
        if (block == last_block)
            break;
    }
    asm volatile("sync" : : : "memory");
}

void ProcessorBase::flush_instruction_cache(VirtualAddress vaddr, size_t byte_count)
{
    if (byte_count == 0)
        return;

    flush_data_cache(vaddr, byte_count);

    constexpr u64 cache_block_size = 32;
    u64 end = static_cast<u64>(vaddr.get()) + byte_count;
    u64 last_block = (end - 1) & ~(cache_block_size - 1);
    for (u64 block = vaddr.get() & ~(cache_block_size - 1);; block += cache_block_size) {
        auto address = static_cast<FlatPtr>(block);
        asm volatile("icbi 0,%0" : : "r"(address) : "memory");
        if (block == last_block)
            break;
    }
    asm volatile("sync; isync" : : : "memory");
}

void ProcessorBase::flush_tlb_local(VirtualAddress vaddr, size_t page_count)
{
    if (page_count == 0)
        return;

    u64 first_page = vaddr.get() & ~(static_cast<u64>(PAGE_SIZE) - 1);
    u64 end = first_page + static_cast<u64>(page_count) * PAGE_SIZE;
    VERIFY(end <= 0x1'0000'0000ull);

    asm volatile("sync" : : : "memory");
    for (u64 page = first_page; page < end; page += PAGE_SIZE) {
        auto address = static_cast<FlatPtr>(page);
        asm volatile("tlbie %0" : : "r"(address) : "memory");
    }
    asm volatile("eieio; tlbsync; sync" : : : "memory");
}

void ProcessorBase::flush_tlb(Memory::PageDirectory const*, VirtualAddress vaddr, size_t page_count)
{
    // Pegasos 2 bring-up currently runs one processor.
    flush_tlb_local(vaddr, page_count);
}

void ProcessorBase::flush_entire_tlb_local()
{
    // The 750 and 74xx do not implement tlbia. EA bits 14-19 index all
    // 64 instruction and data TLB sets.
    asm volatile("sync" : : : "memory");
    for (u32 index = 0; index < 64; ++index) {
        u32 address = index << 14;
        asm volatile("tlbie %0" : : "r"(address) : "memory");
    }
    asm volatile("eieio; tlbsync; sync" : : : "memory");
}

}
