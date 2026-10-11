# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

HOST_CXX ?= c++
BUILD_DIR ?= build

.PHONY: check check-quick clean

check-quick: $(BUILD_DIR)/test-endian
	$(BUILD_DIR)/test-endian

check: check-quick

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test-endian: TestEndian.cpp TestEndianMain.cpp ../../../Kernel/Devices/Storage/NVMe/NVMeDefinitions.h ../../../Kernel/Devices/Storage/NVMe/NVMeEndian.h | $(BUILD_DIR)
	$(HOST_CXX) -std=c++23 -O2 -Wall -Wextra -Werror -I../../.. TestEndian.cpp TestEndianMain.cpp -o $@

clean:
	rm -rf $(BUILD_DIR)
