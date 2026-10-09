# Copyright (c) 2026, SerenityOS developers.
#
# SPDX-License-Identifier: BSD-2-Clause

cmake_minimum_required(VERSION 3.22)
project(PowerPCDataModel LANGUAGES C CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4 OR NOT CMAKE_C_SIZEOF_DATA_PTR EQUAL 4 OR NOT CMAKE_CXX_SIZEOF_DATA_PTR EQUAL 4)
    message(FATAL_ERROR "PowerPC CMake pointer sizes are ${CMAKE_SIZEOF_VOID_P}/${CMAKE_C_SIZEOF_DATA_PTR}/${CMAKE_CXX_SIZEOF_DATA_PTR}, expected 4")
endif()
if(NOT CMAKE_C_BYTE_ORDER STREQUAL "BIG_ENDIAN" OR NOT CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
    message(FATAL_ERROR "PowerPC CMake byte orders are ${CMAKE_C_BYTE_ORDER}/${CMAKE_CXX_BYTE_ORDER}, expected BIG_ENDIAN")
endif()
if(NOT CMAKE_C_FLAGS MATCHES "(^| )-msecure-plt( |$)" OR NOT CMAKE_CXX_FLAGS MATCHES "(^| )-msecure-plt( |$)")
    message(FATAL_ERROR "PowerPC C and C++ must use the secure PLT ABI")
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../../../Kernel/Arch/powerpc/Boot" "${CMAKE_CURRENT_BINARY_DIR}/BootProbe")
