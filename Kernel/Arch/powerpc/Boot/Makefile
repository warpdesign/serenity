# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

CROSS_COMPILE ?= $(abspath ../../../../Toolchain/Local/powerpc/bin)/powerpc-serenity-
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
LD := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
QEMU ?= qemu-system-ppc
BUILD_DIR ?= build

CFLAGS := -m32 -mhard-float -msdata=none -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin -O2 -Wall -Wextra -Werror
CXXFLAGS := $(CFLAGS) -std=c++23 -fno-exceptions -fno-rtti -DKERNEL -I../../../.. -include ../MemoryLayout.h
PARSER_CXXFLAGS := $(CFLAGS) -std=c++23 -fno-exceptions -fno-rtti -I../../../.. -I../../../../Userland/Libraries/LibC -Wno-literal-suffix
IMAGE_CXXFLAGS := $(CXXFLAGS) -I$(BUILD_DIR) -I../../../../Userland/Libraries -Wno-literal-suffix
LDFLAGS := -m elf32ppcserenity -T linker.ld --build-id=none
LIBGCC := $(shell $(CC) -print-libgcc-file-name)
TLS_TEST_OBJECTS := $(BUILD_DIR)/libc_tls.o $(BUILD_DIR)/test_tls_lookup.o
PLT_TEST_OBJECTS := $(BUILD_DIR)/secure_plt_fixture.o $(BUILD_DIR)/test_secure_plt.o $(BUILD_DIR)/plt_trampoline.o
SYSCALL_TEST_OBJECTS := $(BUILD_DIR)/libsystem_syscall.o $(BUILD_DIR)/test_syscall_invoke.o $(BUILD_DIR)/test_syscall_invoke_vector.o $(BUILD_DIR)/test_elf_breakpoint.o
FENV_TEST_OBJECTS := $(BUILD_DIR)/libc_powerpc_fenv.o $(BUILD_DIR)/libc_fenv.o $(BUILD_DIR)/test_fenv.o
FPU_STATE_TEST_OBJECTS := $(BUILD_DIR)/fpu_state.o $(BUILD_DIR)/test_fpu_state.o
CPU_TEST_OBJECTS := $(BUILD_DIR)/cpu_features.o $(BUILD_DIR)/test_cpu_features.o
PROCESSOR_TEST_OBJECTS := $(BUILD_DIR)/processor.o $(BUILD_DIR)/test_processor.o $(BUILD_DIR)/deferred_call_pool.o
MEMORY_FENCES_TEST_OBJECTS := $(BUILD_DIR)/test_memory_fences.o
CACHE_TEST_OBJECTS := $(BUILD_DIR)/test_cache_maintenance.o
TLB_TEST_OBJECTS := $(BUILD_DIR)/tlb.o $(BUILD_DIR)/test_tlb_maintenance.o
SEGMENT_REGISTER_OBJECTS := $(BUILD_DIR)/segment_registers.o
PAGE_DIRECTORY_BACKEND_OBJECTS := $(BUILD_DIR)/page_directory_backend.o
PHYSICAL_PAGE_ARRAY_OBJECTS := $(BUILD_DIR)/physical_page_array.o
PAGE_MAPPING_TEST_OBJECTS := $(BUILD_DIR)/test_page_mapping.o
VSID_ALLOCATOR_OBJECTS := $(BUILD_DIR)/vsid_allocator.o $(BUILD_DIR)/test_vsid_allocator.o
FIRMWARE_MEMORY_MAP_TEST_OBJECTS := $(BUILD_DIR)/open_firmware_memory_map.o $(BUILD_DIR)/test_firmware_memory_map.o
BOOT_INFO_TEST_OBJECTS := $(BUILD_DIR)/boot_handoff.o $(BUILD_DIR)/test_boot_info.o $(BUILD_DIR)/rtas_call.o
REGISTER_TEST_OBJECTS := $(BUILD_DIR)/test_register_state.o $(BUILD_DIR)/test_thread_registers.o $(BUILD_DIR)/fork_registers.o $(BUILD_DIR)/process_entry.o $(BUILD_DIR)/create_thread.o $(BUILD_DIR)/register_dump.o
STACK_TEST_OBJECTS := $(BUILD_DIR)/test_stack_unwinder.o
TIMER_TEST_OBJECTS := $(BUILD_DIR)/test_decrementer.o
PIC_TEST_OBJECTS := $(BUILD_DIR)/test_legacy_pic.o
EXTERNAL_IRQ_TEST_OBJECTS := $(BUILD_DIR)/test_external_irq.o
PCI_TEST_OBJECTS := $(BUILD_DIR)/test_pci_open_firmware_address.o $(BUILD_DIR)/test_pci_address_assignment.o
PEGASOS2_PCI_CONFIG_TEST_OBJECTS := $(BUILD_DIR)/pegasos2_pci_config.o $(BUILD_DIR)/test_pegasos2_pci_config.o $(BUILD_DIR)/open_firmware_pci_ranges.o $(BUILD_DIR)/test_open_firmware_pci_ranges.o
NVME_TEST_OBJECTS := $(BUILD_DIR)/test_nvme_endian.o
SD_TEST_OBJECTS := $(BUILD_DIR)/test_sd_adma_address.o
KEY_EVENT_TEST_OBJECTS := $(BUILD_DIR)/test_deferred_key_press.o
SIGNAL_TEST_OBJECTS := $(BUILD_DIR)/signal_trampoline.o $(BUILD_DIR)/test_signal_trampoline.o
SMAP_TEST_OBJECTS := $(BUILD_DIR)/smap_disabler.o $(BUILD_DIR)/test_smap_disabler.o
SAFEMEM_TEST_OBJECTS := $(BUILD_DIR)/safe_mem.o $(BUILD_DIR)/safe_mem_asm.o $(BUILD_DIR)/test_safe_mem.o
ATOMIC64_TEST_OBJECTS := $(BUILD_DIR)/atomic64.o $(BUILD_DIR)/test_atomic64.o
DEBUG_OUTPUT_TEST_OBJECTS := $(BUILD_DIR)/debug_output.o $(BUILD_DIR)/test_debug_output.o
CURRENT_TIME_TEST_OBJECTS := $(BUILD_DIR)/current_time.o $(BUILD_DIR)/test_current_time.o
KERNEL_CONTEXT_TEST_OBJECTS := $(BUILD_DIR)/kernel_context.o $(BUILD_DIR)/test_kernel_context.o $(BUILD_DIR)/kernel_exception_return.o $(BUILD_DIR)/test_kernel_exception_return.o
INITIAL_THREAD_CONTEXT_TEST_OBJECTS := $(BUILD_DIR)/initial_thread_context.o $(BUILD_DIR)/initial_thread_entry.o $(BUILD_DIR)/test_initial_thread_context.o
EXCEPTION_VECTOR_TEST_OBJECTS := $(BUILD_DIR)/exception_vector_installer.o

