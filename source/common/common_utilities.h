#pragma once

#include "common/macros.h"
#include "common/platform.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>

template<typename t_element, size_t k_count>
constexpr size_t array_count(const t_element (&)[k_count]) {
	return k_count;
}

constexpr size_t valid_index(size_t index, size_t count) {
	return index < count;
}

template<typename t_type>
void zero_type(t_type *value, size_t count = 1) {
	memset(value, 0, count * sizeof(t_type));
}

template<typename t_type>
void copy_type(t_type *destination, const t_type *source, size_t count = 1) {
	memcpy(destination, source, count * sizeof(t_type));
}

template<typename t_type>
void copy_type_with_overlap(t_type *destination, const t_type *source, size_t count = 1) {
	memmove(destination, source, count * sizeof(t_type));
}

// Alignment must be a power of 2 for all alignment functions
// Note: could make alignment a template parameter, because we should never need runtime-custom alignment sizes.
// However, since these functions are inlined, the constant alignment parameters will almost certainly optimize to the
// same result.
template<typename t_size, typename t_alignment>
constexpr t_size align_size(t_size size, t_alignment alignment) {
	return (size + (alignment - 1)) & ~(alignment - 1);
}

template<typename t_size, typename t_alignment>
constexpr t_size align_size_down(t_size size, t_alignment alignment) {
	return size & ~(alignment - 1);
}

template<typename t_size, typename t_alignment>
constexpr bool is_size_aligned(t_size size, t_alignment alignment) {
	return (size & (alignment - 1)) == 0;
}

template<typename t_pointer, typename t_alignment>
constexpr t_pointer align_pointer(t_pointer pointer, t_alignment alignment) {
	return reinterpret_cast<t_pointer>((reinterpret_cast<uintptr_t>(pointer) + (alignment - 1)) & ~(alignment - 1));
}

template<typename t_pointer, typename t_alignment>
constexpr bool is_pointer_aligned(t_pointer *pointer, t_alignment alignment) {
	return is_size_aligned(reinterpret_cast<size_t>(pointer), alignment);
}

template<typename t_value> t_value swap_byte_order(t_value value) {
	t_value result;
	for (size_t index = 0; index < sizeof(result); index++) {
		reinterpret_cast<uint8 *>(&result)[index] = reinterpret_cast<const uint8 *>(&value)[sizeof(value) - index - 1];
	}
	return result;
}

template<typename t_value> t_value clamp(t_value value, t_value lower, t_value upper) {
	return std::min(std::max(value, lower), upper);
}

#if IS_TRUE(ENDIANNESS_LITTLE)
template<typename t_value> t_value native_to_little_endian(t_value value) {
	return value;
}

template<typename t_value> t_value native_to_big_endian(t_value value) {
	return swap_byte_order(value);
}

template<typename t_value> t_value little_to_native_endian(t_value value) {
	return value;
}

template<typename t_value> t_value big_to_native_endian(t_value value) {
	return swap_byte_order(value);
}
#elif IS_TRUE(ENDIANNESS_BIG)
template<typename t_value> t_value native_to_little_endian(t_value value) {
	return swap_byte_order(value);
}

template<typename t_value> t_value native_to_big_endian(t_value value) {
	return value;
}

template<typename t_value> t_value little_to_native_endian(t_value value) {
	return swap_byte_order(value);
}

template<typename t_value> t_value big_to_native_endian(t_value value) {
	return value;
}
#else // endianness
#error Unknown endianness
#endif // endianness

template<typename t_storage, typename t_bit>
void set_bit(t_storage &storage_inout, t_bit bit, bool value) {
	t_storage mask = static_cast<t_storage>(1 << static_cast<uint32>(bit));
	storage_inout = value
		? (storage_inout | mask)
		: (storage_inout & ~mask);
}

template<typename t_storage, typename t_bit>
bool test_bit(t_storage storage, t_bit bit) {
	return (storage & static_cast<t_storage>(1 << static_cast<uint32>(bit))) != 0;
}

// Add this to a class's declaration to disable copy constructor and assignment operator
#define UNCOPYABLE(class_name)							\
	class_name(const class_name &) = delete;			\
	class_name &operator=(const class_name &) = delete

// Add this to a class's declaration to disable copy constructor and assignment operator but allow default move behavior
#define UNCOPYABLE_MOVABLE(class_name)				\
	UNCOPYABLE(class_name);							\
	class_name(class_name &&) = default;			\
	class_name &operator=(class_name &&) = default

template<typename t_to, typename t_from>
t_to reinterpret_bits(t_from from) {
	static_assert(sizeof(t_to) == sizeof(t_from), "Can't reinterpret_bits for types of different sizes");

	// This SHOULD get optimized...
	t_to to;
	memcpy(&to, &from, sizeof(to));
	return to;
}
