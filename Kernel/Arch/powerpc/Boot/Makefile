# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

CROSS_COMPILE ?= $(abspath ../../../../Toolchain/Local/powerpc/bin)/powerpc-serenity-
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
QEMU ?= qemu-system-ppc
BUILD_DIR ?= build

CFLAGS := -m32 -msoft-float -msdata=none -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -Werror
CXXFLAGS := $(CFLAGS) -std=c++23 -fno-exceptions -fno-rtti -DKERNEL -I../../../.. -include ../MemoryLayout.h
LDFLAGS := -m elf32ppcserenity -T linker.ld --build-id=none

.PHONY: all check check-quick clean

all: $(BUILD_DIR)/serenity-powerpc-boot.elf

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/boot.o: boot.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/firmware.o: firmware.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/exception_probe.o: ExceptionProbe.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/exception_probe_vector.o: ExceptionProbe.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/user_mode_probe.o: UserModeProbe.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mmu_probe.o: MMUProbe.c ../MemoryLayout.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../../../.. -c $< -o $@

$(BUILD_DIR)/mmu_probe_asm.o: MMUProbe.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/hash_page_table.o: ../HashPageTable.c ../HashPageTable.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../../../.. -c $< -o $@

$(BUILD_DIR)/hpt_probe.o: HPTProbe.c ../ExceptionState.h ../HashPageTable.h ../MemoryLayout.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../../../.. -c $< -o $@

$(BUILD_DIR)/abi.o: abi.cpp ../MemoryLayout.h ../../../../AK/Platform.h ../../../../AK/Types.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/setjmp.o: ../../../../Userland/Libraries/LibC/arch/powerpc/setjmp.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_setjmp.o: ../../../../Tests/Kernel/PowerPC/TestSetjmp.c ../../../../Userland/Libraries/LibC/setjmp.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -O0 -I../../../../Userland/Libraries/LibC -I../../../.. -c $< -o $@

$(BUILD_DIR)/test_setjmp_registers.o: ../../../../Tests/Kernel/PowerPC/TestSetjmpRegisters.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_short_string_layout.o: ../../../../Tests/Kernel/PowerPC/TestShortStringLayout.cpp ../../../../AK/StringBase.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_float_digit_packing.o: ../../../../Tests/Kernel/PowerPC/TestFloatDigitPacking.cpp ../../../../AK/FloatingPointDigits.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/serenity-powerpc-boot.elf: $(BUILD_DIR)/boot.o $(BUILD_DIR)/firmware.o $(BUILD_DIR)/exception_probe.o $(BUILD_DIR)/exception_probe_vector.o $(BUILD_DIR)/user_mode_probe.o $(BUILD_DIR)/mmu_probe.o $(BUILD_DIR)/mmu_probe_asm.o $(BUILD_DIR)/hash_page_table.o $(BUILD_DIR)/hpt_probe.o $(BUILD_DIR)/abi.o $(BUILD_DIR)/setjmp.o $(BUILD_DIR)/test_setjmp.o $(BUILD_DIR)/test_setjmp_registers.o $(BUILD_DIR)/test_short_string_layout.o $(BUILD_DIR)/test_float_digit_packing.o linker.ld Makefile
	$(LD) $(LDFLAGS) $(BUILD_DIR)/boot.o $(BUILD_DIR)/firmware.o $(BUILD_DIR)/exception_probe.o $(BUILD_DIR)/exception_probe_vector.o $(BUILD_DIR)/user_mode_probe.o $(BUILD_DIR)/mmu_probe.o $(BUILD_DIR)/mmu_probe_asm.o $(BUILD_DIR)/hash_page_table.o $(BUILD_DIR)/hpt_probe.o $(BUILD_DIR)/abi.o $(BUILD_DIR)/setjmp.o $(BUILD_DIR)/test_setjmp.o $(BUILD_DIR)/test_setjmp_registers.o $(BUILD_DIR)/test_short_string_layout.o $(BUILD_DIR)/test_float_digit_packing.o -o $@

check-quick: all
	$(MAKE) -C ../../../../Tests/Toolchain/PowerPC check-quick CROSS_COMPILE=$(CROSS_COMPILE)
	$(MAKE) -C ../../../../Tests/Kernel/PowerPC check-quick CROSS_COMPILE=$(CROSS_COMPILE)
	python3 ../../../../Tests/Kernel/PowerPC/test_boot.py --qemu $(QEMU) --kernel $(BUILD_DIR)/serenity-powerpc-boot.elf --memory 256

check: check-quick
	python3 ../../../../Tests/Kernel/PowerPC/test_boot.py --qemu $(QEMU) --kernel $(BUILD_DIR)/serenity-powerpc-boot.elf --memory 512 --cpu 750cxe

clean:
	rm -rf $(BUILD_DIR)
