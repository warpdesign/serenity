/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Loader/KernelImagePlan.h>
#include <Kernel/Arch/powerpc/Loader/KernelMappings.h>
#include <Kernel/Arch/powerpc/HashPageTable.h>

using FirmwareEntry = int (*)(void*);

struct FirmwareCall {
    char const* service;
    u32 argument_count;
    u32 result_count;
    u32 cells[5];
};

extern "C" u8 _binary_kernel_elf_start[];
extern "C" u8 _binary_kernel_elf_end[];
extern "C" u32 powerpc_loader_high_marker();
extern "C" void powerpc_loader_enable_translation(u32 sdr1);
extern "C" void powerpc_loader_disable_translation();

static u32 firmware_pointer(void const* pointer)
{
    return reinterpret_cast<u32>(pointer);
}

static u32 find_device(FirmwareEntry firmware, char const* path)
{
    FirmwareCall call { "finddevice", 1, 1, { firmware_pointer(path), 0 } };
    if (firmware(&call) != 0)
        return 0;
    return call.cells[1];
}

static u32 claim_pages(FirmwareEntry firmware, u32 size, u32 alignment)
{
    FirmwareCall call { "claim", 3, 1, { 0, size, alignment, 0 } };
    if (firmware(&call) != 0)
        return 0;
    return call.cells[3];
}

static void write_console(FirmwareEntry firmware, u32 stdout, char const* message)
{
    u32 length = 0;
    while (message[length] != '\0')
        ++length;
    FirmwareCall call { "write", 3, 1, { stdout, firmware_pointer(message), length, 0 } };
    (void)firmware(&call);
}

static void write_hex(FirmwareEntry firmware, u32 stdout, u32 value)
{
    char output[] = "0x00000000\r\n";
    char const* digits = "0123456789abcdef";
    for (int index = 9; index >= 2; --index) {
        output[index] = digits[value & 15u];
        value >>= 4;
    }
    write_console(firmware, stdout, output);
}

