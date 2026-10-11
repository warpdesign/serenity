/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Loader/KernelImagePlan.h>

using FirmwareEntry = int (*)(void*);

struct FirmwareCall {
    char const* service;
    u32 argument_count;
    u32 result_count;
    u32 cells[5];
};

extern "C" u8 _binary_kernel_elf_start[];
extern "C" u8 _binary_kernel_elf_end[];

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

static u32 claim_pages(FirmwareEntry firmware, u32 size)
{
    FirmwareCall call { "claim", 3, 1, { 0, size, 2 * 1024 * 1024, 0 } };
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
    u32 physical_base = claim_pages(firmware, span);
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
}
