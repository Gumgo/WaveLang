#include "common/common.h"
#include "common/utility/bit_operations.h"

#include <gtest/gtest.h>

static constexpr int32 k_destination_buffer_contents[] = {
	0b00101101001011010011001111001100,
	0b00110011110011000010110100101101,
	0b00010111011100010111000100010111,
	0b01110001000101110001011101110001
};

static constexpr int32 k_source_buffer_contents[] = {
	0b11010010110100101100110000110011,
	0b11001100001100111101001011010010,
	0b11101000100011101000111011101000,
	0b10001110111010001110100010001110
};

static constexpr size_t k_buffer_count = 4;
STATIC_ASSERT(array_count(k_destination_buffer_contents) == k_buffer_count);
STATIC_ASSERT(array_count(k_source_buffer_contents) == k_buffer_count);

static bool copy_bits_and_compare(size_t destination_offset, size_t source_offset, size_t count) {
	wl_assert(destination_offset + count <= k_buffer_count * 32);
	wl_assert(source_offset + count <= k_buffer_count * 32);
	int32 buffer[k_buffer_count];

	copy_type(buffer, k_destination_buffer_contents, k_buffer_count);
	copy_bits(buffer, destination_offset, k_source_buffer_contents, source_offset, count);

	// Do a slow test to make sure each bit matches
	for (size_t bit = 0; bit < k_buffer_count * 32; bit++) {
		int32 expected_value;
		if (bit < destination_offset || bit >= destination_offset + count) {
			expected_value = (k_destination_buffer_contents[bit / 32] >> (bit % 32)) & 1;
		} else {
			size_t source_bit = bit - destination_offset + source_offset;
			expected_value = (k_source_buffer_contents[source_bit / 32] >> (source_bit % 32)) & 1;
		}

		int32 actual_value = (buffer[bit / 32] >> (bit % 32)) & 1;

		if (expected_value != actual_value) {
			return false;
		}
	}

	return true;
}

static bool replicate_bit_and_compare(size_t destination_offset, size_t count) {
	wl_assert(destination_offset + count <= k_buffer_count * 32);
	int32 buffer[k_buffer_count];

	// Replicate 1s across a buffer of 0s
	zero_type(buffer, k_buffer_count);
	replicate_bit(buffer, destination_offset, true, count);

	// Do a slow test to make sure each bit matches
	for (size_t bit = 0; bit < k_buffer_count * 32; bit++) {
		int32 expected_value = bit >= destination_offset && bit < destination_offset + count;
		int32 actual_value = (buffer[bit / 32] >> (bit % 32)) & 1;

		if (expected_value != actual_value) {
			return false;
		}
	}

	// Replicate 0s across a buffer of 1s
	zero_type(buffer, k_buffer_count);
	for (int32 &value : buffer) {
		value = ~value;
	}

	replicate_bit(buffer, destination_offset, false, count);

	// Do a slow test to make sure each bit matches
	for (size_t bit = 0; bit < k_buffer_count * 32; bit++) {
		int32 expected_value = bit < destination_offset || bit >= destination_offset + count;
		int32 actual_value = (buffer[bit / 32] >> (bit % 32)) & 1;

		if (expected_value != actual_value) {
			return false;
		}
	}

	return true;
}

TEST(Utility, BitOperations) {
	static constexpr size_t k_copy_sizes[] = { 0, 1, 3, 17, 28, 32, 41, 55, 64, 70, 82, 96, 104, 127, 128 };
	for (size_t copy_size : k_copy_sizes) {
		size_t max_offset = k_buffer_count * 32 - copy_size;
		for (size_t destination_offset = 0; destination_offset < max_offset; destination_offset++) {
			EXPECT_TRUE(replicate_bit_and_compare(destination_offset, copy_size));
			for (size_t source_offset = 0; source_offset < max_offset; source_offset++) {
				EXPECT_TRUE(copy_bits_and_compare(destination_offset, source_offset, copy_size));
			}
		}
	}
}
