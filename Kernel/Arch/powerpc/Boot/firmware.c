/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

typedef unsigned int Cell;
typedef int (*FirmwareEntry)(void*);

struct FirmwareCall {
    const char* service;
    Cell argument_count;
    Cell result_count;
    Cell cells[5];
};

static FirmwareEntry s_firmware_entry;
static Cell s_stdout;
static char s_model[64];
static volatile unsigned char* s_uart;

extern Cell powerpc_kernel_mapping_base(void);
extern int powerpc_setjmp_self_test(void);
extern int powerpc_short_string_layout_test(void);
extern int powerpc_float_digit_packing_test(void);
extern int powerpc_program_exception_roundtrip(void);
extern int powerpc_mmu_high_alias_roundtrip(void);
extern int powerpc_hpt_high_alias_roundtrip(void);
extern int powerpc_hpt_duplicate_mapping_rejected(void);
extern int powerpc_hpt_mapping_lifecycle(void);
extern int powerpc_hpt_group_reuse(void);
extern int powerpc_hpt_unmapped_fault_observed(void);
extern int powerpc_hpt_read_only_fault_roundtrip(void);
extern int powerpc_hpt_vsid_switch_roundtrip(void);
extern int powerpc_hpt_kernel_protection_roundtrip(void);
extern int powerpc_hpt_user_mode_roundtrip(void);
extern int powerpc_user_stack_observed(void);
extern int powerpc_user_syscall_resume_observed(void);
extern int powerpc_user_exception_frame_observed(void);

static int is_pegasos2(void)
{
    char const* expected = "Pegasos2";
    for (Cell index = 0; expected[index] != '\0'; ++index) {
        if (s_model[index] != expected[index])
            return 0;
    }
    return s_model[8] == '\0';
}

static Cell firmware_pointer(void const* pointer)
{
    return (Cell)(__UINTPTR_TYPE__)pointer;
}

static Cell find_device(char const* path)
{
    struct FirmwareCall call = { "finddevice", 1, 1, { firmware_pointer(path), 0 } };
    if (s_firmware_entry(&call) != 0)
        return (Cell)-1;
    return call.cells[1];
}

static int get_property(Cell device, char const* name, void* buffer, Cell size)
{
    struct FirmwareCall call = { "getprop", 4, 1, { device, firmware_pointer(name), firmware_pointer(buffer), size, 0 } };
    if (s_firmware_entry(&call) != 0)
        return -1;
    return (int)call.cells[4];
}

static void write_console(char const* string)
{
    Cell length = 0;
    while (string[length] != '\0')
        ++length;

    struct FirmwareCall call = { "write", 3, 1, { s_stdout, firmware_pointer(string), length, 0 } };
    (void)s_firmware_entry(&call);
}

static void uart_write_register(volatile unsigned char* uart, Cell index, unsigned char value)
{
    uart[index] = value;
    __asm__ volatile("eieio" : : : "memory");
}

static void serial_write(char const* message)
{
    for (; *message != '\0'; ++message) {
        Cell attempts = 100000;
        while ((s_uart[5] & 0x20) == 0 && --attempts != 0) { }
        if (attempts == 0)
            return;
        uart_write_register(s_uart, 0, *message);
    }
}

static void serial_write_decimal(Cell value)
{
    char digits[10];
    Cell length = 0;
    do {
        digits[length++] = '0' + value % 10;
        value /= 10;
    } while (value != 0);

    while (length != 0) {
        char character[2] = { digits[--length], '\0' };
        serial_write(character);
    }
}

static void serial_write_hex(Cell value)
{
    char digits[9];
    char const* hex_digits = "0123456789abcdef";
    digits[8] = '\0';
    for (int index = 7; index >= 0; --index) {
        digits[index] = hex_digits[value & 0xf];
        value >>= 4;
    }
    serial_write(digits);
}

static int initialize_qemu_pegasos2_serial(void)
{
    // QEMU's VOF enters in real mode. Its Pegasos 2 PCI I/O window exposes
    // the VIA 8250-compatible UART at COM2 (0xfe000000 + 0x2f8).
    Cell machine_state;
    __asm__ volatile("mfmsr %0" : "=r"(machine_state));
    if ((machine_state & 0x30) != 0)
        return 0;

    s_uart = (volatile unsigned char*)0xfe0002f8u;
    uart_write_register(s_uart, 1, 0);
    uart_write_register(s_uart, 3, 0x80);
    uart_write_register(s_uart, 0, 1);
    uart_write_register(s_uart, 1, 0);
    uart_write_register(s_uart, 3, 3);
    uart_write_register(s_uart, 2, 7);
    uart_write_register(s_uart, 4, 3);
    serial_write("SerenityOS PowerPC: serial online\r\n");
    return 1;
}

static int discover_memory_size(Cell root, Cell* total_bytes)
{
    Cell address_cells = 1;
    Cell size_cells = 1;
    if (get_property(root, "#address-cells", &address_cells, sizeof(address_cells)) != sizeof(address_cells))
        address_cells = 1;
    if (get_property(root, "#size-cells", &size_cells, sizeof(size_cells)) != sizeof(size_cells))
        size_cells = 1;
    if (address_cells < 1 || address_cells > 2 || size_cells < 1 || size_cells > 2)
        return 0;

    Cell memory = find_device("/memory@0");
    if (memory == (Cell)-1)
        memory = find_device("/memory");
    if (memory == (Cell)-1)
        return 0;

    Cell ranges[64];
    int length = get_property(memory, "reg", ranges, sizeof(ranges));
    Cell stride = address_cells + size_cells;
    if (length <= 0 || length % sizeof(Cell) != 0 || (Cell)length / sizeof(Cell) % stride != 0)
        return 0;

    Cell total = 0;
    for (Cell index = 0; index < (Cell)length / sizeof(Cell); index += stride) {
        if ((address_cells == 2 && ranges[index] != 0) || (size_cells == 2 && ranges[index + address_cells] != 0))
            return 0;
        Cell base = ranges[index + address_cells - 1];
        Cell size = ranges[index + stride - 1];
        if (size > (Cell)-1 - base || size > (Cell)-1 - total)
            return 0;
        total += size;
    }
    *total_bytes = total;
    return total != 0;
}

