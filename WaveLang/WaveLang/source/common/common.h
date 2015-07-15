#ifndef WAVELANG_COMMON_H__
#define WAVELANG_COMMON_H__

#include "common/macros.h"
#include "common/platform.h"
#include "common/types.h"
#include "common/asserts.h"
#include <cctype>

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

// alignment must be a power of 2
template<typename t_size, typename t_alignment> t_size align_size(t_size size, t_alignment alignment) {
	return (size + (alignment - 1)) & (alignment - 1);
}

class c_uncopyable {
public:
	c_uncopyable() {}

private:
	// Not implemented
	c_uncopyable(const c_uncopyable &other);
	c_uncopyable &operator=(const c_uncopyable &other);
};

template<typename t_element>
class c_wrapped_array_const {
public:
	c_wrapped_array_const(const t_element *pointer, size_t count)
		: m_pointer(pointer)
		, m_count(count) {
	}

	template<size_t c>
	static c_wrapped_array_const<t_element> construct(const t_element(&arr)[c]) {
		return c_wrapped_array_const<t_element>(arr, c);
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
	c_wrapped_array(t_element *pointer, size_t count)
		: m_pointer(pointer)
		, m_count(count) {
	}

	template<size_t c>
	static c_wrapped_array<t_element> construct(t_element (&arr)[c]) {
		return c_wrapped_array<t_element>(arr, c);
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