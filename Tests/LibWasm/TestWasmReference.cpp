/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <LibWasm/AbstractMachine/AbstractMachine.h>
#include <LibWasm/Types.h>

TEST_CASE(function_reference_preserves_module_pointer)
{
    auto module = adopt_ref(*new Wasm::Module);
    Wasm::Reference reference { Wasm::Reference::Func { Wasm::FunctionAddress { 42 }, module } };
    Wasm::Value encoded { reference };
    auto decoded = encoded.to<Wasm::Reference>();
    EXPECT(decoded.ref().has<Wasm::Reference::Func>());
    if (!decoded.ref().has<Wasm::Reference::Func>())
        return;
    auto const& function = decoded.ref().get<Wasm::Reference::Func>();
    EXPECT_EQ(function.address.value(), 42u);
    EXPECT_EQ(function.source_module.ptr(), module.ptr());
}
