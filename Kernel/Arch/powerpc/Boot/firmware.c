/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/OpenFirmwareMemoryMap.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/OpenFirmwarePCIRanges.h>
#include <Kernel/Arch/powerpc/Pegasos2/RTAS.h>

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
static struct BootMemoryRange s_memory_ranges[32];
static Cell s_memory_range_count;
static struct PowerPCPCIHostBridgeInfo s_pci_host_bridges[POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT];
static Cell s_pci_host_bridge_count;
static struct PowerPCRtasInfo s_rtas_info;
static int s_rtas_power_off_test_requested;
static volatile Cell s_rtas_clock_debug;
static volatile Cell s_rtas_clock_before;
static volatile Cell s_rtas_clock_after;

extern Cell powerpc_kernel_mapping_base(void);
extern int powerpc_setjmp_self_test(void);
extern int powerpc_cpu_features_test(void);
extern int powerpc_processor_test(void);
extern int powerpc_memory_fences_test(void);
extern int powerpc_cache_maintenance_test(void);
extern int powerpc_tlb_maintenance_test(void);
extern int powerpc_vsid_allocator_test(void);
extern int powerpc_firmware_memory_map_test(struct BootMemoryRange const*, unsigned int, unsigned long long);
extern int powerpc_boot_handoff_test(struct BootMemoryRange const*, unsigned int, unsigned long long, Cell, Cell,
    struct PowerPCPCIHostBridgeInfo const*, Cell, struct PowerPCRtasInfo const*);
extern int powerpc_page_mapping_test(void);
extern Cell powerpc_cpu_pvr(void);
extern int powerpc_short_string_layout_test(void);
extern int powerpc_bitmap_endian_test(void);
extern int powerpc_mbr_wire_format_test(void);
extern int powerpc_ext2_wire_format_test(void);
extern int powerpc_float_digit_packing_test(void);
extern int powerpc_float_extractor_endian_test(void);
extern int powerpc_float_parser_test(void);
extern int pci_open_firmware_address_test(void);
extern int pci_address_assignment_test(void);
extern int powerpc_pegasos2_pci_config_test(Cell);
extern volatile Cell powerpc_pegasos2_pci_config_debug;
extern int powerpc_open_firmware_pci_ranges_test(Cell const*, Cell, Cell, Cell);
extern int nvme_endian_test(void);
extern int sd_adma_address_test(void);
extern int powerpc_deferred_key_press_test(void);
extern int powerpc_signal_trampoline_test(void);
extern int powerpc_smap_disabler_test(void);
extern int powerpc_safe_mem_test(void);
extern int powerpc_atomic64_test(void);
extern int powerpc_fenv_test(void);
extern int powerpc_fpu_state_test(void);
extern int powerpc_native_elf_header_test(void);
extern int powerpc_native_elf_image_test(void);
extern int powerpc_relative_relocations_test(void);
extern int powerpc_tls_layout_test(void);
extern int powerpc_static_tls_lookup_test(void);
extern int powerpc_elf_entry_test(void);
extern int powerpc_secure_plt_test(void);
extern int powerpc_elf_breakpoint_test(void);
extern int powerpc_syscall_invoke_test(void);
extern int powerpc_register_state_test(void);
extern int powerpc_thread_registers_test(void);
extern int powerpc_stack_unwinder_test(void);
extern int powerpc_program_exception_roundtrip(void);
extern int powerpc_decrementer_exception_roundtrip(Cell);
extern int powerpc_legacy_pic_irq0_roundtrip(Cell);
extern int powerpc_external_irq_roundtrip(Cell);
extern int powerpc_run_high_stack_external_irq_probe(Cell);
extern int powerpc_mmu_high_alias_roundtrip(void);
extern void powerpc_enable_boot_translation(void);
extern void powerpc_disable_boot_translation(void);
extern int powerpc_hpt_high_alias_roundtrip(void);
extern int powerpc_debug_output_test(void);
extern int powerpc_current_time_fallback_test(void);
extern int powerpc_kernel_context_test(void);
extern int powerpc_assume_context_test(void);
extern volatile Cell powerpc_assume_context_debug;
extern int powerpc_supervisor_exception_return_test(void);
extern int powerpc_initial_thread_context_test(void);
extern int powerpc_hpt_external_irq_stack_roundtrip(Cell);
extern int powerpc_hpt_external_irq_high_handler_roundtrip(Cell);
extern int powerpc_hpt_user_external_irq_observed(void);
extern volatile Cell powerpc_irq_high_debug;
extern int powerpc_hpt_quickmap_roundtrip(void);
extern int powerpc_hpt_physical_array_roundtrip(void);
extern int powerpc_hpt_duplicate_mapping_rejected(void);
extern int powerpc_hpt_mapping_lifecycle(void);
extern int powerpc_hpt_group_reuse(void);
extern int powerpc_hpt_unmapped_fault_observed(void);
extern int powerpc_hpt_read_only_fault_roundtrip(void);
extern int powerpc_hpt_vsid_switch_roundtrip(void);
extern int powerpc_hpt_kernel_protection_roundtrip(void);
extern int powerpc_hpt_user_mode_roundtrip(void);
extern int powerpc_hpt_user_high_vsid_isolation_roundtrip(void);
extern int powerpc_hpt_user_exception_return_roundtrip(void);
extern int powerpc_hpt_translated_user_exception_return_roundtrip(void);
extern int powerpc_hpt_high_frame_user_exception_return_roundtrip(void);
extern int powerpc_hpt_high_frame_atomic_user_return_roundtrip(void);
extern volatile Cell powerpc_hpt_user_high_debug;
extern volatile Cell powerpc_hpt_user_return_debug;
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

