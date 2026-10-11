set(TEST_SOURCES
    BenchmarkGfxPainter.cpp
    BenchmarkJPEGLoader.cpp
    BenchmarkPNG.cpp
    TestBilevelImage.cpp
    TestCCITT.cpp
    TestColor.cpp
    TestDeltaE.cpp
    TestFontHandling.cpp
    TestGfxBitmap.cpp
    TestICCProfile.cpp
    TestImageDecoder.cpp
    TestImageWriter.cpp
    TestJPEGXLHalfFloat.cpp
    TestMedianCut.cpp
    TestPainter.cpp
    TestParseISOBMFF.cpp
    TestPath.cpp
    TestPNGEndian.cpp
    TestRect.cpp
    TestScalingFunctions.cpp
    TestWOFF.cpp
    TestWOFF2.cpp
)

foreach(source IN LISTS TEST_SOURCES)
    serenity_test("${source}" LibGfx LIBS LibGfx)
endforeach()
target_link_libraries(TestPNGEndian PRIVATE LibCompress)

if (NOT CMAKE_CROSSCOMPILING)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    add_custom_target(LibGfxCheckQuick
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_tiff_generator.py"
        COMMAND "$<TARGET_FILE:TestJPEGXLHalfFloat>"
        COMMAND "$<TARGET_FILE:TestFontHandling>"
        COMMAND "$<TARGET_FILE:TestPNGEndian>"
        DEPENDS TestJPEGXLHalfFloat TestFontHandling TestPNGEndian
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
    add_custom_target(LibGfxCheck
        COMMAND "$<TARGET_FILE:TestImageDecoder>"
        DEPENDS LibGfxCheckQuick TestImageDecoder
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
endif()

if (SERENITY_ARCH STREQUAL "powerpc")
    set(powerpc_png_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_png_runtime.py")
    set(powerpc_png_arguments
        --compiler "${CMAKE_CXX_COMPILER}"
        --qemu qemu-system-ppc
        --kernel-loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --library-root "${CMAKE_BINARY_DIR}/Userland/Libraries"
    )
    add_custom_target(PowerPCLibGfxCheckQuick
        COMMAND python3 "${powerpc_png_test}" ${powerpc_png_arguments}
        DEPENDS PowerPCFirmwareLoader Loader.so LibGfx LibC LibSystem
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCLibGfxCheck
        COMMAND python3 "${powerpc_png_test}" ${powerpc_png_arguments} --memory 512 --cpu 750cxe
        DEPENDS PowerPCLibGfxCheckQuick
        USES_TERMINAL
        VERBATIM
    )
endif()

install(DIRECTORY test-inputs DESTINATION usr/Tests/LibGfx)
