if (SERENITY_ARCH STREQUAL "powerpc")
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    find_program(WINDOWSERVER_PROFILE_HOST_CXX NAMES clang++ g++ REQUIRED)
    set(windowserver_profile_test "${CMAKE_CURRENT_SOURCE_DIR}/TestCompositionProfileStats.cpp")
    set(windowserver_profile_executable "${CMAKE_CURRENT_BINARY_DIR}/WindowServer-composition-profile-check")
    add_custom_target(WindowServerCompositionProfileCheckQuick
        COMMAND "${WINDOWSERVER_PROFILE_HOST_CXX}" -std=c++23 -I "${CMAKE_SOURCE_DIR}"
            "${windowserver_profile_test}" -o "${windowserver_profile_executable}"
        COMMAND "${windowserver_profile_executable}"
        VERBATIM
    )
    add_custom_target(WindowServerCompositionProfileCheck
        COMMAND "${windowserver_profile_executable}" extended
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_composition_profile_root.py"
            "${CMAKE_BINARY_DIR}/PowerPCBoot/pegasos2-root.ext2"
        DEPENDS WindowServerCompositionProfileCheckQuick
        VERBATIM
    )
endif()
