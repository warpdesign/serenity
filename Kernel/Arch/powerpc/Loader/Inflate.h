/*
 * Copyright (c) 2026, SerenityOS developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Types.h>

namespace Kernel::PowerPC::Loader {

// Freestanding gzip (RFC 1952) / deflate (RFC 1951) decoder for the embedded kernel image.
// It allocates nothing; the caller supplies the output buffer.

// Returns the uncompressed size recorded in a gzip member's trailer, or 0 if the header is invalid.
u32 gzip_uncompressed_size(u8 const* data, u32 size);

// Inflates one gzip member into output and verifies its CRC-32 and size. Returns false on any error.
bool gzip_inflate(u8 const* data, u32 size, u8* output, u32 output_size);

}
