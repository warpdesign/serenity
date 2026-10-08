# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

foreach(architecture IN ITEMS powerpc x86_64 aarch64)
    set(SERENITY_ARCH "${architecture}")
    set(SERENITY_BUILD_DIR "${TEST_BINARY_DIR}/${architecture}")
    set(configured_toolchain "${TEST_BINARY_DIR}/GNUToolchain-${architecture}.cmake")
    configure_file("${SERENITY_SOURCE_DIR}/Toolchain/CMake/GNUToolchain.txt.in" "${configured_toolchain}" @ONLY)
    include("${configured_toolchain}")

    if (architecture STREQUAL "powerpc")
        set(expected_pointer_size 4)
        set(expected_byte_order BIG_ENDIAN)
    else()
        set(expected_pointer_size 8)
        set(expected_byte_order LITTLE_ENDIAN)
    endif()

    if (NOT CMAKE_C_SIZEOF_DATA_PTR EQUAL expected_pointer_size OR NOT CMAKE_CXX_SIZEOF_DATA_PTR EQUAL expected_pointer_size)
        message(FATAL_ERROR "${architecture}: wrong C/C++ pointer sizes: ${CMAKE_C_SIZEOF_DATA_PTR}/${CMAKE_CXX_SIZEOF_DATA_PTR}")
    endif()
    if (NOT CMAKE_C_BYTE_ORDER STREQUAL expected_byte_order OR NOT CMAKE_CXX_BYTE_ORDER STREQUAL expected_byte_order)
        message(FATAL_ERROR "${architecture}: wrong C/C++ byte order: ${CMAKE_C_BYTE_ORDER}/${CMAKE_CXX_BYTE_ORDER}")
    endif()
endforeach()