static Cell first_child(Cell device)
{
    struct FirmwareCall call = { "child", 1, 1, { device, 0 } };
    if (s_firmware_entry(&call) != 0)
        return 0;
    return call.cells[1];
}

static Cell next_peer(Cell device)
{
    struct FirmwareCall call = { "peer", 1, 1, { device, 0 } };
    if (s_firmware_entry(&call) != 0)
        return 0;
    return call.cells[1];
}

static int get_property(Cell device, char const* name, void* buffer, Cell size)
{
    struct FirmwareCall call = { "getprop", 4, 1, { device, firmware_pointer(name), firmware_pointer(buffer), size, 0 } };
    if (s_firmware_entry(&call) != 0)
        return -1;
    return (int)call.cells[4];
}

static Cell open_device(char const* path)
{
    struct FirmwareCall call = { "open", 1, 1, { firmware_pointer(path), 0 } };
    if (s_firmware_entry(&call) != 0)
        return 0;
    return call.cells[1];
}

static void close_device(Cell instance)
{
    struct FirmwareCall call = { "close", 1, 0, { instance } };
    (void)s_firmware_entry(&call);
}

static Cell claim_pages(Cell size)
{
    struct FirmwareCall call = { "claim", 3, 1, { 0, size, 4096, 0 } };
    if (s_firmware_entry(&call) != 0)
        return 0;
    return call.cells[3];
}

static void release_pages(Cell base, Cell size)
{
    struct FirmwareCall call = { "release", 2, 0, { base, size } };
    (void)s_firmware_entry(&call);
}

static void sync_instruction_cache(Cell start, Cell size)
{
    for (Cell address = start; address < start + size; address += 32)
        __asm__ volatile("dcbst 0,%0" : : "r"(address) : "memory");
    __asm__ volatile("sync" : : : "memory");
    for (Cell address = start; address < start + size; address += 32)
        __asm__ volatile("icbi 0,%0" : : "r"(address) : "memory");
    __asm__ volatile("sync; isync" : : : "memory");
}

static int prepare_rtas(void)
{
    Cell node = find_device("/rtas");
    if (node == (Cell)-1)
        return 0;
    Cell image_size = 0;
    Cell power_off_token = 0;
    Cell reboot_token = 0;
    Cell clock_token = 0;
    if (get_property(node, "rtas-size", &image_size, sizeof(image_size)) != sizeof(image_size)
        || get_property(node, "power-off", &power_off_token, sizeof(power_off_token)) != sizeof(power_off_token)
        || get_property(node, "system-reboot", &reboot_token, sizeof(reboot_token)) != sizeof(reboot_token)
        || image_size == 0 || image_size > 1024 * 1024 - 8192
        || power_off_token == 0 || reboot_token == 0)
        return 0;
    if (get_property(node, "get-time-of-day", &clock_token, sizeof(clock_token)) != sizeof(clock_token))
        clock_token = 0;
    Cell image_allocation_size = (image_size + 4095u) & ~4095u;
    Cell allocation_size = image_allocation_size + 4096;
    Cell base = claim_pages(allocation_size);
    if (base == 0 || base == (Cell)-1 || (base & 4095u) != 0 || base > 0xffffffffu - allocation_size)
        return 0;

    Cell instance = open_device("/rtas");
    if (instance == 0 || instance == (Cell)-1) {
        release_pages(base, allocation_size);
        return 0;
    }
    struct FirmwareCall call = { "call-method", 3, 2,
        { firmware_pointer("instantiate-rtas"), instance, base, (Cell)-1, 0 } };
    int result = s_firmware_entry(&call);
    close_device(instance);
    Cell entry = call.cells[4];
    if (result != 0 || call.cells[3] != 0 || entry < base || entry >= base + image_allocation_size) {
        release_pages(base, allocation_size);
        return 0;
    }
    sync_instruction_cache(base, image_allocation_size);
    s_rtas_info = (struct PowerPCRtasInfo) {
        base, allocation_size, entry, base + image_allocation_size,
        power_off_token, reboot_token, clock_token, 0, 0
    };
    return 1;
}