.PHONY: all check check-quick clean

all: $(BUILD_DIR)/serenity-powerpc-boot.elf

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/AK/Debug.h: ../../../../AK/Debug.h.in | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/AK
	sed -E 's/^#    cmakedefine01 ([A-Z0-9_]+)/#    define \1 0/' $< > $@

$(BUILD_DIR)/Kernel/Debug.h: ../../../../Kernel/Debug.h.in | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/Kernel
	sed -E 's/^#cmakedefine01 ([A-Z0-9_]+)/#define \1 0/' $< > $@

$(BUILD_DIR)/boot.o: boot.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/firmware.o: firmware.c ../OpenFirmwareMemoryMap.h ../Pegasos2/HardwareAddresses.h ../Pegasos2/RTAS.h ../../../../Kernel/Boot/BootMemoryMap.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../../../.. -c $< -o $@

$(BUILD_DIR)/exception_probe.o: ExceptionProbe.c ../ExceptionVectorInstaller.h ../ExceptionVectorAddresses.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../../../.. -c $< -o $@

$(BUILD_DIR)/exception_vector_installer.o: ../ExceptionVectorInstaller.cpp ../ExceptionVectorInstaller.h ../ExceptionVectorAddresses.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/exception_probe_vector.o: ExceptionProbe.S ProbeAddresses.h ../HighSegmentSwitch.inc | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/user_mode_probe.o: UserModeProbe.S ../HighSegmentSwitch.inc ../ExceptionVectorAddresses.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/mmu_probe.o: MMUProbe.c ../MemoryLayout.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../../../.. -c $< -o $@

