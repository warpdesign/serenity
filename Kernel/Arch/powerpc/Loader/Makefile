# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

CROSS_COMPILE ?= $(abspath ../../../../Toolchain/Local/powerpc/bin)/powerpc-serenity-
KERNEL_ELF ?= $(abspath ../../../../Build/powerpc/Kernel/Kernel)
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
QEMU ?= qemu-system-ppc
BUILD_DIR ?= build

CFLAGS := -m32 -mhard-float -msdata=none -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -Werror
CXXFLAGS := $(CFLAGS) -std=c++23 -fno-exceptions -fno-rtti -DKERNEL -I../../../.. -include ../MemoryLayout.h
LDFLAGS := -m elf32ppcserenity -T linker.ld --build-id=none

.PHONY: all check check-quick clean

all: $(BUILD_DIR)/serenity-powerpc-loader.elf

# CMake already builds this PIE. Suppress make's implicit link rule for its .o.
$(KERNEL_ELF):
	@test -f $@

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/boot.o: boot.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/firmware.o: firmware.cpp KernelImagePlan.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel_image_plan.o: KernelImagePlan.cpp KernelImagePlan.h ../../../../Userland/Libraries/LibELF/ELFABI.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/runtime.o: ../Boot/Runtime.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/kernel.elf: $(KERNEL_ELF) | $(BUILD_DIR)
	cp $< $@

$(BUILD_DIR)/kernel_payload.o: $(BUILD_DIR)/kernel.elf
	cd $(BUILD_DIR) && $(OBJCOPY) -I binary -O elf32-powerpc -B powerpc --rename-section .data=.rodata.kernel,alloc,load,readonly,data,contents kernel.elf kernel_payload.o

$(BUILD_DIR)/serenity-powerpc-loader.elf: $(BUILD_DIR)/boot.o $(BUILD_DIR)/firmware.o $(BUILD_DIR)/kernel_image_plan.o $(BUILD_DIR)/kernel_payload.o $(BUILD_DIR)/runtime.o linker.ld
	$(LD) $(LDFLAGS) $(BUILD_DIR)/boot.o $(BUILD_DIR)/firmware.o $(BUILD_DIR)/kernel_image_plan.o $(BUILD_DIR)/kernel_payload.o $(BUILD_DIR)/runtime.o -o $@

check-quick: all
	python3 ../../../../Tests/Kernel/PowerPC/test_loader.py --qemu $(QEMU) --loader $(BUILD_DIR)/serenity-powerpc-loader.elf --kernel-elf $(KERNEL_ELF) --loader-dir . --cross-compile $(CROSS_COMPILE)

check: check-quick
	python3 ../../../../Tests/Kernel/PowerPC/test_loader.py --qemu $(QEMU) --loader $(BUILD_DIR)/serenity-powerpc-loader.elf --kernel-elf $(KERNEL_ELF) --loader-dir . --cross-compile $(CROSS_COMPILE) --corrupt-only

clean:
	rm -rf $(BUILD_DIR)