static int wants_rtas_power_off_test(Cell chosen)
{
    char boot_arguments[64];
    int length = get_property(chosen, "bootargs", boot_arguments, sizeof(boot_arguments));
    char const* expected = "serenity-rtas-power-off-test";
    if (length <= 0 || length >= (int)sizeof(boot_arguments))
        return 0;
    for (int index = 0; expected[index] != '\0'; ++index) {
        if (index >= length || boot_arguments[index] != expected[index])
            return 0;
    }
    return boot_arguments[sizeof("serenity-rtas-power-off-test") - 1] == '\0';
}

static void request_rtas_power_off(void)
{
    struct PowerPCRtasArguments* arguments = (struct PowerPCRtasArguments*)(__UINTPTR_TYPE__)s_rtas_info.arguments_physical;
    arguments->token = s_rtas_info.power_off_token;
    arguments->argument_count = 2;
    arguments->result_count = 1;
    arguments->values[0] = 0;
    arguments->values[1] = 0;
    arguments->values[2] = (Cell)-1;
    __asm__ volatile("sync" : : : "memory");
    powerpc_enable_boot_translation();
    (void)powerpc_rtas_enter(s_rtas_info.arguments_physical, s_rtas_info.entry_physical,
        firmware_pointer(powerpc_rtas_return_real), firmware_pointer(powerpc_rtas_return_virtual));
    powerpc_disable_boot_translation();
}

static int test_translated_rtas_clock(void)
{
    if (s_rtas_info.entry_physical == 0 || s_rtas_info.get_time_of_day_token == 0)
        return 0;
    struct PowerPCRtasArguments* arguments = (struct PowerPCRtasArguments*)(__UINTPTR_TYPE__)s_rtas_info.arguments_physical;
    arguments->token = s_rtas_info.get_time_of_day_token;
    arguments->argument_count = 0;
    arguments->result_count = 8;
    for (Cell index = 0; index < 8; ++index)
        arguments->values[index] = (Cell)-1;
    __asm__ volatile("sync" : : : "memory");

    powerpc_enable_boot_translation();
    Cell translated_before;
    __asm__ volatile("mfmsr %0" : "=r"(translated_before));
    Cell hypercall_status = powerpc_rtas_enter(s_rtas_info.arguments_physical, s_rtas_info.entry_physical,
        firmware_pointer(powerpc_rtas_return_real), firmware_pointer(powerpc_rtas_return_virtual));
    Cell translated_after;
    __asm__ volatile("mfmsr %0" : "=r"(translated_after));
    s_rtas_clock_before = translated_before;
    s_rtas_clock_after = translated_after;
    powerpc_disable_boot_translation();
    __asm__ volatile("sync" : : : "memory");
    // The RTAS entry enables machine checks in QEMU. The guest's
    // privilege, translation, interrupt, and FP state must roundtrip.
    s_rtas_clock_debug = ((translated_before & 0x30u) == 0x30u ? 1u : 0u)
        | (((translated_after ^ translated_before) & ~0x1000u) == 0 ? 2u : 0u)
        | (hypercall_status == 0 ? 4u : 0u)
        | (arguments->values[0] == 0 ? 8u : 0u)
        | (arguments->values[1] >= 2020 && arguments->values[1] < 2100 ? 16u : 0u)
        | (arguments->values[2] >= 1 && arguments->values[2] <= 12 ? 32u : 0u)
        | (arguments->values[3] >= 1 && arguments->values[3] <= 31 ? 64u : 0u);
    return s_rtas_clock_debug == 0x7fu;
}

