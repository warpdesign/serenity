/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

namespace Kernel::PowerPC::Loader {

constexpr char loader_help[] =
    "SerenityOS PowerPC loader options (after serenity.elf):\r\n"
    "  /?       Show this help and prompt for boot options\r\n"
    "  fullram  Use installed RAM, including firmware-reserved RAM\r\n"
    "  fixedio  Use built-in Pegasos 2 legacy I/O addresses\r\n"
    "  ofdump   Print the Open Firmware device tree\r\n"
    "  halt     Stop just before entering the kernel\r\n"
    "  stop     Stop after native allocation preparation\r\n"
    "  realmode Force real-mode firmware calls\r\n"
    "  claimram Claim available RAM through firmware\r\n"
    "  native   Use the native boot path under QEMU\r\n"
    "Kernel options passed through by the loader:\r\n"
    "  dma      Try experimental IDE DMA on physical Pegasos 2\r\n"
    "  weakrng  Allow interrupt-timing entropy during bring-up\r\n"
    "  nodma    Keep IDE in PIO mode (physical default)\r\n"
    "  udma=N   Select supported IDE UDMA mode 0-5\r\n"
    "  root=... Select the root device (native default: lun3:0:0;part1)\r\n"
    "At the prompt, Enter boots with the typed options; exit returns to firmware.\r\n"
    "The typed line replaces the original boot options.\r\n"
    "The loader adds serial_debug automatically.\r\n";

}