$(BUILD_DIR)/mmu_probe_asm.o: MMUProbe.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/hash_page_table.o: ../HashPageTable.cpp ../HashPageTable.h ../TLB.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/hpt_probe.o: HPTProbe.c ProbeAddresses.h ../Pegasos2/HardwareAddresses.h ../ExceptionState.h ../HashPageTable.h ../MemoryLayout.h ../PageDirectoryBackend.h ../PhysicalPageArray.h ../SegmentRegisters.h ../VSIDAllocator.h ../../../../Kernel/Sections.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../../../.. -c $< -o $@

$(BUILD_DIR)/physical_page_array.o: ../PhysicalPageArray.cpp ../PhysicalPageArray.h ../PageDirectoryBackend.h ../../../../Kernel/Memory/PageMapping.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/page_directory_backend.o: ../PageDirectoryBackend.cpp ../PageDirectoryBackend.h ../HashPageTable.h ../SegmentRegisters.h ../VSIDAllocator.h ../../../../Kernel/Memory/PageMapping.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_page_mapping.o: ../../../../Tests/Kernel/PowerPC/TestPageMapping.cpp ../PageDirectoryBackend.h ../PhysicalPageArray.h ../../../../Kernel/Memory/PageMapping.h ../../../../Kernel/Sections.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/segment_registers.o: ../SegmentRegisters.cpp ../SegmentRegisters.h ../VSIDAllocator.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/vsid_allocator.o: ../VSIDAllocator.cpp ../VSIDAllocator.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/open_firmware_memory_map.o: ../OpenFirmwareMemoryMap.cpp ../OpenFirmwareMemoryMap.h ../../../../Kernel/Boot/BootMemoryMap.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_firmware_memory_map.o: ../../../../Tests/Kernel/PowerPC/TestFirmwareMemoryMap.c ../OpenFirmwareMemoryMap.h ../../../../Kernel/Boot/BootMemoryMap.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../../../.. -c $< -o $@

$(BUILD_DIR)/boot_handoff.o: ../BootHandoff.cpp ../BootHandoff.h ../ExceptionVectorAddresses.h ../Time/Timebase.h ../Pegasos2/RTAS.h ../../../../Kernel/Prekernel/Arch/powerpc/ArchSpecificBootInfo.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_boot_info.o: ../../../../Tests/Kernel/PowerPC/TestBootInfo.cpp ../BootHandoff.h ../ExceptionVectorAddresses.h ../Time/Timebase.h ../Pegasos2/RTAS.h ../../../../Kernel/Prekernel/Arch/powerpc/ArchSpecificBootInfo.h ../../../../Kernel/Sections.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/rtas_call.o: ../Pegasos2/RTASCall.S ../Pegasos2/RTAS.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_vsid_allocator.o: ../../../../Tests/Kernel/PowerPC/TestVSIDAllocator.cpp ../VSIDAllocator.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/abi.o: abi.cpp ../MemoryLayout.h ../../../../AK/Platform.h ../../../../AK/Types.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/cpu_features.o: ../CPUID.cpp ../CPUID.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_cpu_features.o: ../../../../Tests/Kernel/PowerPC/TestCPUFeatures.cpp ../CPUID.h ../CPU.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/processor.o: ../Processor.cpp ../Processor.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_processor.o: ../../../../Tests/Kernel/PowerPC/TestProcessor.cpp ../Processor.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/deferred_call_pool.o: ../../../../Kernel/Arch/DeferredCallPool.cpp ../../../../Kernel/Arch/DeferredCallPool.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_memory_fences.o: ../../../../Tests/Kernel/PowerPC/TestMemoryFences.cpp ../MemoryFences.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_cache_maintenance.o: ../../../../Tests/Kernel/PowerPC/TestCacheMaintenance.cpp ../Processor.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_tlb_maintenance.o: ../../../../Tests/Kernel/PowerPC/TestTLBMaintenance.cpp ../Processor.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/tlb.o: ../TLB.cpp ../TLB.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/setjmp.o: ../../../../Userland/Libraries/LibC/arch/powerpc/setjmp.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_setjmp.o: ../../../../Tests/Kernel/PowerPC/TestSetjmp.c ../../../../Userland/Libraries/LibC/setjmp.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -O0 -I../../../../Userland/Libraries/LibC -I../../../.. -c $< -o $@

