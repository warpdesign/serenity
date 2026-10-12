/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/Value.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" void _start()
{
    auto undefined = JS::js_undefined();
    if (!undefined.is_undefined() || undefined.tag() != JS::UNDEFINED_TAG)
        exit(1);

    JS::Value number { i32 { 42 } };
    if (!number.is_number() || number.as_i32() != 42)
        exit(2);

    JS::Value object { reinterpret_cast<JS::Object*>(FlatPtr { 0x1000 }) };
    if (!object.is_object() || JS::Value::extract_pointer_bits(object.encoded()) != 0x1000)
        exit(3);

    puts("js-value-tag=ok");
    exit(0);
}
