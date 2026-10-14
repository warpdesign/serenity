/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Endian.h>
#include <Kernel/Arch/Delay.h>
#include <Kernel/Devices/GPU/ATI/RadeonDisplayConnector.h>
#include <Kernel/Devices/GPU/Console/ContiguousFramebufferConsole.h>
#include <Kernel/Devices/GPU/Management.h>

namespace Kernel::Radeon {

static constexpr size_t bytes_per_pixel = sizeof(u32);

ErrorOr<NonnullRefPtr<RadeonDisplayConnector>> RadeonDisplayConnector::create(Family family, ApertureSwap aperture_swap, PhysicalAddress framebuffer_address, size_t framebuffer_size, NonnullOwnPtr<IOWindow> registers, PLLLimits pll_limits)
{
    auto connector = TRY(Device::try_create_device<RadeonDisplayConnector>(family, aperture_swap, framebuffer_address, framebuffer_size, move(registers), pll_limits));
    TRY(connector->create_attached_framebuffer_console());
    TRY(connector->initialize_edid_for_generic_monitor({}));
    return connector;
}

RadeonDisplayConnector::RadeonDisplayConnector(Family family, ApertureSwap aperture_swap, PhysicalAddress framebuffer_address, size_t framebuffer_size, NonnullOwnPtr<IOWindow> registers, PLLLimits pll_limits)
    : DisplayConnector(framebuffer_address, framebuffer_size, Memory::MemoryType::NonCacheable)
    , m_family(family)
    , m_aperture_swap(aperture_swap)
    , m_registers(move(registers))
    , m_pll_limits(pll_limits)
{
}

ErrorOr<void> RadeonDisplayConnector::create_attached_framebuffer_console()
{
    m_framebuffer_console = Graphics::ContiguousFramebufferConsole::initialize(m_framebuffer_address.value(), 1024, 768, 1024 * bytes_per_pixel);
    GraphicsManagement::the().set_console(*m_framebuffer_console);
    return {};
}

// Display engine registers are little-endian.
u32 RadeonDisplayConnector::read32(u32 offset)
{
    return AK::convert_between_host_and_little_endian(m_registers->read32(offset));
}

void RadeonDisplayConnector::write32(u32 offset, u32 value)
{
    m_registers->write32(offset, AK::convert_between_host_and_little_endian(value));
}

void RadeonDisplayConnector::write32_masked(u32 offset, u32 value, u32 preserved_bits)
{
    write32(offset, (read32(offset) & preserved_bits) | value);
}

// PLL access errata from XFree86, as carried by Haiku's radeon driver.
void RadeonDisplayConnector::after_pll_index_access()
{
    if (m_family != Family::RV200)
        return;
    (void)read32(Register::ClockControlData);
    (void)read32(Register::CRTCGeneralControl);
}

void RadeonDisplayConnector::after_pll_data_access()
{
    // RV100 can hang on a PLL access that follows a posted write too closely.
    if (m_family == Family::RV100)
        microseconds_delay(5000);
}

u32 RadeonDisplayConnector::read_pll(u8 index)
{
    m_registers->write8(Register::ClockControlIndex, index & ClockControlIndex::IndexMask);
    after_pll_index_access();
    u32 value = read32(Register::ClockControlData);
    after_pll_data_access();
    return value;
}

void RadeonDisplayConnector::write_pll(u8 index, u32 value)
{
    m_registers->write8(Register::ClockControlIndex, (index & ClockControlIndex::IndexMask) | ClockControlIndex::PLLWriteEnable);
    after_pll_index_access();
    write32(Register::ClockControlData, value);
    after_pll_data_access();
}

void RadeonDisplayConnector::write_pll_masked(u8 index, u32 value, u32 preserved_bits)
{
    write_pll(index, (read_pll(index) & preserved_bits) | value);
}

void RadeonDisplayConnector::wait_for_pll_atomic_update()
{
    // Some chips never clear the bit, so bound the wait.
    for (size_t attempt = 0; attempt < 10000; ++attempt) {
        if (!(read_pll(PLLRegister::PixelPLLReferenceDivider) & PixelPLLReferenceDivider::AtomicUpdate))
            return;
    }
}

ErrorOr<PLLDividers> RadeonDisplayConnector::calculate_pll_dividers(u32 pixel_clock_in_khz) const
{
    // pixel clock = reference / reference divider * feedback divider / post divider.
    // Frequencies other than the output are in 10 kHz units, like the BIOS PLL table.
    static constexpr struct {
        u32 divider;
        u32 code;
    } post_dividers[] = { { 1, 0 }, { 2, 1 }, { 4, 2 }, { 8, 3 }, { 3, 4 }, { 6, 6 }, { 12, 7 } };
    static constexpr u32 minimum_reference_divider = 2;
    static constexpr u32 maximum_reference_divider = 0x3ff;
    static constexpr u32 minimum_pll_input = 40;
    static constexpr u32 maximum_pll_input = 100;
    static constexpr u32 minimum_feedback_divider = 4;
    static constexpr u32 maximum_feedback_divider = 0x7ff;

    u64 const frequency = static_cast<u64>(pixel_clock_in_khz) * 1000;
    u64 const reference_in_hz = static_cast<u64>(m_pll_limits.reference_frequency) * 10000;
    Optional<PLLDividers> best;
    u64 best_error = NumericLimits<u64>::max();

    for (auto const& post : post_dividers) {
        u64 vco = (frequency / 10000) * post.divider;
        if (vco < m_pll_limits.minimum_vco_frequency || vco > m_pll_limits.maximum_vco_frequency)
            continue;
        for (u32 reference_divider = minimum_reference_divider; reference_divider <= maximum_reference_divider; ++reference_divider) {
            u32 pll_input = m_pll_limits.reference_frequency / reference_divider;
            if (pll_input < minimum_pll_input || pll_input > maximum_pll_input)
                continue;
            u64 numerator = frequency * reference_divider * post.divider;
            u64 feedback_divider = (numerator + reference_in_hz / 2) / reference_in_hz;
            if (feedback_divider < minimum_feedback_divider || feedback_divider > maximum_feedback_divider)
                continue;
            u64 denominator = static_cast<u64>(reference_divider) * post.divider;
            u64 achieved = (reference_in_hz * feedback_divider + denominator / 2) / denominator;
            u64 error = achieved > frequency ? achieved - frequency : frequency - achieved;
            if (error < best_error) {
                best_error = error;
                best = PLLDividers {
                    .reference = reference_divider,
                    .feedback = static_cast<u32>(feedback_divider),
                    .post = post.divider,
                    .post_code = post.code,
                    .frequency_in_hz = static_cast<u32>(achieved),
                };
            }
        }
    }
    if (!best.has_value())
        return Error::from_errno(ERANGE);
    return best.release_value();
}

void RadeonDisplayConnector::program_pll(PLLDividers const& dividers, size_t horizontal_total)
{
    // Clock the CRTC from the CPU clock while the pixel PLL is reprogrammed.
    write_pll_masked(PLLRegister::VideoClockControl, VideoClockControl::SourceCPUClock, ~VideoClockControl::SourceMask);

    constexpr u32 update_control_bits = PixelPLLControl::Reset | PixelPLLControl::AtomicUpdateEnable | PixelPLLControl::VGAAtomicUpdateEnable;
    write_pll_masked(PLLRegister::PixelPLLControl, update_control_bits, ~update_control_bits);

    write32_masked(Register::ClockControlIndex, ClockControlIndex::PLLDividerSelect3, ~ClockControlIndex::PLLDividerSelectMask);
    after_pll_index_access();

    write_pll_masked(PLLRegister::PixelPLLReferenceDivider, dividers.reference, ~PixelPLLReferenceDivider::Mask);
    write_pll_masked(PLLRegister::PixelPLLDivider3, dividers.feedback, ~PixelPLLDivider3::FeedbackMask);
    write_pll_masked(PLLRegister::PixelPLLDivider3, dividers.post_code << PixelPLLDivider3::PostShift, ~PixelPLLDivider3::PostMask);

    wait_for_pll_atomic_update();
    write_pll_masked(PLLRegister::PixelPLLReferenceDivider, PixelPLLReferenceDivider::AtomicUpdate, ~PixelPLLReferenceDivider::AtomicUpdate);
    wait_for_pll_atomic_update();

    // The PLL holds the sub-character part of the horizontal total.
    write_pll(PLLRegister::HorizontalTotalControl, horizontal_total & 7);

    write_pll_masked(PLLRegister::PixelPLLControl, 0, ~(update_control_bits | PixelPLLControl::Sleep));
    // The PLL has no lock indicator.
    microseconds_delay(5000);

    write_pll_masked(PLLRegister::VideoClockControl, VideoClockControl::SourcePixelPLL, ~VideoClockControl::SourceMask);
}

void RadeonDisplayConnector::program_digital_output(u32 horizontal_sync, u32 vertical_sync)
{
    // A DVI monitor on the internal TMDS transmitter is driven through the flat panel unit. Only reconfigure it
    // if the video BIOS enabled it; external transmitters and LVDS panels are not handled.
    auto flat_panel_control = read32(Register::FlatPanelGeneralControl);
    constexpr u32 internal_dvi_enabled = FlatPanelGeneralControl::PanelOn | FlatPanelGeneralControl::TMDSEnable;
    if ((flat_panel_control & internal_dvi_enabled) != internal_dvi_enabled)
        return;

    // The panel sync positions and polarities always come from these registers.
    write32(Register::FlatPanelHorizontalSyncStartWidth, horizontal_sync);
    write32(Register::FlatPanelVerticalSyncStartWidth, vertical_sync);

    // Take the timing straight from CRTC 1 instead of the shadow registers the BIOS used for its text mode.
    flat_panel_control &= ~(FlatPanelGeneralControl::RMXSyncControlEnable | FlatPanelGeneralControl::DFPSyncSelect
        | FlatPanelGeneralControl::CRTSyncSelect | FlatPanelGeneralControl::CRTCLock8Dot | FlatPanelGeneralControl::UseShadowEnable
        | FlatPanelGeneralControl::CRTCUseShadowVerticalEnd | FlatPanelGeneralControl::CRTSyncAlternate | FlatPanelGeneralControl::SourceCRTC2);
    flat_panel_control |= FlatPanelGeneralControl::CRTCDontShadowVerticalParameters | FlatPanelGeneralControl::CRTCDontShadowHorizontalEnd
        | FlatPanelGeneralControl::PanelFormat24Bit | internal_dvi_enabled;
    write32(Register::FlatPanelGeneralControl, flat_panel_control);

    // An external DVI monitor scales on its own, so stop the RMX unit stretching the BIOS's text mode.
    // The TMDS PLL is left as the BIOS programmed it: RV280 reads one of its bits back inverted, and the
    // BIOS value covers pixel clocks below 150 MHz.
    write32_masked(Register::FlatPanelHorizontalStretch, 0, ~(FlatPanelStretch::Enable | FlatPanelStretch::Blend));
    write32_masked(Register::FlatPanelVerticalStretch, 0, ~(FlatPanelStretch::Enable | FlatPanelStretch::Blend));
    dmesgln("Radeon: Driving the DVI output from CRTC 1");
}

void RadeonDisplayConnector::log_output_routing(StringView when)
{
    // Output routing is chosen by the video BIOS and cannot be checked in QEMU, so record it for real hardware.
    dmesgln("Radeon: Output registers {}: CRTC_GEN_CNTL={:#08x} CRTC_EXT_CNTL={:#08x} DAC_CNTL={:#08x} FP_GEN_CNTL={:#08x} FP2_GEN_CNTL={:#08x} "
            "FP_HORZ_STRETCH={:#08x} FP_VERT_STRETCH={:#08x} TMDS_TRANSMITTER_CNTL={:#08x} TMDS_PLL_CNTL={:#08x} LVDS_GEN_CNTL={:#08x}",
        when, read32(Register::CRTCGeneralControl), read32(Register::CRTCExtendedControl), read32(Register::DACControl),
        read32(Register::FlatPanelGeneralControl), read32(Register::FlatPanel2GeneralControl),
        read32(Register::FlatPanelHorizontalStretch), read32(Register::FlatPanelVerticalStretch),
        read32(Register::TMDSTransmitterControl), read32(Register::TMDSPLLControl), read32(Register::LVDSGeneralControl));
}

void RadeonDisplayConnector::load_identity_palette()
{
    // The palette also acts as the gamma table in 32-bit modes; the video BIOS left VGA text colours in it.
    write32_masked(Register::DACControl, DACControl::EightBitEnable, ~0u);
    m_registers->write8(Register::PaletteIndex, 0);
    for (u32 i = 0; i < 256; ++i)
        write32(Register::PaletteData, (i << 16) | (i << 8) | i);
}

ErrorOr<void> RadeonDisplayConnector::set_mode_setting(ModeSetting const& mode_setting)
{
    SpinlockLocker locker(m_modeset_lock);
    VERIFY(m_framebuffer_console);

    auto width = mode_setting.horizontal_active;
    auto height = mode_setting.vertical_active;
    // The CRTC pitch counts groups of 8 pixels, and Radeon scanout wants 64-byte aligned lines.
    if (width % 16 != 0 || mode_setting.horizontal_stride != width * bytes_per_pixel)
        return Error::from_errno(ENOTSUP);
    if (static_cast<u64>(mode_setting.horizontal_stride) * height > m_framebuffer_resource_size)
        return Error::from_errno(ENOSPC);

    auto dividers = TRY(calculate_pll_dividers(mode_setting.pixel_clock_in_khz));
    dmesgln("Radeon: {}x{} with pixel clock {} kHz: reference divider {}, feedback divider {}, post divider {} ({} Hz)",
        width, height, mode_setting.pixel_clock_in_khz, dividers.reference, dividers.feedback, dividers.post, dividers.frequency_in_hz);

    // Overlay, subpicture and I2C state can interfere with a mode switch.
    write32(Register::OverlayClear, 0);
    write32(Register::OverlayWindowLeftRight, 0);
    write32(Register::OverlayWindowTopBottom, 0);
    write32(Register::Overlay0ScaleControl, 0);
    write32(Register::SubpictureControl, 0);
    write32(Register::I2CControl1, 0);
    write32_masked(Register::CRTCGeneralControl, CRTCGeneralControl::ExtendedDisplayEnable, ~CRTCGeneralControl::ExtendedDisplayEnable);
    write32(Register::CRTCMoreControl, 0);

    // Leave the BIOS's choice of outputs alone, but make sure the CRTC drives them from the linear framebuffer.
    auto extended_control = read32(Register::CRTCExtendedControl);
    extended_control |= CRTCExtendedControl::VGAATILinear | CRTCExtendedControl::ExtendedCRTCountEnable;
    extended_control &= ~(CRTCExtendedControl::HSyncDisable | CRTCExtendedControl::VSyncDisable | CRTCExtendedControl::DisplayDisable);
    write32(Register::CRTCExtendedControl, extended_control);

    log_output_routing("before mode set"sv);

    // Sync delay for 32-bit pixels; analog and digital outputs use the same value at this depth.
    constexpr size_t horizontal_sync_fudge = 5;
    auto horizontal_total = mode_setting.horizontal_total();
    auto horizontal_sync_start = mode_setting.horizontal_sync_start() - 8 + horizontal_sync_fudge;
    auto horizontal_sync_width = max<size_t>((mode_setting.horizontal_sync_end() - mode_setting.horizontal_sync_start()) / 8, 1);
    auto vertical_sync_width = mode_setting.vertical_sync_end() - mode_setting.vertical_sync_start();
    u32 pitch = (mode_setting.horizontal_stride / bytes_per_pixel) / 8;

    write32_masked(Register::CRTCGeneralControl,
        CRTCGeneralControl::Enable | (CRTCGeneralControl::PixelFormatARGB8888 << CRTCGeneralControl::PixelWidthShift),
        CRTCGeneralControl::ExtendedDisplayEnable);
    write32(Register::CRTCHorizontalTotalDisplay,
        ((horizontal_total / 8 - 1) & CRTCTiming::HorizontalTotalMask)
            | (((width / 8 - 1) << CRTCTiming::HorizontalDisplayShift) & CRTCTiming::HorizontalDisplayMask));
    // Both syncs are negative for the VESA modes used here.
    u32 const horizontal_sync = (horizontal_sync_start & CRTCTiming::HorizontalSyncStartMask)
        | (horizontal_sync_width << CRTCTiming::HorizontalSyncWidthShift)
        | CRTCTiming::HorizontalSyncNegative;
    u32 const vertical_sync = ((mode_setting.vertical_sync_start() - 1) & CRTCTiming::VerticalSyncStartMask)
        | (vertical_sync_width << CRTCTiming::VerticalSyncWidthShift)
        | CRTCTiming::VerticalSyncNegative;
    write32(Register::CRTCHorizontalSyncStartWidth, horizontal_sync);
    write32(Register::CRTCVerticalTotalDisplay,
        ((mode_setting.vertical_total() - 1) & CRTCTiming::VerticalTotalMask)
            | (((height - 1) << CRTCTiming::VerticalDisplayShift) & CRTCTiming::VerticalDisplayMask));
    write32(Register::CRTCVerticalSyncStartWidth, vertical_sync);
    write32(Register::CRTCOffsetControl, 0);
    write32(Register::CRTCPitch, pitch | (pitch << 16));
    write32(Register::CRTCOffset, 0);

    u32 surface_control = SurfaceControl::TranslationDisable;
    if constexpr (!AK::HostIsLittleEndian) {
        if (m_aperture_swap == ApertureSwap::ConfigControl)
            write32_masked(Register::ConfigControl, ConfigControl::Aperture0Swap32BPP, ~ConfigControl::Aperture0EndianMask);
        else
            surface_control |= SurfaceControl::Aperture0Swap32BPP;
    }
    write32(Register::SurfaceControl, surface_control);

    program_digital_output(horizontal_sync, vertical_sync);
    program_pll(dividers, horizontal_total);
    load_identity_palette();
    log_output_routing("after mode set"sv);

    m_framebuffer_console->set_resolution(width, height, mode_setting.horizontal_stride);
    m_current_mode_setting = mode_setting;
    return {};
}

ErrorOr<void> RadeonDisplayConnector::set_safe_mode_setting()
{
    // VESA 1024x768 at 60 Hz.
    ModeSetting safe_mode_setting {
        .horizontal_stride = 1024 * bytes_per_pixel,
        .pixel_clock_in_khz = 65000,
        .horizontal_active = 1024,
        .horizontal_front_porch_pixels = 24,
        .horizontal_sync_time_pixels = 136,
        .horizontal_blank_pixels = 320,
        .vertical_active = 768,
        .vertical_front_porch_lines = 3,
        .vertical_sync_time_lines = 6,
        .vertical_blank_lines = 38,
        .horizontal_offset = 0,
        .vertical_offset = 0,
    };
    return set_mode_setting(safe_mode_setting);
}

ErrorOr<void> RadeonDisplayConnector::set_y_offset(size_t)
{
    return Error::from_errno(ENOTIMPL);
}

ErrorOr<void> RadeonDisplayConnector::unblank()
{
    return Error::from_errno(ENOTIMPL);
}

ErrorOr<void> RadeonDisplayConnector::flush_first_surface()
{
    return Error::from_errno(ENOTSUP);
}

void RadeonDisplayConnector::enable_console()
{
    VERIFY(m_control_lock.is_locked());
    VERIFY(m_framebuffer_console);
    m_framebuffer_console->enable();
}

void RadeonDisplayConnector::disable_console()
{
    VERIFY(m_control_lock.is_locked());
    VERIFY(m_framebuffer_console);
    m_framebuffer_console->disable();
}

}
