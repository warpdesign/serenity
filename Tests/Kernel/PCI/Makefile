# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

HOST_CXX ?= c++
BUILD_DIR ?= build

.PHONY: check check-quick clean

check-quick: $(BUILD_DIR)/test-open-firmware-address
	$(BUILD_DIR)/test-open-firmware-address

check: check-quick

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test-open-firmware-address: TestOpenFirmwareAddress.cpp TestAddressAssignment.cpp TestOpenFirmwareAddressMain.cpp ../../../Kernel/Bus/PCI/OpenFirmwareAddress.h ../../../Kernel/Bus/PCI/AddressAssignment.h | $(BUILD_DIR)
	$(HOST_CXX) -std=c++23 -O2 -Wall -Wextra -Werror -I../../.. TestOpenFirmwareAddress.cpp TestAddressAssignment.cpp TestOpenFirmwareAddressMain.cpp -o $@

clean:
	rm -rf $(BUILD_DIR)
