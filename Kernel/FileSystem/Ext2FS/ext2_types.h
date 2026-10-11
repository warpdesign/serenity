/*
 * Copyright (c) 2018-2026, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Endian.h>

// These types appear only in ext2's on-disk structures. Accessing a field
// converts it to host byte order; assigning it writes little-endian bytes.
using __u8 = u8;
using __s8 = i8;
using __u16 = AK::LittleEndian<u16>;
using __s16 = AK::LittleEndian<i16>;
using __u32 = AK::LittleEndian<u32>;
using __s32 = AK::LittleEndian<i32>;
using __u64 = AK::LittleEndian<u64>;
using __s64 = AK::LittleEndian<i64>;