$(BUILD_DIR)/test_setjmp_registers.o: ../../../../Tests/Kernel/PowerPC/TestSetjmpRegisters.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_short_string_layout.o: ../../../../Tests/Kernel/PowerPC/TestShortStringLayout.cpp ../../../../AK/StringBase.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_bitmap_endian.o: ../../../../Tests/Kernel/PowerPC/TestBitmapEndian.cpp ../../../../AK/BitmapView.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_mbr_wire_format.o: ../../../../Tests/Kernel/PowerPC/TestMBRWireFormat.cpp ../../../../Userland/Libraries/LibPartition/MBRPartitionTable.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/test_ext2_wire_format.o: ../../../../Tests/Kernel/PowerPC/TestExt2WireFormat.cpp ../../../../Kernel/FileSystem/Ext2FS/Definitions.h ../../../../Kernel/FileSystem/Ext2FS/ext2_types.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_float_digit_packing.o: ../../../../Tests/Kernel/PowerPC/TestFloatDigitPacking.cpp ../../../../AK/FloatingPointDigits.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_float_extractor_endian.o: ../../../../Tests/Kernel/PowerPC/TestFloatExtractorEndian.cpp ../../../../AK/FloatingPoint.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/float_parser.o: ../../../../AK/FloatingPointStringConversions.cpp ../../../../AK/FloatingPointDigits.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_float_parser.o: ../../../../Tests/Kernel/PowerPC/TestFloatParsing.cpp | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/libc_powerpc_fenv.o: ../../../../Userland/Libraries/LibC/arch/powerpc/fenv.cpp ../../../../Userland/Libraries/LibC/arch/powerpc/fenv.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/libc_fenv.o: ../../../../Userland/Libraries/LibC/fenv.cpp | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_fenv.o: ../../../../Tests/Kernel/PowerPC/TestFenv.cpp | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -frounding-math -c $< -o $@

