#ifndef WAVELANG_COMMON_H__
#define WAVELANG_COMMON_H__

#include "common\types.h"
#include "common\asserts.h"
#include <cctype>

// Causes an error if x is used before it has been defined
#define PREDEFINED(x) (1 / defined x## && x)

#define NUMBEROF(x) (sizeof(x) / (sizeof(x[0])))

#define VALID_INDEX(i, c) ((i) >= 0 && (i) < (c))

#define ZERO_STRUCT(s) memset(s, 0, sizeof(*s))

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

template<typename T>
class c_wrapped_array_const {
public:
	c_wrapped_array_const(const T *pointer, size_t count)
		: m_pointer(pointer)
		, m_count(count) {
	}

	template<size_t c>
	static c_wrapped_array_const<T> construct(const T (&arr)[c]) {
		return c_wrapped_array_const<T>(arr, c);
	}

	size_t get_count() const {
		return m_count;
	}

	const T *get_pointer() const {
		return m_pointer;
	}

	const T &operator[](size_t index) const {
		wl_assert(VALID_INDEX(index, m_count));
		return m_pointer[index];
	}

private:
	const T *m_pointer;
	size_t m_count;
};

template<typename T>
class c_wrapped_array {
public:
	c_wrapped_array(T *pointer, size_t count)
		: m_pointer(pointer)
		, m_count(count) {
	}

	template<size_t c>
	static c_wrapped_array<T> construct(T (&arr)[c]) {
		return c_wrapped_array<T>(arr, c);
	}

	size_t get_count() const {
		return m_count
	};

	T *get_pointer() {
		return m_pointer;
	}

	const T *get_pointer() const {
		return m_pointer;
	}

	T &operator[](size_t index) {
		wl_assert(VALID_INDEX(index, m_count));
		return m_pointer[index];
	}

	const T &operator[](size_t index) const {
		wl_assert(VALID_INDEX(index, m_count));
		return m_pointer[index];
	}

	operator c_wrapped_array_const<T>() const {
		return c_wrapped_array_const<T>(m_pointer, m_count);
	}

private:
	T *m_pointer;
	size_t m_count;
};

#endif // WAVELANG_COMMON_H__