static Cell discover_mv64361_registers_base(Cell root, Cell* south_bridge)
{
    // The PCI bridge publishes its i8259 acknowledge register inside the
    // controller's 64 KiB register aperture.
    for (Cell child = first_child(root); child != 0 && child != (Cell)-1; child = next_peer(child)) {
        Cell interrupt_acknowledge = 0;
        if (get_property(child, "8259-interrupt-acknowledge", &interrupt_acknowledge, sizeof(interrupt_acknowledge)) == sizeof(interrupt_acknowledge)
            && (interrupt_acknowledge & 0xffffu) == PEGASOS2_INTACK_OFFSET) {
            *south_bridge = child;
            return interrupt_acknowledge & ~0xffffu;
        }
    }
    return 0;
}

static int discover_pci_host_bridges(Cell root, Cell south_bridge)
{
    if (south_bridge == 0)
        return 0;
    Cell parent_address_cells = 1;
    if (get_property(root, "#address-cells", &parent_address_cells, sizeof(parent_address_cells)) != sizeof(parent_address_cells))
        parent_address_cells = 1;
    if (parent_address_cells < 1 || parent_address_cells > 2)
        return 0;

    s_pci_host_bridge_count = 0;
    Cell seen_bridges = 0;
    for (Cell bridge = first_child(root); bridge != 0 && bridge != (Cell)-1; bridge = next_peer(bridge)) {
        char device_type[8] = {};
        int type_length = get_property(bridge, "device_type", device_type, sizeof(device_type));
        if (type_length < 3 || device_type[0] != 'p' || device_type[1] != 'c' || device_type[2] != 'i'
            || (type_length > 3 && device_type[3] != '\0'))
            continue;

        Cell bridge_number;
        if (get_property(bridge, "pci-bridge-number", &bridge_number, sizeof(bridge_number)) != sizeof(bridge_number))
            bridge_number = bridge == south_bridge ? 0 : 1;
        if (bridge_number >= POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT || (seen_bridges & (1u << bridge_number))
            || (bridge == south_bridge && bridge_number != 0))
            return 0;

        Cell child_address_cells = 0;
        Cell size_cells = 0;
        if (get_property(bridge, "#address-cells", &child_address_cells, sizeof(child_address_cells)) != sizeof(child_address_cells)
            || get_property(bridge, "#size-cells", &size_cells, sizeof(size_cells)) != sizeof(size_cells)
            || child_address_cells != 3)
            return 0;

        Cell bus_range[2] = { 0, 255 };
        int bus_length = get_property(bridge, "bus-range", bus_range, sizeof(bus_range));
        if (bus_length != -1 && bus_length != sizeof(bus_range))
            return 0;
        if (bus_range[0] > bus_range[1] || bus_range[1] > 255)
            return 0;

        Cell cells[96];
        int length = get_property(bridge, "ranges", cells, sizeof(cells));
        if (length <= 0 || length % sizeof(Cell) != 0)
            return 0;
        struct PowerPCPCIHostBridgeInfo* info = &s_pci_host_bridges[bridge_number];
        if (!powerpc_parse_open_firmware_pci_ranges(cells, (Cell)length / sizeof(Cell),
                child_address_cells, parent_address_cells, size_cells, info->ranges,
                POWERPC_PEGASOS2_PCI_MAX_RANGES, &info->range_count))
            return 0;
        if (bridge_number == 0 && !powerpc_open_firmware_pci_ranges_test(cells, (Cell)length / sizeof(Cell), parent_address_cells, size_cells))
            return 0;
        info->bridge_number = bridge_number;
        info->start_bus = bus_range[0];
        info->end_bus = bus_range[1];
        seen_bridges |= 1u << bridge_number;
        ++s_pci_host_bridge_count;
    }
    return seen_bridges == 3;
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

    s_uart = (volatile unsigned char*)PEGASOS2_UART_PHYSICAL_BASE;
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

static int discover_memory_size(Cell root, unsigned long long* total_bytes)
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
    if (length <= 0 || length % sizeof(Cell) != 0)
        return 0;

    return powerpc_parse_open_firmware_memory_reg(ranges, (Cell)length / sizeof(Cell),
        address_cells, size_cells, s_memory_ranges,
        sizeof(s_memory_ranges) / sizeof(s_memory_ranges[0]), &s_memory_range_count, total_bytes);
}