$(BUILD_DIR)/fpu_state.o: ../FPUState.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_fpu_state.o: ../../../../Tests/Kernel/PowerPC/TestFPUState.cpp ../FPUState.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/elf_validation.o: ../../../../Userland/Libraries/LibELF/Validation.cpp ../../../../Userland/Libraries/LibELF/ELFABI.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/test_elf_header.o: ../../../../Tests/Kernel/PowerPC/TestELFHeader.cpp ../../../../Userland/Libraries/LibELF/Validation.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/elf_machine.o: ../ELFMachine.cpp ../../../../Kernel/Arch/ELFMachine.h ../../../../Userland/Libraries/LibELF/ELFABI.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/elf_image.o: ../../../../Userland/Libraries/LibELF/Image.cpp ../../../../Userland/Libraries/LibELF/Image.h $(BUILD_DIR)/AK/Debug.h | $(BUILD_DIR)
	$(CXX) $(IMAGE_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_elf_image.o: ../../../../Tests/Kernel/PowerPC/TestELFImage.cpp ../../../../Userland/Libraries/LibELF/Image.h | $(BUILD_DIR)
	$(CXX) $(IMAGE_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/elf_relocation.o: ../../../../Userland/Libraries/LibELF/Relocation.cpp ../../../../Userland/Libraries/LibELF/Arch/powerpc/GenericDynamicRelocationType.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/test_relative_relocations.o: ../../../../Tests/Kernel/PowerPC/TestRelativeRelocations.cpp | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/elf_tls.o: ../../../../Userland/Libraries/LibELF/Arch/powerpc/tls.cpp ../../../../Userland/Libraries/LibELF/Arch/powerpc/tls.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/test_tls_layout.o: ../../../../Tests/Kernel/PowerPC/TestTLSLayout.cpp ../../../../Userland/Libraries/LibELF/Arch/powerpc/tls.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/libc_tls.o: ../../../../Userland/Libraries/LibC/tls.cpp ../../../../Userland/Libraries/LibELF/Arch/powerpc/tls.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/test_tls_lookup.o: ../../../../Tests/Kernel/PowerPC/TestTLSLookup.cpp ../../../../Userland/Libraries/LibELF/Arch/powerpc/GenericDynamicRelocationType.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/elf_entry.o: ../../../../Userland/Libraries/LibELF/Arch/powerpc/entry.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_elf_entry.o: ../../../../Tests/Kernel/PowerPC/TestELFEntry.cpp | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/secure_plt_probe.so: ../../../../Tests/Kernel/PowerPC/SecurePLTProbe.c | $(BUILD_DIR)
	$(CC) -m32 -mhard-float -msdata=none -msecure-plt -fPIC -shared -nostdlib -fno-stack-protector -O2 $< -o $@

$(BUILD_DIR)/secure_plt_fixture.o: $(BUILD_DIR)/secure_plt_probe.so
	cd $(BUILD_DIR) && $(OBJCOPY) -I binary -O elf32-powerpc -B powerpc secure_plt_probe.so secure_plt_fixture.o

$(BUILD_DIR)/test_secure_plt.o: ../../../../Tests/Kernel/PowerPC/TestSecurePLT.cpp ../../../../Userland/Libraries/LibELF/ELFABI.h | $(BUILD_DIR)
	$(CXX) $(IMAGE_CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/plt_trampoline.o: ../../../../Userland/Libraries/LibELF/Arch/powerpc/plt_trampoline.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_syscall_invoke.o: ../../../../Tests/Kernel/PowerPC/TestSyscallInvoke.cpp ../../../../Kernel/Arch/powerpc/SyscallInvoke.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/libsystem_syscall.o: ../../../../Userland/Libraries/LibSystem/syscall.cpp ../../../../Kernel/Arch/powerpc/SyscallInvoke.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/test_syscall_invoke_vector.o: ../../../../Tests/Kernel/PowerPC/TestSyscallInvoke.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_elf_breakpoint.o: ../../../../Tests/Kernel/PowerPC/TestELFBreakpoint.cpp ../../../../Userland/Libraries/LibELF/Arch/powerpc/breakpoint.h | $(BUILD_DIR)
	$(CXX) $(PARSER_CXXFLAGS) -I../../../../Userland/Libraries -c $< -o $@

$(BUILD_DIR)/test_register_state.o: ../../../../Tests/Kernel/PowerPC/TestRegisterState.cpp ../../../../Kernel/Arch/powerpc/RegisterState.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/register_dump.o: ../RegisterDump.cpp ../RegisterState.h $(BUILD_DIR)/Kernel/Debug.h | $(BUILD_DIR)
	$(CXX) $(IMAGE_CXXFLAGS) -I../../../../Userland/Libraries/LibC -c $< -o $@

$(BUILD_DIR)/test_thread_registers.o: ../../../../Tests/Kernel/PowerPC/TestThreadRegisters.cpp ../ThreadRegisters.h ../ArchSpecificThreadData.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/fork_registers.o: ../ForkRegisters.cpp ../../../../Kernel/Arch/ForkRegisters.h ../ThreadRegisters.h ../RegisterState.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/process_entry.o: ../ProcessEntry.cpp ../../../../Kernel/Arch/ProcessEntry.h ../ThreadRegisters.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/create_thread.o: ../CreateThread.cpp ../../../../Kernel/Arch/CreateThread.h ../ThreadRegisters.h ../../../../Kernel/API/Syscall.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_stack_unwinder.o: ../../../../Tests/Kernel/PowerPC/TestStackUnwinder.cpp ../../../../AK/Arch/powerpc/StackUnwinder.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -fno-omit-frame-pointer -fno-optimize-sibling-calls -c $< -o $@

$(BUILD_DIR)/test_decrementer.o: ../../../../Tests/Kernel/PowerPC/TestDecrementer.cpp ../Time/Timebase.h ../Pegasos2/LegacyPIC.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_legacy_pic.o: ../../../../Tests/Kernel/PowerPC/TestLegacyPIC.cpp ../Time/Timebase.h ../Pegasos2/LegacyPIC.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_external_irq.o: ../../../../Tests/Kernel/PowerPC/TestExternalIRQ.cpp ProbeAddresses.h ../Time/Timebase.h ../Pegasos2/LegacyPIC.h ../ExceptionState.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_pci_open_firmware_address.o: ../../../../Tests/Kernel/PCI/TestOpenFirmwareAddress.cpp ../../../../Kernel/Bus/PCI/OpenFirmwareAddress.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/pegasos2_pci_config.o: ../Pegasos2/PCIConfig.cpp ../Pegasos2/PCIConfig.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_pegasos2_pci_config.o: ../../../../Tests/Kernel/PowerPC/TestPegasos2PCIConfig.cpp ../Pegasos2/PCIConfig.h ../Pegasos2/HardwareAddresses.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/open_firmware_pci_ranges.o: ../Pegasos2/OpenFirmwarePCIRanges.cpp ../Pegasos2/OpenFirmwarePCIRanges.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/test_open_firmware_pci_ranges.o: ../../../../Tests/Kernel/PowerPC/TestOpenFirmwarePCIRanges.c ../Pegasos2/OpenFirmwarePCIRanges.h ../Pegasos2/HardwareAddresses.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I../../../.. -c $< -o $@

$(BUILD_DIR)/test_pci_address_assignment.o: ../../../../Tests/Kernel/PCI/TestAddressAssignment.cpp ../../../../Kernel/Bus/PCI/AddressAssignment.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_nvme_endian.o: ../../../../Tests/Kernel/NVMe/TestEndian.cpp ../../../../Kernel/Devices/Storage/NVMe/NVMeDefinitions.h ../../../../Kernel/Devices/Storage/NVMe/NVMeEndian.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_sd_adma_address.o: ../../../../Tests/Kernel/SD/TestADMAAddress.cpp ../../../../Kernel/Devices/Storage/SD/ADMAAddress.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_deferred_key_press.o: ../../../../Tests/Kernel/PowerPC/TestDeferredKeyPress.cpp ../../../../Kernel/Devices/TTY/DeferredKeyPress.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/signal_trampoline.o: ../SignalTrampoline.cpp ../../../../Kernel/API/Syscall.h ../../../../Kernel/API/POSIX/ucontext.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_signal_trampoline.o: ../../../../Tests/Kernel/PowerPC/TestSignalTrampoline.cpp ../../../../Kernel/API/POSIX/ucontext.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/smap_disabler.o: ../SmapDisabler.cpp ../../../../Kernel/Arch/SmapDisabler.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_smap_disabler.o: ../../../../Tests/Kernel/PowerPC/TestSmapDisabler.cpp ../CPU.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/safe_mem.o: ../SafeMem.cpp ../../../../Kernel/Arch/SafeMem.h ../RegisterState.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/safe_mem_asm.o: ../SafeMem.S | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_safe_mem.o: ../../../../Tests/Kernel/PowerPC/TestSafeMem.cpp ../../../../Kernel/Arch/SafeMem.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/atomic64.o: ../Atomic64.cpp ../CPU.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_atomic64.o: ../../../../Tests/Kernel/PowerPC/TestAtomic64.cpp ../CPU.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/debug_output.o: ../Pegasos2/DebugOutput.cpp ../Pegasos2/HardwareAddresses.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_debug_output.o: ../../../../Tests/Kernel/PowerPC/TestDebugOutput.cpp ../../../../Kernel/Arch/DebugOutput.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/current_time.o: ../CurrentTime.cpp ../../../../Kernel/Arch/CurrentTime.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/test_current_time.o: ../../../../Tests/Kernel/PowerPC/TestCurrentTime.cpp ../../../../Kernel/Arch/CurrentTime.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/kernel_context.o: ../KernelContext.S ../KernelContextOffsets.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_kernel_context.o: ../../../../Tests/Kernel/PowerPC/TestKernelContext.cpp ../KernelContext.h ../KernelContextOffsets.h ../ThreadRegisters.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/kernel_exception_return.o: ../KernelExceptionReturn.S ../KernelContextOffsets.h ../HighSegmentSwitch.inc | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_kernel_exception_return.o: ../../../../Tests/Kernel/PowerPC/TestKernelExceptionReturn.S ../KernelContextOffsets.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/initial_thread_context.o: ../InitialThreadContext.cpp ../InitialThreadContext.h ../RegisterState.h ../ThreadRegisters.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/initial_thread_entry.o: ../InitialThreadEntry.S ../KernelContextOffsets.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/test_initial_thread_context.o: ../../../../Tests/Kernel/PowerPC/TestInitialThreadContext.cpp ../InitialThreadContext.h ../RegisterState.h ../ThreadRegisters.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -I../../../../Userland/Libraries/LibC -Wno-literal-suffix -c $< -o $@

$(BUILD_DIR)/runtime.o: Runtime.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/runtime_assertions.o: RuntimeAssertions.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/serenity-powerpc-boot.elf: $(BUILD_DIR)/boot.o $(BUILD_DIR)/firmware.o $(BUILD_DIR)/exception_probe.o $(BUILD_DIR)/exception_probe_vector.o $(BUILD_DIR)/user_mode_probe.o $(BUILD_DIR)/mmu_probe.o $(BUILD_DIR)/mmu_probe_asm.o $(BUILD_DIR)/hash_page_table.o $(BUILD_DIR)/hpt_probe.o $(SEGMENT_REGISTER_OBJECTS) $(PAGE_DIRECTORY_BACKEND_OBJECTS) $(PHYSICAL_PAGE_ARRAY_OBJECTS) $(PAGE_MAPPING_TEST_OBJECTS) $(VSID_ALLOCATOR_OBJECTS) $(FIRMWARE_MEMORY_MAP_TEST_OBJECTS) $(BOOT_INFO_TEST_OBJECTS) $(BUILD_DIR)/abi.o $(BUILD_DIR)/setjmp.o $(BUILD_DIR)/test_setjmp.o $(BUILD_DIR)/test_setjmp_registers.o $(BUILD_DIR)/test_short_string_layout.o $(BUILD_DIR)/test_bitmap_endian.o $(BUILD_DIR)/test_mbr_wire_format.o $(BUILD_DIR)/test_ext2_wire_format.o $(BUILD_DIR)/test_float_digit_packing.o $(BUILD_DIR)/test_float_extractor_endian.o $(BUILD_DIR)/float_parser.o $(BUILD_DIR)/test_float_parser.o $(BUILD_DIR)/elf_validation.o $(BUILD_DIR)/elf_machine.o $(BUILD_DIR)/test_elf_header.o $(BUILD_DIR)/elf_image.o $(BUILD_DIR)/test_elf_image.o $(BUILD_DIR)/elf_relocation.o $(BUILD_DIR)/test_relative_relocations.o $(BUILD_DIR)/elf_tls.o $(BUILD_DIR)/test_tls_layout.o $(BUILD_DIR)/elf_entry.o $(BUILD_DIR)/test_elf_entry.o $(TLS_TEST_OBJECTS) $(PLT_TEST_OBJECTS) $(SYSCALL_TEST_OBJECTS) $(FENV_TEST_OBJECTS) $(FPU_STATE_TEST_OBJECTS) $(CPU_TEST_OBJECTS) $(PROCESSOR_TEST_OBJECTS) $(MEMORY_FENCES_TEST_OBJECTS) $(CACHE_TEST_OBJECTS) $(TLB_TEST_OBJECTS) $(REGISTER_TEST_OBJECTS) $(STACK_TEST_OBJECTS) $(TIMER_TEST_OBJECTS) $(PIC_TEST_OBJECTS) $(EXTERNAL_IRQ_TEST_OBJECTS) $(PCI_TEST_OBJECTS) $(PEGASOS2_PCI_CONFIG_TEST_OBJECTS) $(NVME_TEST_OBJECTS) $(SD_TEST_OBJECTS) $(KEY_EVENT_TEST_OBJECTS) $(SIGNAL_TEST_OBJECTS) $(SMAP_TEST_OBJECTS) $(SAFEMEM_TEST_OBJECTS) $(ATOMIC64_TEST_OBJECTS) $(DEBUG_OUTPUT_TEST_OBJECTS) $(CURRENT_TIME_TEST_OBJECTS) $(KERNEL_CONTEXT_TEST_OBJECTS) $(INITIAL_THREAD_CONTEXT_TEST_OBJECTS) $(EXCEPTION_VECTOR_TEST_OBJECTS) $(BUILD_DIR)/runtime.o $(BUILD_DIR)/runtime_assertions.o linker.ld Makefile
	$(LD) $(LDFLAGS) $(BUILD_DIR)/boot.o $(BUILD_DIR)/firmware.o $(BUILD_DIR)/exception_probe.o $(BUILD_DIR)/exception_probe_vector.o $(BUILD_DIR)/user_mode_probe.o $(BUILD_DIR)/mmu_probe.o $(BUILD_DIR)/mmu_probe_asm.o $(BUILD_DIR)/hash_page_table.o $(BUILD_DIR)/hpt_probe.o $(SEGMENT_REGISTER_OBJECTS) $(PAGE_DIRECTORY_BACKEND_OBJECTS) $(PHYSICAL_PAGE_ARRAY_OBJECTS) $(PAGE_MAPPING_TEST_OBJECTS) $(VSID_ALLOCATOR_OBJECTS) $(FIRMWARE_MEMORY_MAP_TEST_OBJECTS) $(BOOT_INFO_TEST_OBJECTS) $(BUILD_DIR)/abi.o $(BUILD_DIR)/setjmp.o $(BUILD_DIR)/test_setjmp.o $(BUILD_DIR)/test_setjmp_registers.o $(BUILD_DIR)/test_short_string_layout.o $(BUILD_DIR)/test_bitmap_endian.o $(BUILD_DIR)/test_mbr_wire_format.o $(BUILD_DIR)/test_ext2_wire_format.o $(BUILD_DIR)/test_float_digit_packing.o $(BUILD_DIR)/test_float_extractor_endian.o $(BUILD_DIR)/float_parser.o $(BUILD_DIR)/test_float_parser.o $(BUILD_DIR)/elf_validation.o $(BUILD_DIR)/elf_machine.o $(BUILD_DIR)/test_elf_header.o $(BUILD_DIR)/elf_image.o $(BUILD_DIR)/test_elf_image.o $(BUILD_DIR)/elf_relocation.o $(BUILD_DIR)/test_relative_relocations.o $(BUILD_DIR)/elf_tls.o $(BUILD_DIR)/test_tls_layout.o $(BUILD_DIR)/elf_entry.o $(BUILD_DIR)/test_elf_entry.o $(TLS_TEST_OBJECTS) $(PLT_TEST_OBJECTS) $(SYSCALL_TEST_OBJECTS) $(FENV_TEST_OBJECTS) $(FPU_STATE_TEST_OBJECTS) $(CPU_TEST_OBJECTS) $(PROCESSOR_TEST_OBJECTS) $(MEMORY_FENCES_TEST_OBJECTS) $(CACHE_TEST_OBJECTS) $(TLB_TEST_OBJECTS) $(REGISTER_TEST_OBJECTS) $(STACK_TEST_OBJECTS) $(TIMER_TEST_OBJECTS) $(PIC_TEST_OBJECTS) $(EXTERNAL_IRQ_TEST_OBJECTS) $(PCI_TEST_OBJECTS) $(PEGASOS2_PCI_CONFIG_TEST_OBJECTS) $(NVME_TEST_OBJECTS) $(SD_TEST_OBJECTS) $(KEY_EVENT_TEST_OBJECTS) $(SIGNAL_TEST_OBJECTS) $(SMAP_TEST_OBJECTS) $(SAFEMEM_TEST_OBJECTS) $(ATOMIC64_TEST_OBJECTS) $(DEBUG_OUTPUT_TEST_OBJECTS) $(CURRENT_TIME_TEST_OBJECTS) $(KERNEL_CONTEXT_TEST_OBJECTS) $(INITIAL_THREAD_CONTEXT_TEST_OBJECTS) $(EXCEPTION_VECTOR_TEST_OBJECTS) $(BUILD_DIR)/runtime.o $(BUILD_DIR)/runtime_assertions.o $(LIBGCC) -o $@

check-quick: all
	$(MAKE) -C ../../../../Tests/Toolchain/PowerPC check-quick CROSS_COMPILE=$(CROSS_COMPILE)
	$(MAKE) -C ../../../../Tests/Kernel/PCI check-quick
	$(MAKE) -C ../../../../Tests/Kernel/NVMe check-quick
	$(MAKE) -C ../../../../Tests/Kernel/SD check-quick
	$(MAKE) -C ../../../../Tests/Kernel/PowerPC check-quick CROSS_COMPILE=$(CROSS_COMPILE)
	python3 ../../../../Tests/Kernel/PowerPC/test_boot.py --qemu $(QEMU) --kernel $(BUILD_DIR)/serenity-powerpc-boot.elf --memory 512
	python3 ../../../../Tests/Kernel/PowerPC/test_rtas_power_off.py --qemu $(QEMU) --kernel $(BUILD_DIR)/serenity-powerpc-boot.elf

check: check-quick
	python3 ../../../../Tests/Kernel/PowerPC/test_boot.py --qemu $(QEMU) --kernel $(BUILD_DIR)/serenity-powerpc-boot.elf --memory 512 --cpu 750cxe

clean:
	rm -rf $(BUILD_DIR)
