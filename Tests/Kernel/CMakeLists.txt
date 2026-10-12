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
            ComponentAbout ComponentCalendar ComponentClockSettings ComponentRun ComponentSettings Componentls Componentlscpu Componenttimezone Componentuname
            Terminal TerminalSettings Shell FileOperation utmpupdate
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
