#pragma once

#include "common/asserts.h"
#include "common/cast_integer_verify.h"
#include "common/macros.h"
#include "common/platform.h"
#include "common/platform_macros.h"
#include "common/static_array.h"
#include "common/string.h"
#include "common/types.h"
#include "common/wrapped_array.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

// Some utility functions... if this list gets too big, move these somewhere better

// Alignment must be a power of 2 for all alignment functions

#define ALIGN_SIZE(size, alignment) ((size) + (((alignment) - 1) & ~((alignment) - 1)))
#define ALIGN_SIZE_DOWN(size, alignment) ((size) & ~((alignment) - 1))
#define IS_SIZE_ALIGNED(size, alignment) ((size) & ((alignment) - 1) == 0)

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
void set_bit(t_storage &inout_storage, t_bit bit, bool value) {
	t_storage mask = static_cast<t_storage>(1 << static_cast<uint32>(bit));
	inout_storage = value ?
		(inout_storage | mask) :
		(inout_storage & ~mask);
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

template<typename t_enum>
constexpr std::underlying_type_t<t_enum> enum_index(t_enum value) {
	return static_cast<std::underlying_type_t<t_enum>>(value);
}

template<typename t_enum>
constexpr std::underlying_type_t<t_enum> enum_count() {
	return static_cast<std::underlying_type_t<t_enum>>(t_enum::k_count);
}

template<typename t_enum>
constexpr bool valid_enum_index(t_enum value) {
	return enum_index(value) >= 0 && enum_index(value) < enum_count<t_enum>();
}

template<typename t_enum>
class c_enum_iterator {
public:
	c_enum_iterator()
		: m_index(0) {
	}

	bool is_valid() const {
		return m_index < enum_count<t_enum>();
	}

	void next() {
		m_index++;
	}

	t_enum get() const {
		wl_assert(is_valid());
		return static_cast<t_enum>(m_index);
	}

	class c_iterand {
	public:
		c_iterand(std::underlying_type_t<t_enum> index)
			: m_index(index) {
		}

		bool operator!=(const c_iterand &other) {
			return m_index != other.m_index;
		}

		c_iterand &operator++() {
			m_index++;
			return *this;
		}

		t_enum operator*() const {
			return static_cast<t_enum>(m_index);
		}

	private:
		std::underlying_type_t<t_enum> m_index;
	};

	c_iterand begin() const {
		return c_iterand(0);
	}

	c_iterand end() const {
		return c_iterand(enum_count<t_enum>());
	}

private:
	std::underlying_type_t<t_enum> m_index;
};

template<typename t_enum>
c_enum_iterator<t_enum> iterate_enum() {
	return c_enum_iterator<t_enum>();
}
