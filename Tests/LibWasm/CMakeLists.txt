serenity_testjs_test(test-wasm.cpp test-wasm LIBS LibWasm LibJS LibCrypto)
install(TARGETS test-wasm RUNTIME DESTINATION bin OPTIONAL)

add_executable(TestWasmReference EXCLUDE_FROM_ALL TestWasmReference.cpp "$<TARGET_OBJECTS:LibTestMain>")
serenity_set_implicit_links(TestWasmReference)
target_link_libraries(TestWasmReference PRIVATE LibTest LibCore LibFileSystem LibWasm)

add_custom_target(LibWasmCheckQuick DEPENDS TestWasmReference)
add_custom_target(LibWasmCheck DEPENDS LibWasmCheckQuick test-wasm)
