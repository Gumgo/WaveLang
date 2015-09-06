#ifndef WAVELANG_COMMON_H__
#define WAVELANG_COMMON_H__

#include "common/macros.h"
#include "common/platform.h"
#include "common/types.h"
#include "common/asserts.h"
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>

// Some utility functions... if this list gets too big, move these somewhere better
inline bool string_compare_case_insensitive(const char *str_a, const char *str_b) {
	for (size_t index = 0; true; index++) {
		if (str_a[index] == '\0' && str_b[index] == '\0') {
			// Both same length, and we haven't yet found a mismatch
			return true;
		} else if (::tolower(str_a[index]) != ::tolower(str_b[index])) {
			// If either character differs, return false
			// This will also trigger if one string is shorter than the other, because one character will be \0
			return false;
		}
	}
}

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

// Wrapped arrays can safely be ZERO_STRUCT'd

template<typename t_element>
class c_wrapped_array_const {
public:
	c_wrapped_array_const()
		: m_pointer(nullptr)
		, m_count(0) {
	}

	c_wrapped_array_const(const t_element *pointer, size_t count)
		: m_pointer(pointer)
		, m_count(count) {
	}

	template<size_t k_count>
	static c_wrapped_array_const<t_element> construct(const t_element(&arr)[k_count]) {
		return c_wrapped_array_const<t_element>(arr, k_count);
	}

	size_t get_count() const {
		return m_count;
	}

	const t_element *get_pointer() const {
		return m_pointer;
	}

	const t_element &operator[](size_t index) const {
		wl_assert(VALID_INDEX(index, m_count));
		return m_pointer[index];
	}

private:
	const t_element *m_pointer;
	size_t m_count;
};

template<typename t_element>
class c_wrapped_array {
public:
	c_wrapped_array()
		: m_pointer(nullptr)
		, m_count(0) {
	}

	c_wrapped_array(t_element *pointer, size_t count)
		: m_pointer(pointer)
		, m_count(count) {
	}

	template<size_t k_count>
	static c_wrapped_array<t_element> construct(t_element (&arr)[k_count]) {
		return c_wrapped_array<t_element>(arr, k_count);
	}

	size_t get_count() const {
		return m_count;
	}

	t_element *get_pointer() {
		return m_pointer;
	}

	const t_element *get_pointer() const {
		return m_pointer;
	}

	t_element &operator[](size_t index) {
		wl_assert(VALID_INDEX(index, m_count));
		return m_pointer[index];
	}

	const t_element &operator[](size_t index) const {
		wl_assert(VALID_INDEX(index, m_count));
		return m_pointer[index];
	}

	operator c_wrapped_array_const<t_element>() const {
		return c_wrapped_array_const<t_element>(m_pointer, m_count);
	}

private:
	t_element *m_pointer;
	size_t m_count;
};

#endif // WAVELANG_COMMON_H__
