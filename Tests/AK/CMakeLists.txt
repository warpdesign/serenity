set(AK_TEST_SOURCES
    TestAKMath.cpp
    TestAllOf.cpp
    TestAnyOf.cpp
    TestArbitrarySizedEnum.cpp
    TestArray.cpp
    TestAtomic.cpp
    TestBadge.cpp
    TestBase64.cpp
    TestBinaryHeap.cpp
    TestBinarySearch.cpp
    TestBitCast.cpp
    TestBitmap.cpp
    TestBitStream.cpp
    TestBuiltinWrappers.cpp
    TestByteBuffer.cpp
    TestByteString.cpp
    TestCharacterTypes.cpp
    TestChecked.cpp
    TestCircularBuffer.cpp
    TestCircularDeque.cpp
    TestCircularQueue.cpp
    TestComplex.cpp
    TestConstrainedStream.cpp
    TestCoroutine.cpp
    TestDOSPackedTime.cpp
    TestDisjointChunks.cpp
    TestDistinctNumeric.cpp
    TestDoublyLinkedList.cpp
    TestEndian.cpp
    TestEnumBits.cpp
    TestEnumerate.cpp
    TestFind.cpp
    TestFixedArray.cpp
    TestFixedPoint.cpp
    TestFloatingPoint.cpp
    TestFloatingPointParsing.cpp
    TestFloatingPointStringConversions.cpp
    TestFlyString.cpp
    TestFormat.cpp
    TestFunction.cpp
    TestFuzzyMatch.cpp
    TestGeneratorAK.cpp
    TestGenericLexer.cpp
    TestHashFunctions.cpp
    TestHashMap.cpp
    TestHashTable.cpp
    TestHex.cpp
    TestIPv4Address.cpp
    TestIPv6Address.cpp
    TestIndexSequence.cpp
    TestInsertionSort.cpp
    TestIntegerMath.cpp
    TestInternetChecksum.cpp
    TestIntrusiveList.cpp
    TestIntrusiveRedBlackTree.cpp
    TestJSON.cpp
    TestLEB128.cpp
    TestLexicalPath.cpp
    TestMACAddress.cpp
    TestMemory.cpp
    TestMemoryStream.cpp
    TestNeverDestroyed.cpp
    TestNonnullOwnPtr.cpp
    TestNonnullRefPtr.cpp
    TestNumberFormat.cpp
    TestOptional.cpp
    TestOptionParser.cpp
    TestOwnPtr.cpp
    TestPrint.cpp
    TestQueue.cpp
    TestQuickSelect.cpp
    TestQuickSort.cpp
    TestRedBlackTree.cpp
    TestRefPtr.cpp
    TestSegmentedVector.cpp
    TestSIMD.cpp
    TestSIMDExtras.cpp
    TestSinglyLinkedList.cpp
    TestSlugify.cpp
    TestSourceGenerator.cpp
    TestSourceLocation.cpp
    TestSpan.cpp
    TestStack.cpp
    TestStatistics.cpp
    TestStdLibExtras.cpp
    TestString.cpp
    TestStringFloatingPointConversions.cpp
    TestStringUtils.cpp
    TestStringView.cpp
    TestSyncGenerator.cpp
    TestDuration.cpp
    TestTrie.cpp
    TestTuple.cpp
    TestTypeTraits.cpp
    TestTypedTransfer.cpp
    TestUFixedBigInt.cpp
    TestUtf16.cpp
    TestUtf8.cpp
    TestVariant.cpp
    TestVector.cpp
    TestWeakPtr.cpp
)

foreach(source IN LISTS AK_TEST_SOURCES)
    serenity_test("${source}" AK)
endforeach()

target_link_libraries(TestString PRIVATE LibUnicode)

if (NOT CMAKE_CROSSCOMPILING)
    serenity_test("TestPowerPCSqrt.cpp" AK)
    add_custom_target(AKCheckQuick
        COMMAND "$<TARGET_FILE:TestBitStream>"
        COMMAND "$<TARGET_FILE:TestAKMath>" --tests sqrt_runtime
        COMMAND "$<TARGET_FILE:TestPowerPCSqrt>" --tests
        COMMAND "$<TARGET_FILE:TestIPv4Address>"
        COMMAND "$<TARGET_FILE:TestUFixedBigInt>" --tests endian_swap
        DEPENDS TestBitStream TestAKMath TestPowerPCSqrt TestIPv4Address TestUFixedBigInt
        USES_TERMINAL
    )
    add_custom_target(AKCheck
        COMMAND "$<TARGET_FILE:TestEndian>"
        DEPENDS AKCheckQuick TestEndian
        USES_TERMINAL
    )
endif()

if (SERENITY_ARCH STREQUAL "powerpc")
    set(powerpc_ak_math_test "${CMAKE_CURRENT_SOURCE_DIR}/PowerPC/test_sqrt_runtime.py")
    set(powerpc_ak_math_arguments
        --compiler "${CMAKE_CXX_COMPILER}" --qemu qemu-system-ppc
        --kernel-loader "${CMAKE_BINARY_DIR}/Kernel/Arch/powerpc/Loader/image/serenity-powerpc-loader.elf"
        --user-loader "$<TARGET_FILE:Loader.so>"
        --libc "$<TARGET_FILE:LibC>" --libsystem "$<TARGET_FILE:LibSystem>"
    )
    add_custom_target(PowerPCAKMathCheckQuick
        COMMAND python3 "${powerpc_ak_math_test}" ${powerpc_ak_math_arguments}
        DEPENDS PowerPCFirmwareLoader Loader.so LibC LibSystem
        USES_TERMINAL
        VERBATIM
    )
    add_custom_target(PowerPCAKMathCheck
        COMMAND python3 "${powerpc_ak_math_test}" ${powerpc_ak_math_arguments} --memory 512 --cpu 750cxe
        DEPENDS PowerPCAKMathCheckQuick
        USES_TERMINAL
        VERBATIM
    )
endif()