extern "C" void loader_main(FirmwareEntry firmware);
extern "C" void loader_main(FirmwareEntry firmware)
{
    u32 chosen = find_device(firmware, "/chosen");
    if (chosen == 0 || chosen == 0xffffffffu)
        return;
    u32 stdout = 0;
    FirmwareCall stdout_call { "getprop", 4, 1,
        { chosen, firmware_pointer("stdout"), firmware_pointer(&stdout), sizeof(stdout) } };
    if (firmware(&stdout_call) != 0 || stdout_call.cells[4] != sizeof(stdout))
        return;

    Kernel::PowerPC::Loader::KernelImagePlan plan;
    if (!Kernel::PowerPC::Loader::plan_kernel_image(_binary_kernel_elf_start,
            _binary_kernel_elf_end - _binary_kernel_elf_start, plan)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel ELF invalid\r\n");
        return;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel ELF32 validated\r\n");
    write_console(firmware, stdout, "SerenityOS PowerPC loader: load span ");
    write_hex(firmware, stdout, plan.end_virtual_offset - plan.first_virtual_offset);
    write_console(firmware, stdout, "SerenityOS PowerPC loader: entry ");
    write_hex(firmware, stdout, plan.entry_offset);

    u32 span = plan.end_virtual_offset - plan.first_virtual_offset;
    u32 physical_base = claim_pages(firmware, span, 2 * 1024 * 1024);
    if (physical_base == 0 || physical_base == 0xffffffffu
        || (physical_base & (2 * 1024 * 1024 - 1)) != 0
        || physical_base > 0xffffffffu - span
        || !Kernel::PowerPC::Loader::load_kernel_image(_binary_kernel_elf_start,
            _binary_kernel_elf_end - _binary_kernel_elf_start, plan,
            reinterpret_cast<u8*>(physical_base), span)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel segment load failed\r\n");
        return;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel segments loaded\r\n");
    write_console(firmware, stdout, "SerenityOS PowerPC loader: physical base ");
    write_hex(firmware, stdout, physical_base);

    u64 runtime_base = static_cast<u64>(KERNEL_MAPPING_BASE) + physical_base - plan.first_virtual_offset;
    u32 relocation_count = 0;
    if (runtime_base > 0xffffffffu
        || !Kernel::PowerPC::Loader::relocate_kernel_image(_binary_kernel_elf_start,
            _binary_kernel_elf_end - _binary_kernel_elf_start, plan,
            reinterpret_cast<u8*>(physical_base), span, static_cast<u32>(runtime_base), relocation_count)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel relocation failed\r\n");
        return;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel relocations applied\r\n");
    write_console(firmware, stdout, "SerenityOS PowerPC loader: relocation count ");
    write_hex(firmware, stdout, relocation_count);
    write_console(firmware, stdout, "SerenityOS PowerPC loader: runtime base ");
    write_hex(firmware, stdout, static_cast<u32>(runtime_base));

    constexpr u32 table_size = 256 * 1024;
    u32 table_physical = claim_pages(firmware, table_size, table_size);
    Kernel::PowerPC::Loader::KernelMappings mappings;
    if (table_physical == 0 || table_physical == 0xffffffffu
        || !Kernel::PowerPC::Loader::map_kernel_image(_binary_kernel_elf_start,
            _binary_kernel_elf_end - _binary_kernel_elf_start, plan,
            physical_base, static_cast<u32>(runtime_base), table_physical, table_size, mappings)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel mappings failed\r\n");
        return;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel high mappings prepared\r\n");
    write_console(firmware, stdout, "SerenityOS PowerPC loader: hash table physical ");
    write_hex(firmware, stdout, mappings.table_physical);
    write_console(firmware, stdout, "SerenityOS PowerPC loader: mapped pages ");
    write_hex(firmware, stdout, mappings.mapped_pages);

    u32 marker_physical = firmware_pointer(reinterpret_cast<void const*>(powerpc_loader_high_marker));
    u32 marker_high = KERNEL_MAPPING_BASE + marker_physical;
    PowerPCHashPageTable table { reinterpret_cast<PowerPCHPTEntry*>(mappings.table_physical), mappings.group_count };
    u32 marker_flags = POWERPC_HPT_REFERENCED | POWERPC_HPT_CHANGED | POWERPC_HPT_COHERENT | POWERPC_HPT_READ_ONLY;
    if (!powerpc_hpt_map(&table, marker_high >> 28, marker_high & ~4095u, marker_physical & ~4095u, marker_flags)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: high alias probe mapping failed\r\n");
        return;
    }
    u32 high_entry = static_cast<u32>(runtime_base) + plan.entry_offset;
    auto* entry = powerpc_hpt_find(&table, high_entry >> 28, high_entry & ~4095u);
    if (!entry) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel entry mapping missing\r\n");
        return;
    }
    u32 table_entry_offset = reinterpret_cast<FlatPtr>(entry) - mappings.table_physical;
    u32 expected_upper = entry->upper;
    u32 entry_physical = physical_base + plan.entry_offset - plan.first_virtual_offset;
    u32 expected_entry_word = *reinterpret_cast<volatile u32 const*>(entry_physical);
    powerpc_loader_enable_translation(mappings.table_physical | ((table_size >> 16) - 1));
    u32 high_entry_word = *reinterpret_cast<volatile u32 const*>(high_entry);
    u32 high_table_word = *reinterpret_cast<volatile u32 const*>(mappings.table_virtual + table_entry_offset);
    auto high_marker = reinterpret_cast<u32 (*)()>(marker_high);
    u32 marker_result = high_marker();
    powerpc_loader_disable_translation();
    int marker_unmapped = powerpc_hpt_unmap(&table, marker_high >> 28, marker_high & ~4095u);
    if (high_entry_word != expected_entry_word || high_table_word != expected_upper
        || marker_result != 0x5a5au || !marker_unmapped) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: high alias probe failed\r\n");
        return;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: high kernel alias passed\r\n");
}
