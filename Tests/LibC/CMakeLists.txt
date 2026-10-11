set(TEST_SOURCES
    TestAbort.cpp
    TestAssert.cpp
    TestCType.cpp
    TestEnvironment.cpp
    TestFenv.cpp
    TestIo.cpp
    TestLibCExec.cpp
    TestLibCGrp.cpp
    TestLibCDirEnt.cpp
    TestLibCInodeWatcher.cpp
    TestLibCMkTemp.cpp
    TestLibCNetdb.cpp
    TestLibCSetjmp.cpp
    TestLocalStaticInitialization.cpp
    TestLibCString.cpp
    TestLibCTime.cpp
    TestMalloc.cpp
    TestMath.cpp
    TestMemalign.cpp
    TestMemmem.cpp
    TestMkDir.cpp
    TestPthreadCancel.cpp
    TestPthreadCleanup.cpp
    TestPThreadPriority.cpp
    TestPthreadSpinLocks.cpp
    TestPthreadRWLocks.cpp
    TestPwd.cpp
    TestQsort.cpp
    TestRaise.cpp
    TestRealpath.cpp
    TestScanf.cpp
    TestSearch.cpp
    TestSignal.cpp
    TestSnprintf.cpp
    TestStackSmash.cpp
    TestStdio.cpp
    TestStpncpy.cpp
    TestStrftime.cpp
    TestStrlcpy.cpp
    TestStrtodAccuracy.cpp
    TestTruncate.cpp
    TestThreadLocalDestructors.cpp
    TestWchar.cpp
    TestWctype.cpp
)

set_source_files_properties(TestMath.cpp PROPERTIES COMPILE_FLAGS "-fno-builtin")
set_source_files_properties(TestStrtodAccuracy.cpp PROPERTIES COMPILE_FLAGS "-fno-builtin-strtod")
# Don't assume default rounding behavior is used for testing rounding behavior modifications.
set_source_files_properties(TestFenv.cpp PROPERTIES COMPILE_FLAGS "-frounding-math")

foreach(source IN LISTS TEST_SOURCES)
    serenity_test("${source}" LibC)
endforeach()

if (SERENITY_ARCH STREQUAL "powerpc")
    set(powerpc_rtti_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_rtti_runtime.py")
    set(powerpc_kernel_loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf")
    set(powerpc_rtti_arguments
        --compiler "${CMAKE_CXX_COMPILER}" --readelf "${CMAKE_READELF}"
        --qemu qemu-system-ppc --kernel-loader "${powerpc_kernel_loader}"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --libc "$<TARGET_FILE:LibC>" --libsystem "$<TARGET_FILE:LibSystem>"
    )
    add_custom_target(PowerPCLibCCheckQuick
        COMMAND python3 "${powerpc_rtti_test}" ${powerpc_rtti_arguments}
        DEPENDS PowerPCFirmwareLoader Loader.so LibC LibSystem
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCLibCCheck
        COMMAND python3 "${powerpc_rtti_test}" ${powerpc_rtti_arguments} --memory 512 --cpu 750cxe
        DEPENDS PowerPCLibCCheckQuick
        USES_TERMINAL
        VERBATIM
    )
endif()
