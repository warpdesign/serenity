# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

CROSS_COMPILE ?= $(abspath ../../../Toolchain/Local/powerpc/bin)/powerpc-serenity-
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
BUILD_DIR ?= build

CFLAGS := -m32 -msoft-float -msdata=none -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -Werror
CXXFLAGS := $(CFLAGS) -std=c++23 -fno-exceptions -fno-rtti -I../../.. -Wno-literal-suffix

.PHONY: check check-quick clean

check-quick:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -std=c11 -DKERNEL -I../../.. -DTEST_POWERPC_LAYOUT -fsyntax-only Test32BitLayout.c
	$(CC) $(CFLAGS) -std=c11 -DKERNEL -I../../.. -fsyntax-only Test32BitLayout.c
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -fsyntax-only TestBootInfo.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -fsyntax-only TestLibCHeaders.cpp
	$(CC) $(CFLAGS) -c ../../../Userland/Libraries/LibC/arch/powerpc/setjmp.S -o $(BUILD_DIR)/setjmp.o
	$(CC) $(CFLAGS) -O0 -I../../../Userland/Libraries/LibC -I../../.. -c TestSetjmp.c -o $(BUILD_DIR)/TestSetjmp.o
	$(CC) $(CFLAGS) -c TestSetjmpRegisters.S -o $(BUILD_DIR)/TestSetjmpRegisters.o

check: check-quick
	$(CC) $(CFLAGS) -fPIC -c ../../../Userland/Libraries/LibC/arch/powerpc/setjmp.S -o $(BUILD_DIR)/setjmp-pic.o

clean:
	rm -rf $(BUILD_DIR)