void boot_main(FirmwareEntry firmware_entry)
{
    s_firmware_entry = firmware_entry;
    Cell chosen = find_device("/chosen");
    Cell root = find_device("/");
    if (chosen == (Cell)-1 || root == (Cell)-1)
        return;
    if (get_property(chosen, "stdout", &s_stdout, sizeof(s_stdout)) != sizeof(s_stdout))
        return;

    write_console("SerenityOS PowerPC: Open Firmware entered\r\n");
    int length = get_property(root, "model", s_model, sizeof(s_model) - 1);
    if (length <= 0 || length >= (int)sizeof(s_model)) {
        write_console("SerenityOS PowerPC: model unavailable\r\n");
        return;
    }
    s_model[length] = '\0';
    write_console("SerenityOS PowerPC: model ");
    write_console(s_model);
    write_console("\r\n");

    Cell qemu_boot_kernel[4];
    if (is_pegasos2() && get_property(chosen, "qemu,boot-kernel", qemu_boot_kernel, sizeof(qemu_boot_kernel)) == sizeof(qemu_boot_kernel) && initialize_qemu_pegasos2_serial()) {
        if (powerpc_setjmp_self_test())
            serial_write("SerenityOS PowerPC: setjmp roundtrip passed\r\n");
        else
            serial_write("SerenityOS PowerPC: setjmp roundtrip failed\r\n");
        if (powerpc_short_string_layout_test())
            serial_write("SerenityOS PowerPC: big-endian short string passed\r\n");
        else
            serial_write("SerenityOS PowerPC: big-endian short string failed\r\n");
        if (powerpc_float_digit_packing_test())
            serial_write("SerenityOS PowerPC: big-endian float digits passed\r\n");
        else
            serial_write("SerenityOS PowerPC: big-endian float digits failed\r\n");
        serial_write("SerenityOS PowerPC: 32-bit kernel base 0x");
        serial_write_hex(powerpc_kernel_mapping_base());
        serial_write("\r\n");
        Cell total_bytes;
        if (discover_memory_size(root, &total_bytes)) {
            serial_write("SerenityOS PowerPC: RAM ");
            serial_write_decimal(total_bytes >> 20);
            serial_write(" MiB\r\n");
        } else {
            serial_write("SerenityOS PowerPC: RAM unavailable\r\n");
        }
        if (powerpc_program_exception_roundtrip())
            serial_write("SerenityOS PowerPC: program exception roundtrip passed\r\n");
        else
            serial_write("SerenityOS PowerPC: program exception roundtrip failed\r\n");
        if (powerpc_mmu_high_alias_roundtrip())
            serial_write("SerenityOS PowerPC: MMU high alias passed\r\n");
        else
            serial_write("SerenityOS PowerPC: MMU high alias failed\r\n");
        if (powerpc_hpt_high_alias_roundtrip())
            serial_write("SerenityOS PowerPC: HPT high alias passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT high alias failed\r\n");
        if (powerpc_hpt_duplicate_mapping_rejected())
            serial_write("SerenityOS PowerPC: HPT duplicate mapping rejected\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT duplicate mapping accepted\r\n");
        if (powerpc_hpt_mapping_lifecycle())
            serial_write("SerenityOS PowerPC: HPT remap and secondary hash passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT remap and secondary hash failed\r\n");
        if (powerpc_hpt_group_reuse())
            serial_write("SerenityOS PowerPC: HPT group reuse passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT group reuse failed\r\n");
        if (powerpc_hpt_unmapped_fault_observed())
            serial_write("SerenityOS PowerPC: HPT unmapped fault passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT unmapped fault failed\r\n");
        if (powerpc_hpt_read_only_fault_roundtrip())
            serial_write("SerenityOS PowerPC: HPT read-only fault passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT read-only fault failed\r\n");
        if (powerpc_hpt_vsid_switch_roundtrip())
            serial_write("SerenityOS PowerPC: HPT VSID switch passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT VSID switch failed\r\n");
        if (powerpc_hpt_kernel_protection_roundtrip())
            serial_write("SerenityOS PowerPC: HPT kernel protection passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT kernel protection failed\r\n");
        if (powerpc_hpt_user_mode_roundtrip())
            serial_write("SerenityOS PowerPC: user-mode page protection passed\r\n");
        else
            serial_write("SerenityOS PowerPC: user-mode page protection failed\r\n");
        if (powerpc_user_stack_observed())
            serial_write("SerenityOS PowerPC: user-mode stack passed\r\n");
        else
            serial_write("SerenityOS PowerPC: user-mode stack failed\r\n");
        if (powerpc_user_syscall_resume_observed())
            serial_write("SerenityOS PowerPC: user-mode syscall resume passed\r\n");
        else
            serial_write("SerenityOS PowerPC: user-mode syscall resume failed\r\n");
        if (powerpc_user_exception_frame_observed())
            serial_write("SerenityOS PowerPC: user-mode exception frame passed\r\n");
        else
            serial_write("SerenityOS PowerPC: user-mode exception frame failed\r\n");
    }
}
