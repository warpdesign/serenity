/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

// Register layout of the R100/R200-family display engine, as used by Haiku's radeon driver and Linux's radeonfb.
namespace Kernel::Radeon {

enum class Family {
    R100,
    RV100,
    RV200,
    R200,
    RV280,
};

namespace Register {
static constexpr u32 ClockControlIndex = 0x0008;
static constexpr u32 ClockControlData = 0x000c;
static constexpr u32 CRTCGeneralControl = 0x0050;
static constexpr u32 CRTCExtendedControl = 0x0054;
static constexpr u32 DACControl = 0x0058;
static constexpr u32 I2CControl1 = 0x0094;
static constexpr u32 PaletteIndex = 0x00b0;
static constexpr u32 PaletteData = 0x00b4;
static constexpr u32 ConfigControl = 0x00e0;
static constexpr u32 ConfigMemorySize = 0x00f8;
static constexpr u32 CRTCHorizontalTotalDisplay = 0x0200;
static constexpr u32 CRTCHorizontalSyncStartWidth = 0x0204;
static constexpr u32 CRTCVerticalTotalDisplay = 0x0208;
static constexpr u32 CRTCVerticalSyncStartWidth = 0x020c;
static constexpr u32 CRTCOffset = 0x0224;
static constexpr u32 CRTCOffsetControl = 0x0228;
static constexpr u32 CRTCPitch = 0x022c;
static constexpr u32 OverlayClear = 0x0230;
static constexpr u32 OverlayWindowLeftRight = 0x0234;
static constexpr u32 OverlayWindowTopBottom = 0x0238;
static constexpr u32 CRTCMoreControl = 0x027c;
static constexpr u32 Overlay0ScaleControl = 0x0420;
static constexpr u32 SubpictureControl = 0x0540;
static constexpr u32 SurfaceControl = 0x0b00;
}

namespace CRTCGeneralControl {
static constexpr u32 PixelWidthShift = 8;
static constexpr u32 PixelWidthMask = 0xf << PixelWidthShift;
static constexpr u32 ExtendedDisplayEnable = 1 << 24;
static constexpr u32 Enable = 1 << 25;
static constexpr u32 DisplayRequestDisable = 1 << 26;
// The pixel width field's "datatype" for 32-bit xRGB8888 pixels.
static constexpr u32 PixelFormatARGB8888 = 6;
}

namespace CRTCExtendedControl {
static constexpr u32 VGAATILinear = 1 << 3;
static constexpr u32 ExtendedCRTCountEnable = 1 << 6;
static constexpr u32 HSyncDisable = 1 << 8;
static constexpr u32 VSyncDisable = 1 << 9;
static constexpr u32 DisplayDisable = 1 << 10;
}

namespace CRTCTiming {
static constexpr u32 HorizontalTotalMask = 0x3ff;
static constexpr u32 HorizontalDisplayShift = 16;
static constexpr u32 HorizontalDisplayMask = 0x1ff << HorizontalDisplayShift;
static constexpr u32 HorizontalSyncStartMask = 0x1fff;
static constexpr u32 HorizontalSyncWidthShift = 16;
static constexpr u32 HorizontalSyncNegative = 1 << 23;
static constexpr u32 VerticalTotalMask = 0x7ff;
static constexpr u32 VerticalDisplayShift = 16;
static constexpr u32 VerticalDisplayMask = 0x7ff << VerticalDisplayShift;
static constexpr u32 VerticalSyncStartMask = 0x7ff;
static constexpr u32 VerticalSyncWidthShift = 16;
static constexpr u32 VerticalSyncNegative = 1 << 23;
}

namespace DACControl {
static constexpr u32 EightBitEnable = 1 << 8;
static constexpr u32 VGAAddressEnable = 1 << 13;
}

namespace SurfaceControl {
static constexpr u32 TranslationDisable = 1 << 8;
// Byte-swap 32-bit words written through aperture 0, so big-endian CPUs see native-order pixels.
static constexpr u32 Aperture0Swap32BPP = 1 << 21;
}

// Rage 128-style aperture endianness control, which QEMU's ati-vga uses instead of SURFACE_CNTL.
namespace ConfigControl {
static constexpr u32 Aperture0EndianMask = 3;
static constexpr u32 Aperture0Swap32BPP = 2;
}

// How big-endian CPU writes to the framebuffer aperture get byte-swapped into the card's little-endian pixels.
enum class ApertureSwap {
    SurfaceControl,
    ConfigControl,
};

namespace ClockControlIndex {
static constexpr u32 IndexMask = 0x3f;
static constexpr u32 PLLWriteEnable = 1 << 7;
static constexpr u32 PLLDividerSelectMask = 3 << 8;
static constexpr u32 PLLDividerSelect3 = 3 << 8;
}

// Registers reached through ClockControlIndex/ClockControlData.
namespace PLLRegister {
static constexpr u8 PixelPLLControl = 0x02;
static constexpr u8 PixelPLLReferenceDivider = 0x03;
static constexpr u8 PixelPLLDivider3 = 0x07;
static constexpr u8 VideoClockControl = 0x08;
static constexpr u8 HorizontalTotalControl = 0x09;
}

namespace PixelPLLControl {
static constexpr u32 Reset = 1 << 0;
static constexpr u32 Sleep = 1 << 1;
static constexpr u32 AtomicUpdateEnable = 1 << 16;
static constexpr u32 VGAAtomicUpdateEnable = 1 << 17;
}

namespace PixelPLLReferenceDivider {
static constexpr u32 Mask = 0x3ff;
static constexpr u32 AtomicUpdate = 1 << 15;
}

namespace PixelPLLDivider3 {
static constexpr u32 FeedbackMask = 0x7ff;
static constexpr u32 PostShift = 16;
static constexpr u32 PostMask = 7 << PostShift;
}

namespace VideoClockControl {
static constexpr u32 SourceMask = 3;
static constexpr u32 SourceCPUClock = 0;
static constexpr u32 SourcePixelPLL = 3;
}

// PLL limits in 10 kHz units, as stored in the legacy video BIOS PLL table.
struct PLLLimits {
    u32 reference_frequency { 0 };
    u32 minimum_vco_frequency { 0 };
    u32 maximum_vco_frequency { 0 };
};

struct PLLDividers {
    u32 reference { 0 };
    u32 feedback { 0 };
    u32 post { 0 };
    u32 post_code { 0 };
    u32 frequency_in_hz { 0 };
};

}
