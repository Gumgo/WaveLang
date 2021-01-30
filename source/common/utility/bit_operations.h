#pragma once

#include "common/common.h"

// Copies bits in the range [source + source_offset, source + source_offset + count) to the range [destination +
// destination_offset, destination + destination_offset + count). Offsets and count are specified in bits and copy
// operations are performed on 32-bit words.
void copy_bits(int32 *destination, size_t destination_offset, const int32 *source, size_t source_offset, size_t count);

// Replicates value to the range [destination + destination_offset, destination + destination_offset + count). Offset
// and count are specified in bits and copy operations are performed on 32-bit words.
void replicate_bit(int32 *destination, size_t destination_offset, bool value, size_t count);
