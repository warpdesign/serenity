# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

CROSS_COMPILE ?= $(abspath ../../../Toolchain/Local/powerpc/bin)/powerpc-serenity-
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
BUILD_DIR ?= build

CFLAGS := -m32 -mhard-float -msdata=none -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -Werror
CXXFLAGS := $(CFLAGS) -std=c++23 -fno-exceptions -fno-rtti -I../../.. -Wno-literal-suffix

.PHONY: check check-quick clean

check-quick:
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -std=c11 -DKERNEL -I../../.. -DTEST_POWERPC_LAYOUT -fsyntax-only Test32BitLayout.c
	$(CC) $(CFLAGS) -std=c11 -DKERNEL -I../../.. -fsyntax-only Test32BitLayout.c
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -fsyntax-only TestBootInfo.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fsyntax-only TestRegisterState.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fno-omit-frame-pointer -fsyntax-only TestStackUnwinder.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -fsyntax-only TestShortStringLayout.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -fsyntax-only TestFloatDigitPacking.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -fsyntax-only ../../../AK/FloatingPointStringConversions.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -fsyntax-only TestFloatParsing.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -frounding-math -fsyntax-only TestFenv.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -fsyntax-only ../../../Userland/Libraries/LibC/arch/powerpc/fenv.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -fsyntax-only ../../../Userland/Libraries/LibC/fenv.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only TestELFHeader.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries -fsyntax-only TestELFImage.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Userland/Libraries/LibELF/Validation.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Userland/Libraries/LibELF/Relocation.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only TestRelativeRelocations.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only TestDynamicObjectHeader.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only TestTLSLayout.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only TestTLSLookup.cpp
	$(CXX) $(CXXFLAGS) -fsyntax-only TestELFEntry.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only TestSyscallInvoke.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Userland/Libraries/LibSystem/syscall.cpp
	$(CC) $(CFLAGS) -c TestSyscallInvoke.S -o $(BUILD_DIR)/TestSyscallInvoke.o
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries -fsyntax-only TestELFBreakpoint.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries -fsyntax-only TestSecurePLT.cpp
	mkdir -p $(BUILD_DIR)/AK
	sed -E 's/^#    cmakedefine01 ([A-Z0-9_]+)/#    define \1 0/' ../../../AK/Debug.h.in > $(BUILD_DIR)/AK/Debug.h
	$(CXX) $(CXXFLAGS) -msecure-plt -I$(BUILD_DIR) -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Userland/Libraries/LibELF/DynamicLoader.cpp
	$(CXX) $(CXXFLAGS) -msecure-plt -I$(BUILD_DIR) -I../../../Userland/Libraries/LibSystem -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Userland/Libraries/LibELF/DynamicLinker.cpp
	$(CXX) $(CXXFLAGS) -I../../../Userland/Libraries/LibC -fsyntax-only TestLibCHeaders.cpp
	$(CC) $(CFLAGS) -c ../../../Userland/Libraries/LibC/arch/powerpc/setjmp.S -o $(BUILD_DIR)/setjmp.o
	$(CC) $(CFLAGS) -O0 -I../../../Userland/Libraries/LibC -I../../.. -c TestSetjmp.c -o $(BUILD_DIR)/TestSetjmp.o
	$(CC) $(CFLAGS) -c TestSetjmpRegisters.S -o $(BUILD_DIR)/TestSetjmpRegisters.o

check: check-quick
	$(CC) $(CFLAGS) -fPIC -c ../../../Userland/Libraries/LibC/arch/powerpc/setjmp.S -o $(BUILD_DIR)/setjmp-pic.o

clean:
	rm -rf $(BUILD_DIR)
