/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Loader/AvailableMemoryClaim.h>
#include <Kernel/Arch/powerpc/Loader/FirmwarePropertyValue.h>
#include <Kernel/Arch/powerpc/Loader/Inflate.h>
#include <Kernel/Arch/powerpc/Loader/KernelImagePlan.h>
#include <Kernel/Arch/powerpc/Loader/KernelMappings.h>
#include <Kernel/Arch/powerpc/HashPageTable.h>
#include <Kernel/Arch/powerpc/BootHandoff.h>
#include <Kernel/Arch/powerpc/OpenFirmwareMemoryMap.h>
#include <Kernel/Arch/powerpc/Pegasos2/HardwareAddresses.h>
#include <Kernel/Arch/powerpc/Pegasos2/OpenFirmwarePCIRanges.h>

using FirmwareEntry = int (*)(void*);

struct FirmwareCall {
    char const* service;
    u32 argument_count;
    u32 result_count;
    u32 cells[5];
};

// The kernel ELF is embedded gzip-compressed to keep the image SmartFirmware reads from disk small.
extern "C" u8 _binary_kernel_elf_gz_start[];
extern "C" u8 _binary_kernel_elf_gz_end[];
extern "C" u32 powerpc_loader_high_marker();
extern "C" int powerpc_loader_call_firmware_real_mode(void* arguments, FirmwareEntry entry);
extern "C" void powerpc_loader_enable_translation(u32 sdr1);
extern "C" void powerpc_loader_disable_translation();
extern "C" u8 __loader_start[];
extern "C" u8 __stack_end[];

static BootMemoryRange s_memory_ranges[32];
static BootMemoryRange s_available_ranges[32];
static FirmwareEntry s_firmware_entry;
// SmartFirmware declares /options/real-mode? and must then be called with IR and DR clear.
static bool s_firmware_real_mode;

static int call_firmware(void* arguments)
{
    if (s_firmware_real_mode)
        return powerpc_loader_call_firmware_real_mode(arguments, s_firmware_entry);
    return s_firmware_entry(arguments);
}
static BootMemoryRange s_reserved_ranges[4];
static char s_command_line[256];

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

// Claims memory at an explicit address. QEMU's Virtual Open Firmware claims only the sum of the loader's
// segment sizes, which falls short of its bss and stack, so memory directly after the loader must not
// come from an address-unspecified claim.
static u32 claim_pages_at(FirmwareEntry firmware, u32 address, u32 size)
{
    FirmwareCall call { "claim", 3, 1, { address, size, 0, 0 } };
    if (firmware(&call) != 0 || call.cells[3] != address)
        return 0;
    return address;
}

static int get_property(FirmwareEntry firmware, u32 node, char const* name, void* buffer, u32 size)
{
    FirmwareCall call { "getprop", 4, 1,
        { node, firmware_pointer(name), firmware_pointer(buffer), size, 0 } };
    if (firmware(&call) != 0)
        return -1;
    return static_cast<int>(call.cells[4]);
}

static u32 first_child(FirmwareEntry firmware, u32 node)
{
    FirmwareCall call { "child", 1, 1, { node, 0 } };
    if (firmware(&call) != 0)
        return 0;
    return call.cells[1];
}

static u32 next_peer(FirmwareEntry firmware, u32 node)
{
    FirmwareCall call { "peer", 1, 1, { node, 0 } };
    if (firmware(&call) != 0)
        return 0;
    return call.cells[1];
}

static bool discover_memory_ranges(FirmwareEntry firmware, u32 root, char const* property,
    BootMemoryRange* ranges, u32& range_count)
{
    u32 address_cells = 1;
    u32 size_cells = 1;
    if (get_property(firmware, root, "#address-cells", &address_cells, sizeof(address_cells)) != sizeof(address_cells))
        address_cells = 1;
    if (get_property(firmware, root, "#size-cells", &size_cells, sizeof(size_cells)) != sizeof(size_cells))
        size_cells = 1;
    u32 memory = find_device(firmware, "/memory@0");
    if (memory == 0 || memory == 0xffffffffu)
        memory = find_device(firmware, "/memory");
    if (memory == 0 || memory == 0xffffffffu)
        return false;
    u32 cells[64];
    int length = get_property(firmware, memory, property, cells, sizeof(cells));
    if (length <= 0 || length > static_cast<int>(sizeof(cells)) || length % sizeof(u32) != 0)
        return false;
    unsigned long long total_bytes;
    return powerpc_parse_open_firmware_memory_reg(cells, length / sizeof(u32),
        address_cells, size_cells, ranges, 32, &range_count, &total_bytes);
}