static Cell discover_timebase_frequency(void)
{
    Cell cpus = find_device("/cpus");
    if (cpus == (Cell)-1)
        return 0;
    Cell cpu = first_child(cpus);
    Cell frequency = 0;
    if (cpu == 0 || cpu == (Cell)-1 || get_property(cpu, "timebase-frequency", &frequency, sizeof(frequency)) != sizeof(frequency))
        return 0;
    return frequency;
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
        Cell timebase_frequency = discover_timebase_frequency();
        Cell south_bridge = 0;
        Cell mv64361_registers_base = discover_mv64361_registers_base(root, &south_bridge);
        s_rtas_power_off_test_requested = wants_rtas_power_off_test(chosen);
        if (prepare_rtas())
            serial_write("SerenityOS PowerPC: RTAS runtime prepared\r\n");
        else
            serial_write("SerenityOS PowerPC: RTAS runtime unavailable\r\n");
        if (powerpc_setjmp_self_test())
            serial_write("SerenityOS PowerPC: setjmp roundtrip passed\r\n");
        else
            serial_write("SerenityOS PowerPC: setjmp roundtrip failed\r\n");
        if (powerpc_cpu_features_test())
            serial_write("SerenityOS PowerPC: CPU features passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: CPU features failed PVR 0x");
            serial_write_hex(powerpc_cpu_pvr());
            serial_write("\r\n");
        }
        if (powerpc_processor_test())
            serial_write("SerenityOS PowerPC: processor primitives passed\r\n");
        else
            serial_write("SerenityOS PowerPC: processor primitives failed\r\n");
        if (powerpc_memory_fences_test())
            serial_write("SerenityOS PowerPC: memory fences passed\r\n");
        else
            serial_write("SerenityOS PowerPC: memory fences failed\r\n");
        if (powerpc_vsid_allocator_test())
            serial_write("SerenityOS PowerPC: VSID allocator passed\r\n");
        else
            serial_write("SerenityOS PowerPC: VSID allocator failed\r\n");
        if (powerpc_page_mapping_test())
            serial_write("SerenityOS PowerPC: page mapping transaction passed\r\n");
        else
            serial_write("SerenityOS PowerPC: page mapping transaction failed\r\n");
        if (powerpc_cache_maintenance_test())
            serial_write("SerenityOS PowerPC: cache maintenance passed\r\n");
        else
            serial_write("SerenityOS PowerPC: cache maintenance failed\r\n");
        if (powerpc_short_string_layout_test())
            serial_write("SerenityOS PowerPC: big-endian short string passed\r\n");
        else
            serial_write("SerenityOS PowerPC: big-endian short string failed\r\n");
        if (powerpc_bitmap_endian_test())
            serial_write("SerenityOS PowerPC: big-endian bitmap search passed\r\n");
        else
            serial_write("SerenityOS PowerPC: big-endian bitmap search failed\r\n");
        if (powerpc_mbr_wire_format_test())
            serial_write("SerenityOS PowerPC: little-endian MBR fields passed\r\n");
        else
            serial_write("SerenityOS PowerPC: little-endian MBR fields failed\r\n");
        if (powerpc_ext2_wire_format_test())
            serial_write("SerenityOS PowerPC: little-endian ext2 fields passed\r\n");
        else
            serial_write("SerenityOS PowerPC: little-endian ext2 fields failed\r\n");
        if (powerpc_float_digit_packing_test())
            serial_write("SerenityOS PowerPC: big-endian float digits passed\r\n");
        else
            serial_write("SerenityOS PowerPC: big-endian float digits failed\r\n");
        if (powerpc_float_extractor_endian_test())
            serial_write("SerenityOS PowerPC: big-endian float extraction passed\r\n");
        else
            serial_write("SerenityOS PowerPC: big-endian float extraction failed\r\n");
        if (powerpc_float_parser_test())
            serial_write("SerenityOS PowerPC: big-endian float parser passed\r\n");
        else
            serial_write("SerenityOS PowerPC: big-endian float parser failed\r\n");
        if (pci_open_firmware_address_test())
            serial_write("SerenityOS PowerPC: PCI Open Firmware address passed\r\n");
        else
            serial_write("SerenityOS PowerPC: PCI Open Firmware address failed\r\n");
        if (pci_address_assignment_test())
            serial_write("SerenityOS PowerPC: PCI 64-bit address assignment passed\r\n");
        else
            serial_write("SerenityOS PowerPC: PCI 64-bit address assignment failed\r\n");
        if (powerpc_pegasos2_pci_config_test(mv64361_registers_base))
            serial_write("SerenityOS PowerPC: Pegasos 2 PCI config access passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: Pegasos 2 PCI config access failed 0x");
            serial_write_hex(powerpc_pegasos2_pci_config_debug);
            serial_write("\r\n");
        }
        if (discover_pci_host_bridges(root, south_bridge))
            serial_write("SerenityOS PowerPC: Pegasos 2 PCI firmware ranges passed\r\n");
        else
            serial_write("SerenityOS PowerPC: Pegasos 2 PCI firmware ranges failed\r\n");
        if (nvme_endian_test())
            serial_write("SerenityOS PowerPC: NVMe little-endian wire format passed\r\n");
        else
            serial_write("SerenityOS PowerPC: NVMe little-endian wire format failed\r\n");
        if (sd_adma_address_test())
            serial_write("SerenityOS PowerPC: SD ADMA physical addresses passed\r\n");
        else
            serial_write("SerenityOS PowerPC: SD ADMA physical addresses failed\r\n");
        if (powerpc_deferred_key_press_test())
            serial_write("SerenityOS PowerPC: deferred key press passed\r\n");
        else
            serial_write("SerenityOS PowerPC: deferred key press failed\r\n");
        if (powerpc_signal_trampoline_test())
            serial_write("SerenityOS PowerPC: signal trampoline layout passed\r\n");
        else
            serial_write("SerenityOS PowerPC: signal trampoline layout failed\r\n");
        if (powerpc_smap_disabler_test())
            serial_write("SerenityOS PowerPC: supervisor access guard passed\r\n");
        else
            serial_write("SerenityOS PowerPC: supervisor access guard failed\r\n");
        if (powerpc_safe_mem_test())
            serial_write("SerenityOS PowerPC: safe memory operations passed\r\n");
        else
            serial_write("SerenityOS PowerPC: safe memory operations failed\r\n");
        if (powerpc_atomic64_test())
            serial_write("SerenityOS PowerPC: 64-bit atomics passed\r\n");
        else
            serial_write("SerenityOS PowerPC: 64-bit atomics failed\r\n");
        int fenv_result = powerpc_fenv_test();
        if (fenv_result == 1)
            serial_write("SerenityOS PowerPC: FPSCR environment passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: FPSCR environment failed 0x");
            serial_write_hex(fenv_result);
            serial_write("\r\n");
        }
        if (powerpc_fpu_state_test())
            serial_write("SerenityOS PowerPC: FPU state roundtrip passed\r\n");
        else
            serial_write("SerenityOS PowerPC: FPU state roundtrip failed\r\n");
        if (powerpc_native_elf_header_test())
            serial_write("SerenityOS PowerPC: ELF32 big-endian header passed\r\n");
        else
            serial_write("SerenityOS PowerPC: ELF32 big-endian header failed\r\n");
        if (powerpc_native_elf_image_test())
            serial_write("SerenityOS PowerPC: ELF32 big-endian image passed\r\n");
        else
            serial_write("SerenityOS PowerPC: ELF32 big-endian image failed\r\n");
        if (powerpc_relative_relocations_test())
            serial_write("SerenityOS PowerPC: ELF32 relative relocations passed\r\n");
        else
            serial_write("SerenityOS PowerPC: ELF32 relative relocations failed\r\n");
        if (powerpc_tls_layout_test())
            serial_write("SerenityOS PowerPC: PPC32 TLS layout passed\r\n");
        else
            serial_write("SerenityOS PowerPC: PPC32 TLS layout failed\r\n");
        if (powerpc_static_tls_lookup_test())
            serial_write("SerenityOS PowerPC: PPC32 static TLS lookup passed\r\n");
        else
            serial_write("SerenityOS PowerPC: PPC32 static TLS lookup failed\r\n");
        if (powerpc_elf_entry_test())
            serial_write("SerenityOS PowerPC: ELF32 entry transfer passed\r\n");
        else
            serial_write("SerenityOS PowerPC: ELF32 entry transfer failed\r\n");
        int secure_plt_result = powerpc_secure_plt_test();
        if (secure_plt_result == 1)
            serial_write("SerenityOS PowerPC: secure PLT lazy binding passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: secure PLT lazy binding failed 0x");
            serial_write_hex(secure_plt_result);
            serial_write("\r\n");
        }
        serial_write("SerenityOS PowerPC: 32-bit kernel base 0x");
        serial_write_hex(powerpc_kernel_mapping_base());
        serial_write("\r\n");
        unsigned long long total_bytes = 0;
        if (discover_memory_size(root, &total_bytes)) {
            serial_write("SerenityOS PowerPC: RAM ");
            serial_write_decimal((Cell)(total_bytes >> 20));
            serial_write(" MiB\r\n");
            if (powerpc_firmware_memory_map_test(s_memory_ranges, s_memory_range_count, total_bytes))
                serial_write("SerenityOS PowerPC: firmware memory ranges passed\r\n");
            else
                serial_write("SerenityOS PowerPC: firmware memory ranges failed\r\n");
        } else {
            serial_write("SerenityOS PowerPC: RAM unavailable\r\n");
        }
        if (powerpc_program_exception_roundtrip())
            serial_write("SerenityOS PowerPC: program exception roundtrip passed\r\n");
        else
            serial_write("SerenityOS PowerPC: program exception roundtrip failed\r\n");
        if (powerpc_decrementer_exception_roundtrip(timebase_frequency))
            serial_write("SerenityOS PowerPC: decrementer interrupt passed\r\n");
        else
            serial_write("SerenityOS PowerPC: decrementer interrupt failed\r\n");
        if (powerpc_legacy_pic_irq0_roundtrip(timebase_frequency))
            serial_write("SerenityOS PowerPC: legacy PIC IRQ0 acknowledge passed\r\n");
        else
            serial_write("SerenityOS PowerPC: legacy PIC IRQ0 acknowledge failed\r\n");
        if (powerpc_external_irq_roundtrip(timebase_frequency))
            serial_write("SerenityOS PowerPC: external IRQ0 roundtrip passed\r\n");
        else
            serial_write("SerenityOS PowerPC: external IRQ0 roundtrip failed\r\n");
        if (powerpc_elf_breakpoint_test())
            serial_write("SerenityOS PowerPC: loader breakpoint passed\r\n");
        else
            serial_write("SerenityOS PowerPC: loader breakpoint failed\r\n");
        if (powerpc_syscall_invoke_test())
            serial_write("SerenityOS PowerPC: syscall register ABI passed\r\n");
        else
            serial_write("SerenityOS PowerPC: syscall register ABI failed\r\n");
        if (powerpc_mmu_high_alias_roundtrip())
            serial_write("SerenityOS PowerPC: MMU high alias passed\r\n");
        else
            serial_write("SerenityOS PowerPC: MMU high alias failed\r\n");
        if (test_translated_rtas_clock())
            serial_write("SerenityOS PowerPC: translated RTAS clock passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: translated RTAS clock failed 0x");
            serial_write_hex(s_rtas_clock_debug);
            serial_write(" MSR ");
            serial_write_hex(s_rtas_clock_before);
            serial_write("/");
            serial_write_hex(s_rtas_clock_after);
            serial_write("\r\n");
        }
        if (powerpc_run_high_stack_external_irq_probe(timebase_frequency))
            serial_write("SerenityOS PowerPC: translated external IRQ stack passed\r\n");
        else
            serial_write("SerenityOS PowerPC: translated external IRQ stack failed\r\n");
        if (powerpc_hpt_high_alias_roundtrip())
            serial_write("SerenityOS PowerPC: HPT high alias passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT high alias failed\r\n");
        if (!powerpc_debug_output_test())
            serial_write("SerenityOS PowerPC: kernel debug UART failed\r\n");
        if (powerpc_current_time_fallback_test())
            serial_write("SerenityOS PowerPC: wall-clock fallback passed\r\n");
        else
            serial_write("SerenityOS PowerPC: wall-clock fallback failed\r\n");
        if (powerpc_kernel_context_test())
            serial_write("SerenityOS PowerPC: kernel context switch passed\r\n");
        else
            serial_write("SerenityOS PowerPC: kernel context switch failed\r\n");
        if (powerpc_assume_context_test())
            serial_write("SerenityOS PowerPC: assumed context stack transfer passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: assumed context stack transfer failed 0x");
            serial_write_hex(powerpc_assume_context_debug);
            serial_write("\r\n");
        }
        if (powerpc_supervisor_exception_return_test())
            serial_write("SerenityOS PowerPC: supervisor exception return passed\r\n");
        else
            serial_write("SerenityOS PowerPC: supervisor exception return failed\r\n");
        if (powerpc_initial_thread_context_test())
            serial_write("SerenityOS PowerPC: initial thread frame passed\r\n");
        else
            serial_write("SerenityOS PowerPC: initial thread frame failed\r\n");
        if (powerpc_hpt_external_irq_stack_roundtrip(timebase_frequency))
            serial_write("SerenityOS PowerPC: HPT external IRQ stack passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT external IRQ stack failed\r\n");
        if (powerpc_hpt_external_irq_high_handler_roundtrip(timebase_frequency))
            serial_write("SerenityOS PowerPC: HPT high IRQ handler passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: HPT high IRQ handler failed checks 0x");
            serial_write_hex(powerpc_irq_high_debug);
            serial_write("\r\n");
        }
        if (powerpc_hpt_user_external_irq_observed())
            serial_write("SerenityOS PowerPC: user-mode external IRQ resume passed\r\n");
        else
            serial_write("SerenityOS PowerPC: user-mode external IRQ resume failed\r\n");
        if (powerpc_boot_handoff_test(s_memory_ranges, s_memory_range_count, total_bytes, timebase_frequency,
                mv64361_registers_base, s_pci_host_bridges, s_pci_host_bridge_count, &s_rtas_info))
            serial_write("SerenityOS PowerPC: HPT, timebase, PCI, and RTAS handoff passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT, timebase, PCI, and RTAS handoff failed\r\n");
        if (powerpc_hpt_quickmap_roundtrip())
            serial_write("SerenityOS PowerPC: HPT quickmap alias passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT quickmap alias failed\r\n");
        if (powerpc_hpt_physical_array_roundtrip())
            serial_write("SerenityOS PowerPC: HPT physical metadata array passed\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT physical metadata array failed\r\n");
        if (powerpc_hpt_duplicate_mapping_rejected())
            serial_write("SerenityOS PowerPC: HPT duplicate mapping rejected\r\n");
        else
            serial_write("SerenityOS PowerPC: HPT duplicate mapping accepted\r\n");
        if (powerpc_tlb_maintenance_test())
            serial_write("SerenityOS PowerPC: processor TLB maintenance passed\r\n");
        else
            serial_write("SerenityOS PowerPC: processor TLB maintenance failed\r\n");
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
        if (powerpc_hpt_user_high_vsid_isolation_roundtrip())
            serial_write("SerenityOS PowerPC: user high VSID isolation passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: user high VSID isolation failed checks 0x");
            serial_write_hex(powerpc_hpt_user_high_debug);
            serial_write("\r\n");
        }
        if (powerpc_hpt_user_exception_return_roundtrip())
            serial_write("SerenityOS PowerPC: real-mode user frame return passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: real-mode user frame return failed checks 0x");
            serial_write_hex(powerpc_hpt_user_return_debug);
            serial_write("\r\n");
        }
        if (powerpc_hpt_translated_user_exception_return_roundtrip())
            serial_write("SerenityOS PowerPC: translated supervisor user return passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: translated supervisor user return failed checks 0x");
            serial_write_hex(powerpc_hpt_user_return_debug);
            serial_write("\r\n");
        }
        if (powerpc_hpt_high_frame_user_exception_return_roundtrip())
            serial_write("SerenityOS PowerPC: high-frame user return passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: high-frame user return failed checks 0x");
            serial_write_hex(powerpc_hpt_user_return_debug);
            serial_write("\r\n");
        }
        if (powerpc_hpt_high_frame_atomic_user_return_roundtrip())
            serial_write("SerenityOS PowerPC: high-frame atomic user return passed\r\n");
        else {
            serial_write("SerenityOS PowerPC: high-frame atomic user return failed checks 0x");
            serial_write_hex(powerpc_hpt_user_return_debug);
            serial_write("\r\n");
        }
        if (powerpc_register_state_test())
            serial_write("SerenityOS PowerPC: register state from user trap passed\r\n");
        else
            serial_write("SerenityOS PowerPC: register state from user trap failed\r\n");
        if (powerpc_thread_registers_test())
            serial_write("SerenityOS PowerPC: thread register setup passed\r\n");
        else
            serial_write("SerenityOS PowerPC: thread register setup failed\r\n");
        if (powerpc_stack_unwinder_test())
            serial_write("SerenityOS PowerPC: stack backchain unwinder passed\r\n");
        else
            serial_write("SerenityOS PowerPC: stack backchain unwinder failed\r\n");
        if (s_rtas_power_off_test_requested && s_rtas_info.entry_physical != 0) {
            serial_write("SerenityOS PowerPC: RTAS power-off requested\r\n");
            request_rtas_power_off();
            serial_write("SerenityOS PowerPC: RTAS power-off returned\r\n");
        }
    }
}
