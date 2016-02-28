#ifndef WAVELANG_WRAPPED_ARRAY_H__
#define WAVELANG_WRAPPED_ARRAY_H__

#include "common/types.h"
#include "common/asserts.h"
#include "common/macros.h"

// Wrapped arrays can safely be ZERO_STRUCT'd
// $TODO consider switching to s_wrapped_array and making them POD

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
	static c_wrapped_array<t_element> construct(t_element(&arr)[k_count]) {
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

#endif // WAVELANG_WRAPPED_ARRAY_H__