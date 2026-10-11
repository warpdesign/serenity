set(TEST_SOURCES
    TestBrotli.cpp
    TestDeflate.cpp
    TestGzip.cpp
    TestLzma.cpp
    TestLzw.cpp
    TestPackBits.cpp
    TestXz.cpp
    TestZlib.cpp
)

foreach(source IN LISTS TEST_SOURCES)
    serenity_test("${source}" LibCompress LIBS LibCompress)
endforeach()

if (NOT CMAKE_CROSSCOMPILING)
    add_custom_target(LibCompressCheckQuick
        COMMAND "$<TARGET_FILE:TestZlib>"
        DEPENDS TestZlib
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
    add_custom_target(LibCompressCheck
        COMMAND "$<TARGET_FILE:TestDeflate>"
        DEPENDS LibCompressCheckQuick TestDeflate
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
endif()

install(DIRECTORY brotli-test-files DESTINATION usr/Tests/LibCompress)
install(DIRECTORY deflate-test-files DESTINATION usr/Tests/LibCompress)
