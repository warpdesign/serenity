set(SOURCES
    main.cpp
    misc.cpp
    arch/${SERENITY_ARCH}/entry.S
)

add_library(DynamicLoader_CompileOptions INTERFACE)
target_compile_definitions(DynamicLoader_CompileOptions INTERFACE NO_TLS _DYNAMIC_LOADER)
target_compile_options(DynamicLoader_CompileOptions INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti> -fpie -ffunction-sections -fdata-sections)
if (SERENITY_ARCH STREQUAL "powerpc")
    # PowerPC jump-table addresses in the early relocation code use GOT entries
    # that have not yet been relocated.
    target_compile_options(DynamicLoader_CompileOptions INTERFACE -fno-jump-tables)
    set(loader_libgcc_option -nodefaultlibs)
else()
    set(loader_libgcc_option -static-libgcc)
endif()
target_link_options(DynamicLoader_CompileOptions INTERFACE -nolibc -nostdlib++ -nostartfiles ${loader_libgcc_option} -fpie -Wl,--gc-sections)
target_link_options(DynamicLoader_CompileOptions INTERFACE -fno-sanitize=undefined) # Sanitizer runtime is linked in manually

add_executable(Loader.so ${SOURCES})

# Mixing C and C++ results in linker errors with GCC > 16 and CMake < 4.0.
# The issue is fixed upstream in https://gitlab.kitware.com/cmake/cmake/-/merge_requests/12027.
# FIXME: Remove this workaround once we require a CMake version >= 4.1.
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES "gcc_s_asneeded")

target_link_libraries(Loader.so PRIVATE DynamicLoader_CompileOptions DynamicLoader_LibELF DynamicLoader_LibCoreArgsParser)
if (ENABLE_UNDEFINED_SANITIZER)
    target_link_libraries(Loader.so PRIVATE DynamicLoader_LibUBSanitizer)
endif()
target_link_options(Loader.so PRIVATE LINKER:--no-dynamic-linker)
# Don't confuse the coverage results by instrumenting Loader
target_link_libraries(Loader.so PRIVATE NoCoverage)

install(TARGETS Loader.so RUNTIME DESTINATION usr/lib/)

if (SERENITY_ARCH STREQUAL "powerpc")
    set(powerpc_kernel_loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf")
    set(powerpc_loader_test "${SerenityOS_SOURCE_DIR}/Tests/DynamicLoader/PowerPC/test_bootstrap.py")
    set(powerpc_program_mapping_test "${SerenityOS_SOURCE_DIR}/Tests/DynamicLoader/PowerPC/test_program_mapping.py")
    set(powerpc_program_mapping_arguments
        --qemu qemu-system-ppc --kernel-loader "${powerpc_kernel_loader}"
        --user-loader "$<TARGET_FILE:Loader.so>" --program "$<TARGET_FILE:init>"
        --library "$<TARGET_FILE:LibC>" --library "$<TARGET_FILE:LibCore>"
        --library "$<TARGET_FILE:LibCoreBasic>" --library "$<TARGET_FILE:LibCoreMinimal>"
        --library "$<TARGET_FILE:LibCrypt>" --library "$<TARGET_FILE:LibURL>"
        --library "$<TARGET_FILE:LibUnicode>" --library "$<TARGET_FILE:LibTextCodec>"
        --library "$<TARGET_FILE:LibSystem>"
    )
    add_custom_target(PowerPCDynamicLoaderCheckQuick
        COMMAND python3 "${powerpc_loader_test}" --qemu qemu-system-ppc --kernel-loader "${powerpc_kernel_loader}" --user-loader "$<TARGET_FILE:Loader.so>"
        COMMAND python3 "${powerpc_program_mapping_test}" ${powerpc_program_mapping_arguments}
        DEPENDS PowerPCFirmwareLoader Loader.so init LibC
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCDynamicLoaderCheck
        COMMAND python3 "${powerpc_loader_test}" --qemu qemu-system-ppc --kernel-loader "${powerpc_kernel_loader}" --user-loader "$<TARGET_FILE:Loader.so>" --memory 512 --cpu 750cxe
        COMMAND python3 "${powerpc_program_mapping_test}" ${powerpc_program_mapping_arguments} --memory 512 --cpu 750cxe
        DEPENDS PowerPCDynamicLoaderCheckQuick
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCDynamicLoaderForkCheckQuick
        COMMAND python3 "${powerpc_program_mapping_test}" ${powerpc_program_mapping_arguments} --through-fork
        DEPENDS PowerPCFirmwareLoader Loader.so init LibC
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCDynamicLoaderForkCheck
        COMMAND python3 "${powerpc_program_mapping_test}" ${powerpc_program_mapping_arguments} --through-fork --memory 512 --cpu 750cxe
        DEPENDS PowerPCDynamicLoaderForkCheckQuick
        USES_TERMINAL
        VERBATIM
    )
endif()
