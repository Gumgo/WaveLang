#ifndef WAVELANG_COMMON_H__
#define WAVELANG_COMMON_H__

#include "common/macros.h"
#include "common/platform.h"
#include "common/types.h"
#include "common/asserts.h"
#include "common/cast_integer_verify.h"
#include "common/string.h"
#include "common/static_array.h"
#include "common/wrapped_array.h"
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <limits>

// Some utility functions... if this list gets too big, move these somewhere better

#define ALIGN_SIZE(size, alignment) ((size) + ((alignment) - 1) & ~((alignment) - 1))
#define ALIGN_SIZE_DOWN(size, alignment) ((size) & ~((alignment) - 1))
#define IS_SIZE_ALIGNED(size, alignment) ((size) & ((alignment) - 1) == 0)

// $TODO make alignment a template parameter, because we should never need runtime-custom alignment sizes
// alignment must be a power of 2
template<typename t_size, typename t_alignment> t_size align_size(t_size size, t_alignment alignment) {
	return (size + (alignment - 1)) & ~(alignment - 1);
}

template<typename t_size, typename t_alignment> t_size align_size_down(t_size size, t_alignment alignment) {
	return size & ~(alignment - 1);
}

template<typename t_size, typename t_alignment> bool is_size_aligned(t_size size, t_alignment alignment) {
	return (size & (alignment - 1)) == 0;
}

template<typename t_pointer, typename t_alignment> t_pointer align_pointer(t_pointer pointer, t_alignment alignment) {
	return reinterpret_cast<t_pointer>((reinterpret_cast<uintptr_t>(pointer) + (alignment - 1)) & ~(alignment - 1));
}

template<typename t_pointer, typename t_alignment> bool is_pointer_aligned(t_pointer *pointer, t_alignment alignment) {
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

#if PREDEFINED(ENDIANNESS_LITTLE)
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
#elif PREDEFINED(ENDIANNESS_BIG)
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


class c_uncopyable {
public:
	c_uncopyable() {}

private:
	// Not implemented
	c_uncopyable(const c_uncopyable &other);
	c_uncopyable &operator=(const c_uncopyable &other);
};

#endif // WAVELANG_COMMON_H__
