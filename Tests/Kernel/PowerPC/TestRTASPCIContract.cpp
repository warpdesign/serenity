/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <Kernel/Arch/powerpc/Pegasos2/RTAS.h>
#include <stddef.h>

static_assert(sizeof(PowerPCRtasArguments) == 64);
static_assert(offsetof(PowerPCRtasArguments, values) == 12);
static_assert(powerpc_rtas_pci_config_address(0, 0, 12, 0, 0) == 0x00006000u);
static_assert(powerpc_rtas_pci_config_address(0, 0, 12, 1, 0x20) == 0x00006120u);
static_assert(powerpc_rtas_pci_config_address(1, 0, 8, 0, 0x10) == 0x01004010u);

int main(int argc, char**)
{
    if (argc == 1)
        return 0;
    if (powerpc_rtas_pci_config_address(1, 255, 31, 7, 0xfc) != 0x01fffffcu)
        return 1;
    return 0;
}
