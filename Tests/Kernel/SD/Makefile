# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

HOST_CXX ?= c++
BUILD_DIR ?= build

.PHONY: check check-quick clean

check-quick: $(BUILD_DIR)/test-adma-address
	$(BUILD_DIR)/test-adma-address

check: check-quick

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/test-adma-address: TestADMAAddress.cpp TestADMAAddressMain.cpp ../../../Kernel/Devices/Storage/SD/ADMAAddress.h | $(BUILD_DIR)
	$(HOST_CXX) -std=c++23 -O2 -Wall -Wextra -Werror -I../../.. TestADMAAddress.cpp TestADMAAddressMain.cpp -o $@

clean:
	rm -rf $(BUILD_DIR)