static u32 discover_timebase_frequency(FirmwareEntry firmware)
{
    u32 cpus = find_device(firmware, "/cpus");
    if (cpus == 0 || cpus == 0xffffffffu)
        return 0;
    u32 cpu = first_child(firmware, cpus);
    u32 frequency = 0;
    if (cpu == 0 || cpu == 0xffffffffu
        || get_property(firmware, cpu, "timebase-frequency", &frequency, sizeof(frequency)) != sizeof(frequency))
        return 0;
    return frequency;
}

static u32 discover_controller_base(FirmwareEntry firmware, u32 root, u32& south_bridge)
{
    south_bridge = 0;
    for (u32 child = first_child(firmware, root); child != 0 && child != 0xffffffffu; child = next_peer(firmware, child)) {
        u32 acknowledge = 0;
        if (get_property(firmware, child, "8259-interrupt-acknowledge", &acknowledge, sizeof(acknowledge)) == sizeof(acknowledge)
            && (acknowledge & 0xffffu) == PEGASOS2_INTACK_OFFSET) {
            south_bridge = child;
            return acknowledge & ~0xffffu;
        }
    }
    return 0;
}

static bool discover_pci_host_bridges(FirmwareEntry firmware, u32 root, u32 south_bridge,
    PowerPCPCIHostBridgeInfo (&bridges)[POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT])
{
    if (south_bridge == 0 || south_bridge == 0xffffffffu)
        return false;
    u32 parent_address_cells = 1;
    if (get_property(firmware, root, "#address-cells", &parent_address_cells, sizeof(parent_address_cells)) != sizeof(parent_address_cells))
        parent_address_cells = 1;
    if (parent_address_cells < 1 || parent_address_cells > 2)
        return false;

    u32 seen_bridges = 0;
    for (u32 bridge = first_child(firmware, root); bridge != 0 && bridge != 0xffffffffu; bridge = next_peer(firmware, bridge)) {
        char device_type[8] {};
        int type_length = get_property(firmware, bridge, "device_type", device_type, sizeof(device_type));
        if (type_length < 3 || device_type[0] != 'p' || device_type[1] != 'c' || device_type[2] != 'i'
            || (type_length > 3 && device_type[3] != '\0'))
            continue;

        u32 bridge_number = 0;
        if (get_property(firmware, bridge, "pci-bridge-number", &bridge_number, sizeof(bridge_number)) != sizeof(bridge_number))
            bridge_number = bridge == south_bridge ? 0 : 1;
        if (bridge_number >= POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT || (seen_bridges & (1u << bridge_number))
            || (bridge == south_bridge && bridge_number != 0))
            return false;

        u32 child_address_cells = 0;
        u32 size_cells = 0;
        if (get_property(firmware, bridge, "#address-cells", &child_address_cells, sizeof(child_address_cells)) != sizeof(child_address_cells)
            || get_property(firmware, bridge, "#size-cells", &size_cells, sizeof(size_cells)) != sizeof(size_cells)
            || child_address_cells != 3)
            return false;

        u32 bus_range[2] { 0, 255 };
        int bus_length = get_property(firmware, bridge, "bus-range", bus_range, sizeof(bus_range));
        if (bus_length != -1 && bus_length != sizeof(bus_range))
            return false;
        if (bus_range[0] > bus_range[1] || bus_range[1] > 255)
            return false;

        u32 cells[96];
        int length = get_property(firmware, bridge, "ranges", cells, sizeof(cells));
        if (length <= 0 || length % sizeof(u32) != 0)
            return false;
        auto& info = bridges[bridge_number];
        if (!powerpc_parse_open_firmware_pci_ranges(cells, length / sizeof(u32), child_address_cells,
                parent_address_cells, size_cells, info.ranges, POWERPC_PEGASOS2_PCI_MAX_RANGES, &info.range_count))
            return false;
        info.bridge_number = bridge_number;
        info.start_bus = bus_range[0];
        info.end_bus = bus_range[1];
        seen_bridges |= 1u << bridge_number;
    }
    return seen_bridges == (1u << POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT) - 1;
}

