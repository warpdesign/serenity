/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#define PEGASOS2_PCI_IO_PHYSICAL_BASE 0xfe000000u
#define PEGASOS2_UART_PHYSICAL_BASE (PEGASOS2_PCI_IO_PHYSICAL_BASE + 0x2f8u)
#define PEGASOS2_INTACK_PHYSICAL_BASE 0xf1000000u
#define PEGASOS2_INTACK_OFFSET 0xcb4u
