/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/powerpc/CPUID.h>
#include <Kernel/Arch/powerpc/CPU.h>
#include <Kernel/Arch/powerpc/ExceptionSetup.h>
#include <Kernel/Arch/powerpc/TLB.h>
#include <Kernel/Interrupts/InterruptDisabler.h>

namespace Kernel {

Processor* g_current_processor;

StringView ProcessorBase::platform_string()
{
    return "powerpc"sv;
}

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

void ProcessorBase::initialize(u32 cpu)
{
    VERIFY(cpu == m_cpu);
    VERIFY((PowerPC::read_msr() & 0x2000u) != 0);
    PowerPC::initialize_exception_vectors();
    if (has_feature(CPUFeature::Doze)) {
        auto hid0 = PowerPC::read_hid0();
        hid0 = (hid0 & ~(PowerPC::HID0_NAP | PowerPC::HID0_SLEEP)) | PowerPC::HID0_DOZE;
        PowerPC::write_hid0(hid0);
    }
    m_deferred_call_pool.init();
    g_total_processors.store(1, AK::MemoryOrder::memory_order_release);
    store_fpu_state(s_clean_fpu_state);
}

[[noreturn]] void ProcessorBase::halt()
{
    disable_interrupts();
    for (;;)
        asm volatile("nop" : : : "memory");
}

u32 ProcessorBase::clear_critical()
{
    InterruptDisabler disabler;
    auto previous = in_critical();
    auto& processor = current();
    processor.m_in_critical = 0;
    if (processor.m_in_irq == 0)
        processor.check_invoke_scheduler();
    return previous;
}

u32 ProcessorBase::smp_wake_n_idle_processors(u32)
{
    // Pegasos 2 bring-up currently has only the bootstrap processor.
    return 0;
}

Processor& ProcessorBase::by_id(u32 id)
{
    VERIFY(id == 0);
    return current();
}

void ProcessorBase::idle_begin() const
{
}

void ProcessorBase::idle_end() const
{
}

void ProcessorBase::idle() const
{
    VERIFY_INTERRUPTS_DISABLED();
    idle_begin();
    if (has_feature(CPUFeature::Doze)) {
        // The 750 wakes from doze on an external or decrementer interrupt.
        // The exception handler clears POW in the saved MSR before returning.
        asm volatile("sync" : : : "memory");
        PowerPC::write_msr(PowerPC::read_msr() | PowerPC::MSR_EE | PowerPC::MSR_POW);
    } else {
        enable_interrupts();
        pause();
    }
    disable_interrupts();
    idle_end();
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
        asm volatile("dcbf 0,%0" : : "r"(address) : "memory");
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
    powerpc_flush_entire_tlb();
}

}
