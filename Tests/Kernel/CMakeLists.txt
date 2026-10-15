set(TEST_SOURCES
    bind-local-socket-to-symlink.cpp
    crash-fcntl-invalid-cmd.cpp
    elf-execve-mmap-race.cpp
    fuzz-syscalls.cpp
    kill-pidtid-confusion.cpp
    mmap-write-into-running-programs-executable-file.cpp
    mprotect-multi-region-mprotect.cpp
    munmap-multi-region-unmapping.cpp
    nanosleep-race-outbuf-munmap.cpp
    null-deref-close-during-select.cpp
    null-deref-crash-during-pthread_join.cpp
    path-resolution-race.cpp
    setpgid-across-sessions-without-leader.cpp
    siginfo-example.cpp
    stress-truncate.cpp
    stress-writeread.cpp
    uaf-close-while-blocked-in-read.cpp
    unveil-symlinks.cpp
)

if (NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    list(APPEND TEST_SOURCES elf-symbolication-kernel-read-exploit.cpp)
endif()

# FIXME: These tests do not use LibTest
foreach(source IN LISTS TEST_SOURCES)
    get_filename_component(test_name "${source}" NAME_WE)
    add_executable("${test_name}" "${source}")
    target_link_libraries("${test_name}" PRIVATE LibCore)
    serenity_set_implicit_links("${test_name}")
    install(TARGETS "${test_name}" RUNTIME DESTINATION usr/Tests/Kernel/Legacy)
endforeach()

target_link_libraries(fuzz-syscalls PRIVATE LibSystem)

serenity_test("crash.cpp" Kernel MAIN_ALREADY_DEFINED)

set(LIBTEST_BASED_SOURCES
    TestEFault.cpp
    TestEmptyPrivateInodeVMObject.cpp
    TestEmptySharedInodeVMObject.cpp
    TestExt2FS.cpp
    TestFileSystemDirentTypes.cpp
    TestInvalidUIDSet.cpp
    TestSFNUtilities.cpp
    TestSharedInodeVMObject.cpp
    TestPageFaultRace.cpp
    TestPosixFallocate.cpp
    TestPosixSpawn.cpp
    TestPrivateInodeVMObject.cpp
    TestPtrace.cpp
    TestKernelAlarm.cpp
    TestKernelFilePermissions.cpp
    TestKernelPledge.cpp
    TestKernelUnveil.cpp
    TestLoopDevice.cpp
    TestMunMap.cpp
    TestProcFS.cpp
    TestProcFSWrite.cpp
    TestSigAltStack.cpp
    TestSigHandler.cpp
    TestSignalDispatch.cpp
    TestSymlink.cpp
    TestTCPSocket.cpp
    TestUDPSocket.cpp
    TestWXProtection.cpp
)

if (ENABLE_KERNEL_COVERAGE_COLLECTION)
    list(APPEND LIBTEST_BASED_SOURCES TestKCOV.cpp)
endif()

if (NOT ENABLE_USERSPACE_COVERAGE_COLLECTION)
    # FIXME: Make these tests generate valid coverage files.
    list(APPEND LIBTEST_BASED_SOURCES
        TestAnonymousMmap.cpp
        TestSigWait.cpp
    )
endif()

if (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    list(APPEND LIBTEST_BASED_SOURCES TestMemoryDeviceMmap.cpp)
endif()

foreach(libtest_source IN LISTS LIBTEST_BASED_SOURCES)
    serenity_test("${libtest_source}" Kernel LIBS LibSystem)
endforeach()

target_link_libraries(TestPageFaultRace PRIVATE LibThreading)

add_subdirectory(POSIX)

if (SERENITY_ARCH STREQUAL "powerpc")
    find_program(POWERPC_HOST_CXX NAMES c++ clang++ REQUIRED)
    set(powerpc_uhci_interrupt_source "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/TestUHCIInterruptTransfer.cpp")
    set(powerpc_uhci_interrupt_executable "${CMAKE_CURRENT_BINARY_DIR}/PowerPC-UHCI-interrupt-transfer")
    add_custom_target(PowerPCUHCIInterruptTransferCheckQuick
        COMMAND "${POWERPC_HOST_CXX}" -std=c++23 -I "${CMAKE_SOURCE_DIR}"
            "${powerpc_uhci_interrupt_source}" -o "${powerpc_uhci_interrupt_executable}"
        COMMAND "${powerpc_uhci_interrupt_executable}"
        VERBATIM
    )
    add_custom_target(PowerPCUHCIInterruptTransferCheck
        COMMAND "${powerpc_uhci_interrupt_executable}" extended
        DEPENDS PowerPCUHCIInterruptTransferCheckQuick
        VERBATIM
    )
    set(powerpc_loader_boot_arguments_source "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/TestLoaderBootArguments.cpp")
    set(powerpc_loader_boot_arguments_executable "${CMAKE_CURRENT_BINARY_DIR}/PowerPC-loader-boot-arguments")
    add_custom_target(PowerPCLoaderBootArgumentsCheckQuick
        COMMAND "${POWERPC_HOST_CXX}" -std=c++23 -I "${CMAKE_SOURCE_DIR}"
            "${powerpc_loader_boot_arguments_source}" -o "${powerpc_loader_boot_arguments_executable}"
        COMMAND "${powerpc_loader_boot_arguments_executable}"
        VERBATIM
    )
    add_custom_target(PowerPCLoaderBootArgumentsCheck
        COMMAND "${powerpc_loader_boot_arguments_executable}"
        DEPENDS PowerPCLoaderBootArgumentsCheckQuick
        VERBATIM
    )
    set(powerpc_rtas_contract_source "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/TestRTASPCIContract.cpp")
    set(powerpc_rtas_contract_executable "${CMAKE_CURRENT_BINARY_DIR}/PowerPC-RTAS-PCI-contract")
    add_custom_target(PowerPCRtasPCIContractCheckQuick
        COMMAND "${POWERPC_HOST_CXX}" -std=c++23 -I "${CMAKE_SOURCE_DIR}"
            "${powerpc_rtas_contract_source}" -o "${powerpc_rtas_contract_executable}"
        COMMAND "${powerpc_rtas_contract_executable}"
        VERBATIM
    )
    add_custom_target(PowerPCRtasPCIContractCheck
        COMMAND "${powerpc_rtas_contract_executable}" extended
        DEPENDS PowerPCRtasPCIContractCheckQuick
        VERBATIM
    )
    set(powerpc_via_codec_contract_source "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/TestVIAAC97CodecContract.cpp")
    set(powerpc_via_codec_contract_executable "${CMAKE_CURRENT_BINARY_DIR}/PowerPC-VIA-AC97-codec-contract")
    add_custom_target(PowerPCVIAAC97CodecContractCheckQuick
        COMMAND "${POWERPC_HOST_CXX}" -std=c++23 -I "${CMAKE_SOURCE_DIR}"
            "${powerpc_via_codec_contract_source}" -o "${powerpc_via_codec_contract_executable}"
        COMMAND "${powerpc_via_codec_contract_executable}"
        VERBATIM
    )
    add_custom_target(PowerPCVIAAC97CodecContractCheck
        COMMAND "${powerpc_via_codec_contract_executable}" extended
        DEPENDS PowerPCVIAAC97CodecContractCheckQuick
        VERBATIM
    )
    set(powerpc_radeon_budget_source "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/TestRadeonFramebufferBudget.cpp")
    set(powerpc_radeon_budget_executable "${CMAKE_CURRENT_BINARY_DIR}/PowerPC-Radeon-framebuffer-budget")
    add_custom_target(PowerPCRadeonFramebufferBudgetCheckQuick
        COMMAND "${POWERPC_HOST_CXX}" -std=c++23 -I "${CMAKE_SOURCE_DIR}"
            "${powerpc_radeon_budget_source}" -o "${powerpc_radeon_budget_executable}"
        COMMAND "${powerpc_radeon_budget_executable}"
        VERBATIM
    )
    add_custom_target(PowerPCRadeonFramebufferBudgetCheck
        COMMAND "${powerpc_radeon_budget_executable}" extended
        DEPENDS PowerPCRadeonFramebufferBudgetCheckQuick
        VERBATIM
    )
    set(powerpc_radeon_aperture_endian_source "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/TestRadeonApertureEndian.cpp")
    set(powerpc_radeon_aperture_endian_executable "${CMAKE_CURRENT_BINARY_DIR}/PowerPC-Radeon-aperture-endian")
    add_custom_target(PowerPCRadeonApertureEndianCheckQuick
        COMMAND "${POWERPC_HOST_CXX}" -std=c++23 -I "${CMAKE_SOURCE_DIR}"
            "${powerpc_radeon_aperture_endian_source}" -o "${powerpc_radeon_aperture_endian_executable}"
        COMMAND "${powerpc_radeon_aperture_endian_executable}"
        VERBATIM
    )
    add_custom_target(PowerPCRadeonApertureEndianCheck
        COMMAND "${powerpc_radeon_aperture_endian_executable}" extended
        DEPENDS PowerPCRadeonApertureEndianCheckQuick
        VERBATIM
    )
    set(powerpc_native_pci_boot_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_native_pci_boot.py")
    add_custom_target(PowerPCNativePCIHardwareCheckQuick
        COMMAND python3 "${powerpc_native_pci_boot_test}"
        VERBATIM
    )
    add_custom_target(PowerPCNativePCIHardwareCheck
        COMMAND python3 "${powerpc_native_pci_boot_test}" --normal
        DEPENDS PowerPCNativePCIHardwareCheckQuick
        VERBATIM
    )
    add_custom_target(PowerPCNativeRTASHardwareCheckQuick
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-rtas
        VERBATIM
    )
    add_custom_target(PowerPCNativeRTASHardwareCheck
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-rtas --normal
        DEPENDS PowerPCNativeRTASHardwareCheckQuick
        VERBATIM
    )
    add_custom_target(PowerPCNativeUHCIBootCheckQuick
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-uhci-progress
        VERBATIM
    )
    add_custom_target(PowerPCNativeUHCIBootCheck
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-uhci-progress --normal
        DEPENDS PowerPCNativeUHCIBootCheckQuick
        VERBATIM
    )
    set(powerpc_native_usb_keyboard_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_native_usb_keyboard.py")
    add_custom_target(PowerPCNativeUSBKeyboardCheckQuick
        COMMAND python3 "${powerpc_native_usb_keyboard_test}"
        VERBATIM
    )
    add_custom_target(PowerPCNativeUSBKeyboardCheck
        COMMAND python3 "${powerpc_native_usb_keyboard_test}" --normal
        DEPENDS PowerPCNativeUSBKeyboardCheckQuick
        VERBATIM
    )
    add_custom_target(PowerPCNativeEntropyBootCheckQuick
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-entropy-progress
        VERBATIM
    )
    add_custom_target(PowerPCNativeEntropyBootCheck
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-entropy-progress --normal
        DEPENDS PowerPCNativeEntropyBootCheckQuick
        VERBATIM
    )
    add_custom_target(PowerPCNativePS2MouseCheckQuick
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-ps2-mouse
        VERBATIM
    )
    add_custom_target(PowerPCNativePS2MouseCheck
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-ps2-mouse --normal
        DEPENDS PowerPCNativePS2MouseCheckQuick
        VERBATIM
    )
    add_custom_target(PowerPCNativeVIAAudioCheckQuick
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-via-audio
        VERBATIM
    )
    add_custom_target(PowerPCNativeVIAAudioCheck
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-via-audio --normal
        DEPENDS PowerPCNativeVIAAudioCheckQuick
        VERBATIM
    )
    add_custom_target(PowerPCNativeDesktopMemoryCheckQuick
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-desktop-memory
        VERBATIM
    )
    add_custom_target(PowerPCNativeDesktopMemoryCheck
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-desktop-memory --normal
        DEPENDS PowerPCNativeDesktopMemoryCheckQuick
        VERBATIM
    )
    add_custom_target(PowerPCNativeInstalledMemoryCheckQuick
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-installed-memory
        VERBATIM
    )
    add_custom_target(PowerPCNativeInstalledMemoryCheck
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-installed-memory --normal
        DEPENDS PowerPCNativeInstalledMemoryCheckQuick
        VERBATIM
    )
    add_custom_target(PowerPCNativeRadeonEndianCheckQuick
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-radeon-endian
        VERBATIM
    )
    add_custom_target(PowerPCNativeRadeonEndianCheck
        COMMAND python3 "${powerpc_native_pci_boot_test}" --require-radeon-endian --normal
        DEPENDS PowerPCNativeRadeonEndianCheckQuick
        VERBATIM
    )

    string(REGEX REPLACE "gcc$" "" powerpc_test_cross_compile "${CMAKE_C_COMPILER}")
    set(powerpc_kmalloc_hpt_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_kernel_entry.py")
    set(powerpc_kmalloc_hpt_arguments
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --check-heap-hpt-alias
    )
    add_custom_target(PowerPCKmallocHPTCheckQuick
        COMMAND python3 "${powerpc_kmalloc_hpt_test}" ${powerpc_kmalloc_hpt_arguments} --cpu 750
        DEPENDS PowerPCFirmwareLoader
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCKmallocHPTCheck
        COMMAND python3 "${powerpc_kmalloc_hpt_test}" ${powerpc_kmalloc_hpt_arguments} --cpu 750cxe
        DEPENDS PowerPCKmallocHPTCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_hpt_refill_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_hpt_refill.py")
    set(powerpc_hpt_refill_arguments
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --cross-compile "${powerpc_test_cross_compile}"
    )
    add_custom_target(PowerPCHPTRefillCheckQuick
        COMMAND python3 "${powerpc_hpt_refill_test}" ${powerpc_hpt_refill_arguments} --cpu 750
        DEPENDS PowerPCFirmwareLoader
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCHPTRefillCheck
        COMMAND python3 "${powerpc_hpt_refill_test}" ${powerpc_hpt_refill_arguments} --cpu 750cxe
        DEPENDS PowerPCHPTRefillCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_idle_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_idle_wakeup.py")
    set(powerpc_idle_arguments
        --compiler "${CMAKE_C_COMPILER}" --qemu qemu-system-ppc
        --kernel-loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --libc "$<TARGET_FILE:LibC>" --libsystem "$<TARGET_FILE:LibSystem>"
    )
    add_custom_target(PowerPCIdleCheckQuick
        COMMAND python3 "${powerpc_idle_test}" ${powerpc_idle_arguments} --cpu 750
        DEPENDS PowerPCFirmwareLoader Loader.so LibC LibSystem
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCIdleCheck
        COMMAND python3 "${powerpc_idle_test}" ${powerpc_idle_arguments} --memory 512 --cpu 750cxe
        DEPENDS PowerPCIdleCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_block_latency_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_virtio_block_latency.py")
    set(powerpc_block_latency_arguments
        --compiler "${CMAKE_C_COMPILER}" --qemu qemu-system-ppc
        --kernel-loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --libc "$<TARGET_FILE:LibC>" --libsystem "$<TARGET_FILE:LibSystem>"
    )
    add_custom_target(PowerPCVirtIOBlockLatencyCheckQuick
        COMMAND python3 "${powerpc_block_latency_test}" ${powerpc_block_latency_arguments} --cpu 750
        DEPENDS PowerPCFirmwareLoader Loader.so LibC LibSystem
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCVirtIOBlockLatencyCheck
        COMMAND python3 "${powerpc_block_latency_test}" ${powerpc_block_latency_arguments} --cpu 750cxe
        DEPENDS PowerPCVirtIOBlockLatencyCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_signal_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_signal_stack_alignment.py")
    set(powerpc_signal_arguments
        --compiler "${CMAKE_C_COMPILER}" --qemu qemu-system-ppc
        --kernel-loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --libc "$<TARGET_FILE:LibC>" --libsystem "$<TARGET_FILE:LibSystem>"
    )
    add_custom_target(PowerPCSignalCheckQuick
        COMMAND python3 "${powerpc_signal_test}" ${powerpc_signal_arguments}
        DEPENDS PowerPCFirmwareLoader Loader.so LibC LibSystem
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCSignalCheck
        COMMAND python3 "${powerpc_signal_test}" ${powerpc_signal_arguments} --memory 512 --cpu 750cxe
        DEPENDS PowerPCSignalCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(PowerPCWindowServerCheckQuick
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_window_server.py"
            --qemu qemu-system-ppc
            --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
            --user-loader "$<TARGET_FILE:Loader.so>"
            --build-dir "${CMAKE_BINARY_DIR}"
            --cpu 750
            --omit-fonts
        DEPENDS PowerPCFirmwareLoaderCheckQuick init mount DeviceMapper SystemServer WindowServer keymap Loader.so
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCWindowServerCheck
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_window_server.py"
            --qemu qemu-system-ppc
            --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
            --user-loader "$<TARGET_FILE:Loader.so>"
            --build-dir "${CMAKE_BINARY_DIR}"
        DEPENDS PowerPCWindowServerCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    add_custom_target(PowerPCVirtIOGPUFootprintCheckQuick
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_virtio_gpu_footprint.py"
            --qemu qemu-system-ppc
            --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
            --user-loader "$<TARGET_FILE:Loader.so>"
            --build-dir "${CMAKE_BINARY_DIR}"
            --cpu 750
        DEPENDS PowerPCFirmwareLoader init mount DeviceMapper SystemServer WindowServer keymap Loader.so
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCVirtIOGPUFootprintCheck
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_virtio_gpu_footprint.py"
            --qemu qemu-system-ppc
            --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
            --user-loader "$<TARGET_FILE:Loader.so>"
            --build-dir "${CMAKE_BINARY_DIR}"
            --cpu 750cxe
        DEPENDS PowerPCVirtIOGPUFootprintCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    get_directory_property(powerpc_desktop_utility_names
        DIRECTORY "${CMAKE_SOURCE_DIR}/Userland/Utilities" DEFINITION REQUIRED_TARGETS)
    if(NOT powerpc_desktop_utility_names)
        message(FATAL_ERROR "PowerPC desktop test cannot find required utility components")
    endif()
    list(TRANSFORM powerpc_desktop_utility_names PREPEND "Component")

    add_custom_target(PowerPCDesktopCheckQuick
        DEPENDS PowerPCWindowServerCheckQuick Clipboard ConfigServer ImageDecoder LaunchServer LoginServer Taskbar FileManager
            FileSystemAccessServer RequestServer WebContent
            ComponentAbout ComponentCalendar ComponentCalendarSettings ComponentClockSettings ComponentRun ComponentSettings ComponentTextEditor
            Componentls Componentlscpu Componenttimezone Componentuname Terminal TerminalSettings Shell FileOperation utmpupdate
            SystemMonitor Escalator NetworkSettings lspci lsusb memstat membench disk_benchmark lsblk lsdev lsirq
    )
    add_custom_target(PowerPCDesktopCheck
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_desktop.py"
            --qemu qemu-system-ppc
            --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
            --user-loader "$<TARGET_FILE:Loader.so>"
            --build-dir "${CMAKE_BINARY_DIR}"
        DEPENDS PowerPCDesktopCheckQuick ${powerpc_desktop_utility_names}
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_ps2_input_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_ps2_input.py")
    set(powerpc_ps2_input_arguments
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --build-dir "${CMAKE_BINARY_DIR}"
    )
    add_custom_target(PowerPCPS2InputCheckQuick
        COMMAND python3 "${powerpc_ps2_input_test}" ${powerpc_ps2_input_arguments} --cpu 750
        DEPENDS PowerPCDesktopCheckQuick ${powerpc_desktop_utility_names}
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCPS2InputCheck
        COMMAND python3 "${powerpc_ps2_input_test}" ${powerpc_ps2_input_arguments} --cpu 750cxe
        DEPENDS PowerPCPS2InputCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_usb_input_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_usb_input.py")
    set(powerpc_usb_input_arguments
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --build-dir "${CMAKE_BINARY_DIR}"
    )
    add_custom_target(PowerPCUSBInputCheckQuick
        COMMAND python3 "${powerpc_usb_input_test}" ${powerpc_usb_input_arguments} --cpu 750
        DEPENDS PowerPCDesktopCheckQuick ${powerpc_desktop_utility_names}
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCUSBInputCheck
        COMMAND python3 "${powerpc_usb_input_test}" ${powerpc_usb_input_arguments} --cpu 750cxe
        DEPENDS PowerPCUSBInputCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_rdb_card_boot_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_rdb_card_boot.py")
    set(powerpc_rdb_card_boot_arguments
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --build-dir "${CMAKE_BINARY_DIR}"
    )
    add_custom_target(PowerPCRDBCardBootCheckQuick
        COMMAND python3 "${powerpc_rdb_card_boot_test}" ${powerpc_rdb_card_boot_arguments} --cpu 750
        DEPENDS PowerPCDesktopCheckQuick ${powerpc_desktop_utility_names}
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCRDBCardBootCheck
        COMMAND python3 "${powerpc_rdb_card_boot_test}" ${powerpc_rdb_card_boot_arguments} --cpu 750cxe
        DEPENDS PowerPCRDBCardBootCheckQuick
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCNativeCardBootCheckQuick
        COMMAND python3 "${powerpc_rdb_card_boot_test}" ${powerpc_rdb_card_boot_arguments} --cpu 750 --nativeboot
        DEPENDS PowerPCDesktopCheckQuick ${powerpc_desktop_utility_names}
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCNativeCardBootCheck
        COMMAND python3 "${powerpc_rdb_card_boot_test}" ${powerpc_rdb_card_boot_arguments} --cpu 750cxe --nativeboot
        DEPENDS PowerPCNativeCardBootCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_ide_boot_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_ide_boot.py")
    set(powerpc_ide_boot_arguments
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --build-dir "${CMAKE_BINARY_DIR}"
    )
    add_custom_target(PowerPCIDEBootCheckQuick
        COMMAND python3 "${powerpc_ide_boot_test}" ${powerpc_ide_boot_arguments} --cpu 750
        DEPENDS PowerPCDesktopCheckQuick ${powerpc_desktop_utility_names}
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCIDEBootCheck
        COMMAND python3 "${powerpc_ide_boot_test}" ${powerpc_ide_boot_arguments} --cpu 750cxe
        DEPENDS PowerPCIDEBootCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_radeon_desktop_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_radeon_desktop.py")
    set(powerpc_radeon_desktop_arguments
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --build-dir "${CMAKE_BINARY_DIR}"
    )
    add_custom_target(PowerPCRadeonDesktopCheckQuick
        COMMAND python3 "${powerpc_radeon_desktop_test}" ${powerpc_radeon_desktop_arguments} --cpu 750
        DEPENDS PowerPCDesktopCheckQuick ${powerpc_desktop_utility_names}
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCRadeonDesktopCheck
        COMMAND python3 "${powerpc_radeon_desktop_test}" ${powerpc_radeon_desktop_arguments} --cpu 750cxe
        DEPENDS PowerPCRadeonDesktopCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_pixel_paint_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_pixel_paint.py")
    set(powerpc_pixel_paint_arguments
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --build-dir "${CMAKE_BINARY_DIR}"
    )
    add_custom_target(PowerPCPixelPaintCheckQuick
        COMMAND python3 "${powerpc_pixel_paint_test}" ${powerpc_pixel_paint_arguments} --cpu 750
        DEPENDS PowerPCDesktopCheckQuick PixelPaint FileSystemAccessServer
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCPixelPaintCheck
        COMMAND python3 "${powerpc_pixel_paint_test}" ${powerpc_pixel_paint_arguments} --cpu 750cxe
        DEPENDS PowerPCPixelPaintCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_via_audio_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_via_audio.py")
    set(powerpc_via_audio_arguments
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --build-dir "${CMAKE_BINARY_DIR}"
    )
    add_custom_target(PowerPCVIAAudioCheckQuick
        COMMAND python3 "${powerpc_via_audio_test}" ${powerpc_via_audio_arguments} --cpu 750
        DEPENDS PowerPCDesktopCheckQuick SoundPlayer AudioServer
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCVIAAudioCheck
        COMMAND python3 "${powerpc_via_audio_test}" ${powerpc_via_audio_arguments} --cpu 750cxe
        DEPENDS PowerPCVIAAudioCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_desktop_programs_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_desktop_programs.py")
    set(powerpc_desktop_programs_arguments
        --compiler "${CMAKE_C_COMPILER}"
        --cxx-compiler "${CMAKE_CXX_COMPILER}"
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --libc "$<TARGET_FILE:LibC>" --libsystem "$<TARGET_FILE:LibSystem>"
        --build-dir "${CMAKE_BINARY_DIR}"
    )
    add_custom_target(PowerPCDesktopProgramsCheckQuick
        COMMAND python3 "${powerpc_desktop_programs_test}" ${powerpc_desktop_programs_arguments} --cpu 750 --quick
        DEPENDS PowerPCDesktopCheckQuick ${powerpc_desktop_utility_names} LibCrypto
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCDesktopProgramsCheck
        COMMAND python3 "${powerpc_desktop_programs_test}" ${powerpc_desktop_programs_arguments} --cpu 750cxe
        DEPENDS PowerPCDesktopProgramsCheckQuick
        USES_TERMINAL
        VERBATIM
    )

    set(powerpc_browser_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_browser.py")
    set(powerpc_browser_arguments
        --qemu qemu-system-ppc
        --loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --build-dir "${CMAKE_BINARY_DIR}"
        --timeout 120
    )
    add_custom_target(PowerPCBrowserCheckQuick
        COMMAND python3 "${powerpc_browser_test}" ${powerpc_browser_arguments} --cpu 750
        DEPENDS PowerPCDesktopProgramsCheckQuick ComponentBrowser ComponentSQLServer ComponentFileSystemAccessServer
            ComponentWebWorker ComponentAudioServer ComponentLookupServer ComponentNetworkServer ComponentDHCPClient
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCBrowserCheck
        COMMAND python3 "${powerpc_browser_test}" ${powerpc_browser_arguments} --cpu 750cxe
        DEPENDS PowerPCBrowserCheckQuick
        USES_TERMINAL
        VERBATIM
    )
endif()
