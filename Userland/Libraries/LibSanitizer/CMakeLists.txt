set(SOURCES
    UBSanitizer.cpp
    ../LibC/ssp.cpp
)

if (SERENITY_ARCH STREQUAL "powerpc")
    list(APPEND SOURCES ../LibC/arch/powerpc/stack_chk_fail_local.cpp)
endif()

serenity_libc(LibUBSanitizer ubsan)
target_link_options(LibUBSanitizer PRIVATE -nostdlib)

add_library(DynamicLoader_LibUBSanitizer STATIC UBSanitizer.cpp)
target_link_libraries(DynamicLoader_LibUBSanitizer PUBLIC DynamicLoader_CompileOptions)
