add_executable(TestPacketHeader EXCLUDE_FROM_ALL TestPacketHeader.cpp "$<TARGET_OBJECTS:LibTestMain>")
serenity_set_implicit_links(TestPacketHeader)
target_link_libraries(TestPacketHeader PRIVATE LibTest LibCore LibFileSystem LibDNS)
if (TARGET AK)
    target_link_libraries(TestPacketHeader PRIVATE AK)
endif()

if (NOT CMAKE_CROSSCOMPILING)
    add_custom_target(LibDNSCheckQuick
        COMMAND "$<TARGET_FILE:TestPacketHeader>"
        DEPENDS TestPacketHeader
        USES_TERMINAL
    )
    add_custom_target(LibDNSCheck DEPENDS LibDNSCheckQuick)
endif()
