#ifndef WAVELANG_COMMON_UTILITY_RADIX_SORT_H__
#define WAVELANG_COMMON_UTILITY_RADIX_SORT_H__

#include "common/common.h"

// Works for integers
template<typename t_key>
struct s_get_integer_byte {
	uint8 operator()(const t_key &key, size_t index) const {
		return static_cast<uint8>(key >> (index * 8));
	}
};

// Basic implementation of stable radix sort.
// - t_get_key should override the () operator to take a t_value and produce a t_key
// - t_get_key_byte should return the Nth byte of the given key, used for bucketing. For integer sorting, this is
// trivial. To support floats, some bit manipulation is required. By default, an integer implementation is provided.
template<typename t_key, typename t_value, typename t_get_key, typename t_get_key_byte = s_get_integer_byte<t_key>>
void radix_sort_stable(
	c_wrapped_array<t_value> values,
	c_wrapped_array<t_value> intermediate_storage,
	const t_get_key &get_key,
	const t_get_key_byte &get_key_byte = t_get_key_byte()) {
	static const size_t k_key_size = sizeof(t_key);

	struct s_bucket {
		size_t count;
		size_t start_index;
	};

	s_static_array<uint8, 256> buckets;

	for (size_t pass = 0; pass < k_key_size; pass++) {
		ZERO_STRUCT(&buckets);

		// First, determine the size of each bucket
		for (size_t index = 0; index < values.get_count(); index++) {
			t_key key = get_key(values[index]);
			uint8 key_byte = get_key_byte(key);
			buckets[key_byte].count++;
		}

		// Determine start indices for all buckets
		size_t next_start_index = 0;
		for (size_t bucket = 0; bucket < 256; bucket++) {
			buckets[bucket].start_index = next_start_index;
			next_start_index += buckets[bucket].count;
			buckets[bucket].count = 0;
		}

		// Scatter elements into buckets
		for (size_t index = 0; index < values.get_count(); index++) {
			t_key key = get_key(values[index]);
			uint8 key_byte = get_key_byte(key);

			size_t intermediate_index = buckets[key_byte].start_index + buckets[key_byte].count;
			buckets[key_byte].count++;

			// Copy into the bucket
			intermediate_storage[intermediate_index] = values[index];
		}

		// Gather elements back into original storage array
		size_t next_index = 0;
		for (size_t bucket = 0; bucket < 256; bucket++) {
			size_t start = buckets[bucket].start_index;
			size_t end = start + buckets[bucket].count;

			for (size_t index = start; index < end; index++) {
				values[next_index] = intermediate_storage[index];
				next_index++;
			}
		}
	}
}

#endif // WAVELANG_COMMON_UTILITY_RADIX_SORT_H__
