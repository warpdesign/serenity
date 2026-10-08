# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

CROSS_COMPILE ?= $(abspath ../../../Toolchain/Local/powerpc/bin)/powerpc-serenity-
CC := $(CROSS_COMPILE)gcc
READELF := $(CROSS_COMPILE)readelf
BUILD_DIR ?= build

.PHONY: check check-quick clean

check: check-quick

check-quick:
	mkdir -p $(BUILD_DIR)
	python3 test_toolchain.py --compiler $(CC) --readelf $(READELF)
	cmake -DSERENITY_SOURCE_DIR=$(abspath ../../..) -DTEST_BINARY_DIR=$(abspath $(BUILD_DIR)) -P TestGNUToolchain.cmake
	cmake -S CMakeDataModel -B $(BUILD_DIR)/CMakeDataModel -DCMAKE_TOOLCHAIN_FILE=$(abspath $(BUILD_DIR))/GNUToolchain-powerpc.cmake

clean:
	rm -rf $(BUILD_DIR)
