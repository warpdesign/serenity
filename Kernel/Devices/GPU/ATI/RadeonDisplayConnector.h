/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <Kernel/Devices/GPU/ATI/RadeonDefinitions.h>
#include <Kernel/Devices/GPU/Console/GenericFramebufferConsole.h>
#include <Kernel/Devices/GPU/DisplayConnector.h>
#include <Kernel/Library/IOWindow.h>

namespace Kernel::Radeon {

// Drives CRTC 1 of an R100/R200-family Radeon in a 32-bit mode on the outputs the video BIOS enabled (VGA, or DVI on the internal TMDS transmitter).
class RadeonDisplayConnector final : public DisplayConnector {
    friend class Kernel::Device;

public:
    static ErrorOr<NonnullRefPtr<RadeonDisplayConnector>> create(Family, ApertureSwap, PhysicalAddress framebuffer_address, size_t framebuffer_size, NonnullOwnPtr<IOWindow> registers, PLLLimits);

private:
    RadeonDisplayConnector(Family, ApertureSwap, PhysicalAddress framebuffer_address, size_t framebuffer_size, NonnullOwnPtr<IOWindow> registers, PLLLimits);

    ErrorOr<void> create_attached_framebuffer_console();

    virtual bool mutable_mode_setting_capable() const override { return false; }
    virtual bool double_framebuffering_capable() const override { return false; }
    virtual ErrorOr<void> set_mode_setting(ModeSetting const&) override;
    virtual ErrorOr<void> set_y_offset(size_t y) override;
    virtual ErrorOr<void> set_safe_mode_setting() override;
    virtual ErrorOr<void> unblank() override;
    virtual bool partial_flush_support() const override { return false; }
    virtual bool flush_support() const override { return false; }
    virtual bool refresh_rate_support() const override { return false; }
    virtual ErrorOr<void> flush_first_surface() override;
    virtual void enable_console() override;
    virtual void disable_console() override;

    u32 read32(u32 offset);
    void write32(u32 offset, u32 value);
    void write32_masked(u32 offset, u32 value, u32 preserved_bits);
    u32 read_pll(u8 index);
    void write_pll(u8 index, u32 value);
    void write_pll_masked(u8 index, u32 value, u32 preserved_bits);
    void after_pll_index_access();
    void after_pll_data_access();
    void wait_for_pll_atomic_update();

    ErrorOr<PLLDividers> calculate_pll_dividers(u32 pixel_clock_in_khz) const;
    void program_pll(PLLDividers const&, size_t horizontal_total);
    void program_digital_output(u32 horizontal_sync, u32 vertical_sync);
    void log_output_routing(StringView when);
    void load_identity_palette();

    Family const m_family;
    ApertureSwap const m_aperture_swap;
    NonnullOwnPtr<IOWindow> m_registers;
    PLLLimits const m_pll_limits;
    LockRefPtr<Graphics::GenericFramebufferConsole> m_framebuffer_console;
};

}
