/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

extern "C" int pci_open_firmware_address_test();
extern "C" int pci_address_assignment_test();

int main()
{
    return pci_open_firmware_address_test() && pci_address_assignment_test() ? 0 : 1;
}
