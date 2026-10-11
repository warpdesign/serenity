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
	mkdir -p $(BUILD_DIR)/AK $(BUILD_DIR)/Kernel
	sed -E 's/^#    cmakedefine01 ([A-Z0-9_]+)/#    define \1 0/' ../../../AK/Debug.h.in > $(BUILD_DIR)/AK/Debug.h
	sed -E 's/^#cmakedefine01 ([A-Z0-9_]+)/#define \1 0/' ../../../Kernel/Debug.h.in > $(BUILD_DIR)/Kernel/Debug.h
	$(CC) $(CFLAGS) -std=c11 -DKERNEL -I../../.. -DTEST_POWERPC_LAYOUT -fsyntax-only Test32BitLayout.c
	$(CC) $(CFLAGS) -std=c11 -DKERNEL -I../../.. -fsyntax-only Test32BitLayout.c
	$(CXX) $(CXXFLAGS) -DKERNEL -fsyntax-only ../../../Kernel/Arch/powerpc/SegmentRegisters.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -fsyntax-only ../../../Kernel/Arch/powerpc/HashPageTable.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -fsyntax-only ../../../Kernel/Arch/powerpc/TLB.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fsyntax-only ../../../Kernel/Arch/powerpc/PageDirectory.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -fsyntax-only ../../../Kernel/Arch/powerpc/PageDirectoryBackend.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -fsyntax-only ../../../Kernel/Arch/powerpc/PhysicalPageArray.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -fsyntax-only ../../../Kernel/Arch/powerpc/OpenFirmwareMemoryMap.cpp
	$(CC) $(CFLAGS) -std=c11 -DKERNEL -I../../.. -fsyntax-only TestFirmwareMemoryMap.c
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -fsyntax-only TestPageMapping.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -fsyntax-only ../../../Kernel/Arch/powerpc/VSIDAllocator.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -fsyntax-only TestVSIDAllocator.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -fsyntax-only TestBootInfo.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fsyntax-only ../../../Kernel/Arch/powerpc/BootHandoff.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -fsyntax-only TestCPUFeatures.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fsyntax-only TestProcessor.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fsyntax-only ../../../Kernel/Arch/powerpc/Processor.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -I$(BUILD_DIR) -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Kernel/Arch/Processor.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -I$(BUILD_DIR) -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Kernel/Arch/powerpc/InterruptManagement.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -I$(BUILD_DIR) -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Kernel/Arch/powerpc/Interrupts.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -I$(BUILD_DIR) -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Kernel/Arch/powerpc/Pegasos2/InterruptController.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -I$(BUILD_DIR) -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Kernel/Bus/PCI/Controller/HostController.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -fsyntax-only TestPCIMSI.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -I$(BUILD_DIR) -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Kernel/Bus/PCI/Device.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -I$(BUILD_DIR) -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Kernel/Devices/Storage/NVMe/NVMeController.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -I$(BUILD_DIR) -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Kernel/Devices/Storage/NVMe/NVMeQueue.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -I$(BUILD_DIR) -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland -I../../../Userland/Libraries/LibC -I../../../Userland/Libraries -fsyntax-only ../../../Kernel/Bus/PCI/DeviceTreeHelpers.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -fsyntax-only TestMemoryFences.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fsyntax-only TestCacheMaintenance.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fsyntax-only TestTLBMaintenance.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -fsyntax-only ../../../Kernel/Arch/powerpc/CPUID.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fsyntax-only TestRegisterState.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fsyntax-only TestThreadRegisters.cpp
	$(CXX) $(CXXFLAGS) -DKERNEL -include ../../../Kernel/Arch/powerpc/MemoryLayout.h -I../../../Userland/Libraries/LibC -fsyntax-only TestFPUState.cpp
	$(CC) $(CFLAGS) -c ../../../Kernel/Arch/powerpc/FPUState.S -o $(BUILD_DIR)/FPUState.o
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
