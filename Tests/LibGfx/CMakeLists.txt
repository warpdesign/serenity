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
    TestRect.cpp
    TestScalingFunctions.cpp
    TestWOFF.cpp
    TestWOFF2.cpp
)

foreach(source IN LISTS TEST_SOURCES)
    serenity_test("${source}" LibGfx LIBS LibGfx)
endforeach()

if (NOT CMAKE_CROSSCOMPILING)
    add_custom_target(LibGfxCheckQuick
        COMMAND "$<TARGET_FILE:TestJPEGXLHalfFloat>"
        COMMAND "$<TARGET_FILE:TestFontHandling>"
        DEPENDS TestJPEGXLHalfFloat TestFontHandling
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

install(DIRECTORY test-inputs DESTINATION usr/Tests/LibGfx)
