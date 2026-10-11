/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/Processor.h>
#include <Kernel/Arch/powerpc/CPUID.h>
#include <Kernel/Arch/powerpc/ExceptionVectorInstaller.h>
#include <Kernel/Arch/powerpc/TLB.h>
#include <Kernel/Boot/BootInfo.h>
#include <Kernel/Interrupts/InterruptDisabler.h>

namespace Kernel {

Processor* g_current_processor;

extern "C" u8 powerpc_data_storage_vector_start[];
extern "C" u8 powerpc_data_storage_vector_end[];
extern "C" void powerpc_data_storage_high_handler();
alignas(4096) static u8 s_exception_stack[16 * 1024];

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
    auto virtual_stack_top = reinterpret_cast<FlatPtr>(s_exception_stack + sizeof(s_exception_stack));
    VERIFY(virtual_stack_top >= g_boot_info.physical_to_virtual_offset);
    auto physical_stack_top = virtual_stack_top - g_boot_info.physical_to_virtual_offset;
    VERIFY(physical_stack_top < 256 * MiB);
    *reinterpret_cast<u32*>(virtual_stack_top - sizeof(u32)) = reinterpret_cast<FlatPtr>(powerpc_data_storage_high_handler);
    asm volatile("mtspr 272,%0" : : "r"(physical_stack_top) : "memory");
    VERIFY(powerpc_install_exception_vector(0x300, powerpc_data_storage_vector_start, powerpc_data_storage_vector_end));
    u32 msr = PowerPC::read_msr() & ~0x40u;
    asm volatile("sync; mtmsr %0; isync" : : "r"(msr) : "memory");
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
    // The 750 has no architecture-neutral WFI. Let pending interrupts run,
    // then return to the scheduler with interrupts disabled as it expects.
    enable_interrupts();
    pause();
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
