/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Endian.h>
#include <Kernel/Bus/PCI/API.h>
#include <Kernel/Bus/PCI/BarMapping.h>
#include <Kernel/Bus/PCI/IDs.h>
#include <Kernel/Devices/GPU/ATI/RadeonDisplayConnector.h>
#include <Kernel/Devices/GPU/ATI/RadeonFramebufferBudget.h>
#include <Kernel/Devices/GPU/ATI/RadeonGraphicsAdapter.h>
#include <Kernel/Library/IOWindow.h>
#include <Kernel/Memory/MemoryManager.h>
#include <Kernel/Memory/TypedMapping.h>

namespace Kernel {

using Radeon::Family;

static constexpr struct {
    u16 device_id;
    Family family;
} supported_models[] = {
    { 0x5144, Family::R100 },
    { 0x5145, Family::R100 },
    { 0x5146, Family::R100 },
    { 0x5147, Family::R100 },
    { 0x5159, Family::RV100 }, // Radeon 7000/VE, also QEMU's ati-vga model=rv100
    { 0x515a, Family::RV100 },
    { 0x5157, Family::RV200 },
    { 0x5158, Family::RV200 },
    { 0x5148, Family::R200 },
    { 0x5149, Family::R200 },
    { 0x514a, Family::R200 },
    { 0x514b, Family::R200 },
    { 0x514c, Family::R200 },
    { 0x514d, Family::R200 },
    { 0x5960, Family::RV280 }, // Radeon 9200 Pro
    { 0x5961, Family::RV280 }, // Radeon 9200
    { 0x5962, Family::RV280 },
    { 0x5964, Family::RV280 }, // Radeon 9200 SE
    { 0x5965, Family::RV280 },
};

static Optional<Family> family_for(PCI::HardwareID id)
{
    if (id.vendor_id != PCI::VendorID::ATI)
        return {};
    for (auto const& model : supported_models) {
        if (model.device_id == id.device_id)
            return model.family;
    }
    return {};
}

// Limits used by radeonfb when the BIOS has no PLL table: 27 MHz reference, 125-350 MHz VCO.
static constexpr Radeon::PLLLimits default_pll_limits { 2700, 12500, 35000 };

static ErrorOr<Radeon::PLLLimits> read_pll_limits_from_rom(PCI::DeviceIdentifier const& identifier, u32 rom_bar)
{
    auto rom_address = TRY(PCI::translate_bus_address_to_host_address(identifier, PCI::BARSpaceType::Memory32BitSpace, rom_bar & 0xfffff800));
    static constexpr size_t rom_window_size = 64 * KiB;
    auto rom = TRY(Memory::map_typed<u8 const volatile[]>(rom_address, rom_window_size, Memory::Region::Access::Read));
    auto const* bytes = rom.ptr();
    auto le16 = [&](size_t offset) -> u16 { return bytes[offset] | (bytes[offset + 1] << 8); };
    auto le32 = [&](size_t offset) -> u32 { return le16(offset) | (static_cast<u32>(le16(offset + 2)) << 16); };

    if (bytes[0] != 0x55 || bytes[1] != 0xaa)
        return Error::from_errno(ENOENT);
    u16 bios_header = le16(0x48);
    if (bios_header + 0x32u > rom_window_size)
        return Error::from_errno(EINVAL);
    // AtomBIOS ROMs (R300 and later) use different tables.
    char const* tag = reinterpret_cast<char const*>(const_cast<u8 const*>(bytes) + bios_header + 4);
    if ((tag[0] == 'A' && tag[1] == 'T' && tag[2] == 'O' && tag[3] == 'M') || (tag[0] == 'M' && tag[1] == 'O' && tag[2] == 'T' && tag[3] == 'A'))
        return Error::from_errno(ENOTSUP);
    u16 pll_table = le16(bios_header + 0x30);
    if (pll_table == 0 || pll_table + 26u > rom_window_size)
        return Error::from_errno(EINVAL);

    Radeon::PLLLimits limits {
        .reference_frequency = le16(pll_table + 14),
        .minimum_vco_frequency = le32(pll_table + 18),
        .maximum_vco_frequency = le32(pll_table + 22),
    };
    if (limits.reference_frequency < 1000 || limits.reference_frequency > 5000
        || limits.minimum_vco_frequency == 0 || limits.minimum_vco_frequency >= limits.maximum_vco_frequency
        || limits.maximum_vco_frequency > 100000)
        return Error::from_errno(EINVAL);
    return limits;
}

static Radeon::PLLLimits read_pll_limits(PCI::DeviceIdentifier const& identifier)
{
    u32 rom_bar;
    {
        SpinlockLocker locker(identifier.operation_lock());
        rom_bar = PCI::read32_locked(identifier, PCI::RegisterOffset::EXPANSION_ROM_POINTER);
    }
    if ((rom_bar & 0xfffff800) == 0) {
        dmesgln("Radeon: No expansion ROM assigned, using default PLL limits");
        return default_pll_limits;
    }

    {
        SpinlockLocker locker(identifier.operation_lock());
        PCI::write32_locked(identifier, PCI::RegisterOffset::EXPANSION_ROM_POINTER, rom_bar | 1);
    }
    auto limits_or_error = read_pll_limits_from_rom(identifier, rom_bar);
    {
        SpinlockLocker locker(identifier.operation_lock());
        PCI::write32_locked(identifier, PCI::RegisterOffset::EXPANSION_ROM_POINTER, rom_bar);
    }

    if (limits_or_error.is_error()) {
        dmesgln("Radeon: No usable PLL table in the video BIOS ({}), using default PLL limits", limits_or_error.error());
        return default_pll_limits;
    }
    auto limits = limits_or_error.release_value();
    dmesgln("Radeon: Video BIOS PLL reference {} kHz, VCO {}-{} kHz", limits.reference_frequency * 10, limits.minimum_vco_frequency * 10, limits.maximum_vco_frequency * 10);
    return limits;
}

ErrorOr<bool> RadeonGraphicsAdapter::probe(PCI::DeviceIdentifier const& identifier)
{
    return family_for(identifier.hardware_id()).has_value();
}

ErrorOr<NonnullLockRefPtr<GPUDevice>> RadeonGraphicsAdapter::create(PCI::DeviceIdentifier const& identifier)
{
    auto adapter = TRY(adopt_nonnull_lock_ref_or_enomem(new (nothrow) RadeonGraphicsAdapter(identifier)));
    TRY(adapter->initialize_adapter(identifier, family_for(identifier.hardware_id()).value()));
    return adapter;
}

RadeonGraphicsAdapter::RadeonGraphicsAdapter(PCI::DeviceIdentifier const& identifier)
    : PCI::Device(const_cast<PCI::DeviceIdentifier&>(identifier))
{
}

ErrorOr<void> RadeonGraphicsAdapter::initialize_adapter(PCI::DeviceIdentifier const& identifier, Family family)
{
    PCI::enable_memory_space(identifier);

    // BAR0 is the framebuffer aperture and BAR2 the register aperture.
    auto registers = TRY(IOWindow::create_for_pci_device_bar(identifier, PCI::HeaderType0BaseRegister::BAR2));
    auto framebuffer_address = TRY(PCI::get_bar_address(identifier, PCI::HeaderType0BaseRegister::BAR0));
    size_t framebuffer_size = PCI::get_BAR_space_size(identifier, PCI::HeaderType0BaseRegister::BAR0);
    u32 video_memory_size = AK::convert_between_host_and_little_endian(registers->read32(Radeon::Register::ConfigMemorySize));
    if (video_memory_size != 0)
        framebuffer_size = min<size_t>(framebuffer_size, video_memory_size);
    dmesgln("Radeon: {} {} framebuffer {} ({} KiB)", identifier.address(), identifier.hardware_id(), framebuffer_address, framebuffer_size / KiB);
    auto mapped_framebuffer_size = static_cast<size_t>(Radeon::framebuffer_mapping_bytes(framebuffer_size));
    auto uncommitted_before = MM.get_system_memory_info().physical_pages_uncommitted;

    auto pll_limits = read_pll_limits(identifier);
    // QEMU's emulated card (subsystem vendor 0x1af4) only implements the Rage 128-style aperture swap.
    auto aperture_swap = identifier.subsystem_vendor_id().value() == PCI::VendorID::VirtIO ? Radeon::ApertureSwap::ConfigControl : Radeon::ApertureSwap::SurfaceControl;
    auto connector = TRY(Radeon::RadeonDisplayConnector::create(family, aperture_swap, framebuffer_address, mapped_framebuffer_size, move(registers), pll_limits));
    dmesgln("Radeon: framebuffer mapping {} KiB from {} KiB aperture; uncommitted pages {} -> {}",
        mapped_framebuffer_size / KiB, framebuffer_size / KiB, uncommitted_before,
        MM.get_system_memory_info().physical_pages_uncommitted);
    m_display_connector = connector;
    TRY(m_display_connector->set_safe_mode_setting());
    return {};
}

}