static u32 prepare_command_line(FirmwareEntry firmware, u32 chosen)
{
    int length = get_property(firmware, chosen, "bootargs", s_command_line, sizeof(s_command_line) - sizeof(" serial_debug"));
    u32 count = 0;
    if (length > 0 && length < static_cast<int>(sizeof(s_command_line))) {
        while (count < static_cast<u32>(length) && s_command_line[count] != '\0')
            ++count;
    }
    char const* debug = count == 0 ? "serial_debug" : " serial_debug";
    for (u32 index = 0; debug[index] != '\0'; ++index)
        s_command_line[count++] = debug[index];
    s_command_line[count] = '\0';
    return count;
}

static bool command_line_contains(char const* needle)
{
    for (u32 offset = 0; s_command_line[offset] != '\0'; ++offset) {
        u32 index = 0;
        while (needle[index] != '\0' && s_command_line[offset + index] == needle[index])
            ++index;
        if (needle[index] == '\0')
            return true;
    }
    return false;
}

static bool should_stop_before_kernel()
{
    return command_line_contains("serenity-loader-stop-before-kernel");
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

static void write_text(FirmwareEntry firmware, u32 stdout, char const* text, u32 length)
{
    FirmwareCall call { "write", 3, 1, { stdout, firmware_pointer(text), length, 0 } };
    firmware(&call);
}

static bool run_claim_probe(FirmwareEntry firmware, u32 stdout)
{
    u32 root = find_device(firmware, "/");
    u32 range_count = 0;
    if (root == 0 || root == 0xffffffffu
        || !discover_memory_ranges(firmware, root, "available", s_available_ranges, range_count)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: claim probe available memory unavailable\r\n");
        return false;
    }

    constexpr u32 probe_size = 4096;
    constexpr u32 probe_alignment = 4 * 1024 * 1024;
    u32 loader_end = (reinterpret_cast<FlatPtr>(__stack_end) + probe_size - 1) & ~(probe_size - 1);
    u32 candidate = 0;
    if (!Kernel::PowerPC::Loader::choose_claim_probe_address(
            s_available_ranges, range_count, loader_end, probe_size, probe_alignment, candidate)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: claim probe has no safe candidate\r\n");
        return false;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: claim probe candidate ");
    write_hex(firmware, stdout, candidate);
    if (claim_pages_at(firmware, candidate, probe_size) != candidate) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: claim probe failed\r\n");
        return false;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: claim probe succeeded\r\n");

    FirmwareCall release_call { "release", 2, 0, { candidate, probe_size } };
    if (firmware(&release_call) != 0) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: claim probe release failed\r\n");
        return false;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: claim probe released\r\n");
    return true;
}

static void write_word(FirmwareEntry firmware, u32 stdout, u32 value)
{
    char output[] = "0x00000000";
    char const* digits = "0123456789abcdef";
    for (int index = 9; index >= 2; --index) {
        output[index] = digits[value & 15u];
        value >>= 4;
    }
    write_text(firmware, stdout, output, 10);
}

static u32 parent_node(FirmwareEntry firmware, u32 node)
{
    FirmwareCall call { "parent", 1, 1, { node, 0 } };
    if (firmware(&call) != 0)
        return 0;
    return call.cells[1];
}

static bool is_node(u32 node)
{
    return node != 0 && node != 0xffffffffu;
}

static void dump_properties(FirmwareEntry firmware, u32 stdout, u32 node)
{
    static constexpr u32 max_shown_bytes = 128;
    char name[32] {};
    char next[32] {};
    for (u32 count = 0; count < 256; ++count) {
        FirmwareCall next_call { "nextprop", 3, 1, { node, firmware_pointer(name), firmware_pointer(next), 0 } };
        if (firmware(&next_call) != 0 || next_call.cells[3] != 1)
            return;
        for (u32 i = 0; i < sizeof(name); ++i)
            name[i] = next[i];
        name[sizeof(name) - 1] = 0;

        FirmwareCall length_call { "getproplen", 2, 1, { node, firmware_pointer(name), 0 } };
        int full_length = firmware(&length_call) == 0 ? static_cast<int>(length_call.cells[2]) : -1;
        write_console(firmware, stdout, "    ");
        write_console(firmware, stdout, name);
        if (full_length < 0) {
            write_console(firmware, stdout, " (unreadable)\r\n");
            continue;
        }
        u8 value[max_shown_bytes];
        u32 wanted = static_cast<u32>(full_length) < max_shown_bytes ? static_cast<u32>(full_length) : max_shown_bytes;
        int length = wanted == 0 ? 0 : get_property(firmware, node, name, value, wanted);
        write_console(firmware, stdout, " = ");
        Kernel::PowerPC::Loader::write_firmware_property_value(
            [&](char const* text, u32 size) { write_text(firmware, stdout, text, size); },
            value, length > 0 ? static_cast<u32>(length) : 0, wanted, static_cast<u32>(full_length));
        write_console(firmware, stdout, "\r\n");
    }
}

// Walks the whole device tree without recursion, printing every node's path and properties.
static void dump_firmware_tree(FirmwareEntry firmware, u32 stdout)
{
    write_console(firmware, stdout, "SerenityOS PowerPC loader: firmware tree begin\r\n");
    u32 root = find_device(firmware, "/");
    u32 node = root;
    for (u32 visited = 0; is_node(node) && visited < 1024; ++visited) {
        char path[256] {};
        FirmwareCall path_call { "package-to-path", 3, 1, { node, firmware_pointer(path), sizeof(path) - 1, 0 } };
        write_console(firmware, stdout, "  ");
        if (firmware(&path_call) == 0 && static_cast<int>(path_call.cells[3]) > 0)
            write_console(firmware, stdout, path);
        else
            write_word(firmware, stdout, node);
        write_console(firmware, stdout, "\r\n");
        dump_properties(firmware, stdout, node);

        u32 next = first_child(firmware, node);
        while (!is_node(next) && is_node(node) && node != root) {
            next = next_peer(firmware, node);
            if (!is_node(next))
                node = parent_node(firmware, node);
        }
        node = next;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: firmware tree end\r\n");
}

static void write_firmware_identity(FirmwareEntry firmware, u32 stdout, u32 chosen, u32 entry_msr, bool is_qemu)
{
    u32 pvr;
    asm volatile("mfpvr %0" : "=r"(pvr));
    write_console(firmware, stdout, "SerenityOS PowerPC loader: firmware ");
    write_console(firmware, stdout, is_qemu ? "QEMU Virtual Open Firmware" : "native Open Firmware");
    write_console(firmware, stdout, s_firmware_real_mode ? ", real-mode client calls" : ", client calls as entered");
    write_console(firmware, stdout, ", entry MSR ");
    write_word(firmware, stdout, entry_msr);
    write_console(firmware, stdout, ", PVR ");
    write_word(firmware, stdout, pvr);
    write_console(firmware, stdout, "\r\n");

    char text[256] {};
    FirmwareCall console_call { "instance-to-path", 3, 1, { stdout, firmware_pointer(text), sizeof(text) - 1, 0 } };
    if (firmware(&console_call) == 0 && static_cast<int>(console_call.cells[3]) > 0) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: console ");
        write_console(firmware, stdout, text);
        write_console(firmware, stdout, "\r\n");
    }
    char const* properties[] = { "bootpath", "bootargs" };
    for (char const* property : properties) {
        for (auto& character : text)
            character = 0;
        if (get_property(firmware, chosen, property, text, sizeof(text) - 1) > 0) {
            write_console(firmware, stdout, "SerenityOS PowerPC loader: ");
            write_console(firmware, stdout, property);
            write_console(firmware, stdout, " ");
            write_console(firmware, stdout, text);
            write_console(firmware, stdout, "\r\n");
        }
    }
}

// The binding allows "true"/"false" strings or a 4-byte boolean.
static bool firmware_requires_real_mode(FirmwareEntry firmware)
{
    u32 options = find_device(firmware, "/options");
    if (!is_node(options))
        return false;
    u8 value[8] {};
    int length = get_property(firmware, options, "real-mode?", value, sizeof(value));
    if (length >= 4 && value[0] == 't' && value[1] == 'r' && value[2] == 'u' && value[3] == 'e')
        return true;
    if (length == 4 && value[0] != 'f')
        return (value[0] | value[1] | value[2] | value[3]) != 0;
    return false;
}

extern "C" void loader_main(FirmwareEntry raw_firmware, u32 entry_msr);
extern "C" void loader_main(FirmwareEntry raw_firmware, u32 entry_msr)
{
    // These first calls run in the environment the firmware entered with, before the client-call
    // convention is known; the loader and its stack are identity-addressed there.
    s_firmware_entry = raw_firmware;
    FirmwareEntry firmware = call_firmware;
    u32 chosen = find_device(firmware, "/chosen");
    if (chosen == 0 || chosen == 0xffffffffu)
        return;
    u32 stdout = 0;
    FirmwareCall stdout_call { "getprop", 4, 1,
        { chosen, firmware_pointer("stdout"), firmware_pointer(&stdout), sizeof(stdout) } };
    if (firmware(&stdout_call) != 0 || stdout_call.cells[4] != sizeof(stdout))
        return;
    u32 qemu_boot_kernel[2];
    bool is_qemu = get_property(firmware, chosen, "qemu,boot-kernel", qemu_boot_kernel, sizeof(qemu_boot_kernel)) > 0;
    u32 command_line_length = prepare_command_line(firmware, chosen);
    s_firmware_real_mode = firmware_requires_real_mode(firmware) || command_line_contains("serenity-loader-real-mode-calls");

    write_firmware_identity(firmware, stdout, chosen, entry_msr, is_qemu);
    if (command_line_contains("ofdump"))
        dump_firmware_tree(firmware, stdout);
    if (!is_qemu || command_line_contains("serenity-loader-claim-probe")) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: exact RAM claim probe starting\r\n");
        run_claim_probe(firmware, stdout);
        if (is_qemu)
            return;
    }
    if (!is_qemu) {
        // Full kernel/HPT allocation, MMU takeover and quiesce remain unsupported on native
        // firmware. The tested Pegasos 2 SmartFirmware restarts after this exit call.
        write_console(firmware, stdout, "SerenityOS PowerPC loader: stopping, only QEMU firmware is supported past this point\r\n");
        FirmwareCall exit_call { "exit", 0, 0, {} };
        firmware(&exit_call);
        return;
    }

    u32 compressed_size = _binary_kernel_elf_gz_end - _binary_kernel_elf_gz_start;
    u32 kernel_elf_size = Kernel::PowerPC::Loader::gzip_uncompressed_size(_binary_kernel_elf_gz_start, compressed_size);
    u32 above_loader = (reinterpret_cast<FlatPtr>(__stack_end) + 0xfffffu) & ~0xfffffu;
    u32 kernel_elf_physical = kernel_elf_size == 0 ? 0 : claim_pages_at(firmware, above_loader, (kernel_elf_size + 4095u) & ~4095u);
    if (kernel_elf_physical == 0 || kernel_elf_physical == 0xffffffffu
        || !Kernel::PowerPC::Loader::gzip_inflate(_binary_kernel_elf_gz_start, compressed_size,
            reinterpret_cast<u8*>(kernel_elf_physical), kernel_elf_size)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel image decompression failed\r\n");
        return;
    }
    u8 const* kernel_elf = reinterpret_cast<u8 const*>(kernel_elf_physical);
    write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel image decompressed ");
    write_hex(firmware, stdout, kernel_elf_size);

    Kernel::PowerPC::Loader::KernelImagePlan plan;
    if (!Kernel::PowerPC::Loader::plan_kernel_image(kernel_elf, kernel_elf_size, plan)) {
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
        || !Kernel::PowerPC::Loader::load_kernel_image(kernel_elf,
            kernel_elf_size, plan,
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
        || !Kernel::PowerPC::Loader::relocate_kernel_image(kernel_elf,
            kernel_elf_size, plan,
            reinterpret_cast<u8*>(physical_base), span, static_cast<u32>(runtime_base), relocation_count)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel relocation failed\r\n");
        return;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel relocations applied\r\n");
    write_console(firmware, stdout, "SerenityOS PowerPC loader: relocation count ");
    write_hex(firmware, stdout, relocation_count);
    write_console(firmware, stdout, "SerenityOS PowerPC loader: runtime base ");
    write_hex(firmware, stdout, static_cast<u32>(runtime_base));

    // Keep kernel translations pinned while user translations use the HPT as
    // a cache. With 65536 hash groups, each of the four kernel segments can
    // occupy at most two entries in a complementary group pair.
    constexpr u32 table_size = 4 * 1024 * 1024;
    u32 table_physical = claim_pages(firmware, table_size, table_size);
    Kernel::PowerPC::Loader::KernelMappings mappings;
    if (table_physical == 0 || table_physical == 0xffffffffu
        || !Kernel::PowerPC::Loader::map_kernel_image(kernel_elf,
            kernel_elf_size, plan,
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

    u32 root = find_device(firmware, "/");
    u32 memory_range_count = 0;
    if (root == 0 || root == 0xffffffffu
        || !discover_memory_ranges(firmware, root, "reg", s_memory_ranges, memory_range_count)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: firmware memory unavailable\r\n");
        return;
    }
    u32 loader_start = reinterpret_cast<FlatPtr>(__loader_start);
    u32 loader_end = (reinterpret_cast<FlatPtr>(__stack_end) + 4095u) & ~4095u;
    s_reserved_ranges[0] = { 0, 0x2000 };
    s_reserved_ranges[1] = { loader_start, loader_end - loader_start };
    s_reserved_ranges[2] = { physical_base, span };
    s_reserved_ranges[3] = { mappings.table_physical, table_size };

    Kernel::BootInfo boot_info {};
    boot_info.boot_method = Kernel::BootMethod::FirmwareMemoryMap;
    boot_info.boot_method_specific.firmware_memory_map = BootMemoryMap {
        s_memory_ranges, memory_range_count, s_reserved_ranges, 4
    };
    boot_info.physical_to_virtual_offset = static_cast<u32>(runtime_base) - physical_base;
    boot_info.kernel_mapping_base = KERNEL_MAPPING_BASE;
    boot_info.kernel_load_base = static_cast<u32>(runtime_base);
    boot_info.arch_specific.open_firmware_entry = firmware_pointer(reinterpret_cast<void const*>(raw_firmware));
    boot_info.arch_specific.hpt_virtual_address = mappings.table_virtual;
    boot_info.arch_specific.hpt_physical_address = PhysicalAddress { mappings.table_physical };
    boot_info.arch_specific.hpt_group_count = mappings.group_count;
    boot_info.arch_specific.timebase_frequency_hz = discover_timebase_frequency(firmware);
    u32 south_bridge = 0;
    boot_info.arch_specific.mv64361_registers_physical_base = discover_controller_base(firmware, root, south_bridge);
    if (!discover_pci_host_bridges(firmware, root, south_bridge, boot_info.arch_specific.pci_host_bridges)) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: firmware PCI bridges unavailable\r\n");
        return;
    }
    boot_info.arch_specific.pci_host_bridge_count = POWERPC_PEGASOS2_PCI_HOST_BRIDGE_COUNT;
    boot_info.cmdline = AK::StringView { s_command_line, command_line_length };

    PowerPCHashPageTable validated_table;
    if (!Kernel::PowerPC::boot_hpt_from_boot_info(boot_info, validated_table)
        || validated_table.entries != reinterpret_cast<PowerPCHPTEntry*>(mappings.table_virtual)
        || validated_table.group_count != mappings.group_count
        || !Kernel::PowerPC::boot_timebase_frequency(boot_info).has_value()
        || !Kernel::PowerPC::boot_mv64361_registers_base(boot_info).has_value()
        || !Kernel::PowerPC::boot_pci_host_bridges(boot_info).has_value()) {
        write_console(firmware, stdout, "SerenityOS PowerPC loader: boot handoff invalid\r\n");
        return;
    }
    write_console(firmware, stdout, "SerenityOS PowerPC loader: boot handoff validated\r\n");
    write_console(firmware, stdout, "SerenityOS PowerPC loader: PCI bridge handoff prepared\r\n");
    if (should_stop_before_kernel())
        return;
    write_console(firmware, stdout, "SerenityOS PowerPC loader: entering kernel\r\n");
    powerpc_loader_enable_translation(mappings.table_physical | ((table_size >> 16) - 1));
    auto kernel_entry = reinterpret_cast<void (*)(Kernel::BootInfo const&)>(static_cast<u32>(runtime_base) + plan.entry_offset);
    kernel_entry(boot_info);
    powerpc_loader_disable_translation();
    write_console(firmware, stdout, "SerenityOS PowerPC loader: kernel returned\r\n");
}
