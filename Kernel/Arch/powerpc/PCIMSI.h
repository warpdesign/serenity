/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Assertions.h>
#include <AK/Platform.h>
#include <AK/Types.h>

VALIDATE_IS_POWERPC32()

namespace Kernel {

// The Pegasos 2 controller currently routes only its legacy 8259 IRQ lines.
inline constexpr bool pci_msi_supported() { return false; }

inline u64 msi_address_register(u8, bool, bool)
{
    VERIFY_NOT_REACHED();
}

inline u32 msi_data_register(u8, bool, bool)
{
    VERIFY_NOT_REACHED();
}

inline u32 msix_vector_control_register(u32, bool)
{
    VERIFY_NOT_REACHED();
}

inline void msi_signal_eoi()
{
    VERIFY_NOT_REACHED();
}

}
