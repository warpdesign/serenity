/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Types.h>

namespace Kernel::PowerPC::Pegasos2 {

Optional<u32> pci_config_address(u8 bus, u8 device, u8 function, u16 offset);
Optional<u8> read_pci_config8(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset);
Optional<u16> read_pci_config16(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset);
Optional<u32> read_pci_config32(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset);
bool write_pci_config8(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset, u8 value);
bool write_pci_config16(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset, u16 value);
bool write_pci_config32(volatile u32* address_register, volatile u32* data_register,
    u8 bus, u8 device, u8 function, u16 offset, u32 value);

}
