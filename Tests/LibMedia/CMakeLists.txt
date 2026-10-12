set(TEST_SOURCES
    TestParseMatroska.cpp
    TestVP8Decode.cpp
    TestVP9Decode.cpp
)

foreach(source IN LISTS TEST_SOURCES)
    serenity_test("${source}" LibMedia LIBS LibMedia)
endforeach()

add_executable(TestVideoFrame EXCLUDE_FROM_ALL TestVideoFrame.cpp "$<TARGET_OBJECTS:LibTestMain>")
serenity_set_implicit_links(TestVideoFrame)
target_link_libraries(TestVideoFrame PRIVATE LibTest LibCore LibFileSystem LibMedia)
if (TARGET AK)
    target_link_libraries(TestVideoFrame PRIVATE AK)
endif()

if (NOT CMAKE_CROSSCOMPILING)
    add_custom_target(LibMediaCheckQuick
        COMMAND "$<TARGET_FILE:TestVideoFrame>"
        DEPENDS TestVideoFrame
        USES_TERMINAL
    )
    add_custom_target(LibMediaCheck
        COMMAND "$<TARGET_FILE:TestVP9Decode>"
        DEPENDS LibMediaCheckQuick TestVP9Decode
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        USES_TERMINAL
    )
endif()

install(FILES vp8_in_webm.webm DESTINATION usr/Tests/LibMedia)
install(FILES vp9_in_webm.webm DESTINATION usr/Tests/LibMedia)
install(FILES vp9_4k.webm DESTINATION usr/Tests/LibMedia)
install(FILES vp9_clamp_reference_mvs.webm DESTINATION usr/Tests/LibMedia)
install(FILES vp9_oob_blocks.webm DESTINATION usr/Tests/LibMedia)
install(FILES master_elements_containing_crc32.mkv DESTINATION usr/Tests/LibMedia)
install(FILES oss-fuzz-testcase-52630.vp9 DESTINATION usr/Tests/LibMedia)
install(FILES oss-fuzz-testcase-53977.vp9 DESTINATION usr/Tests/LibMedia)
install(FILES oss-fuzz-testcase-62054.vp9 DESTINATION usr/Tests/LibMedia)
install(FILES oss-fuzz-testcase-63182.vp9 DESTINATION usr/Tests/LibMedia)
