set(TEST_SOURCES
    TestAES.cpp
    TestASN1.cpp
    TestBigFraction.cpp
    TestBigInteger.cpp
    TestChaCha20.cpp
    TestChacha20Poly1305.cpp
    TestChecksum.cpp
    TestCurves.cpp
    TestEd25519.cpp
    TestHash.cpp
    TestHKDF.cpp
    TestHMAC.cpp
    TestMGF.cpp
    TestOAEP.cpp
    TestPBKDF2.cpp
    TestPoly1305.cpp
    TestRSA.cpp
)

foreach(source IN LISTS TEST_SOURCES)
    serenity_test("${source}" LibCrypto LIBS LibCrypto)
endforeach()

if (NOT CMAKE_CROSSCOMPILING)
    add_custom_target(LibCryptoCheckQuick
        COMMAND "$<TARGET_FILE:TestMGF>"
        DEPENDS TestMGF
        USES_TERMINAL
    )
    add_custom_target(LibCryptoCheck
        COMMAND "$<TARGET_FILE:TestOAEP>"
        DEPENDS LibCryptoCheckQuick TestOAEP
        USES_TERMINAL
    )
endif()
