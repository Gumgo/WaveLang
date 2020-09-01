#pragma once

#include "common/asserts.h"
#include "common/macros.h"
#include "common/types.h"

// Wrapped arrays can safely be ZERO_STRUCT'd
// $TODO consider switching to s_wrapped_array and making them POD

template<typename t_element>
class c_wrapped_array {
public:
	c_wrapped_array() = default;
	c_wrapped_array(const c_wrapped_array &) = default;
	c_wrapped_array &operator=(const c_wrapped_array &) = default;

	c_wrapped_array(t_element *pointer, size_t count)
		: m_pointer(pointer)
		, m_count(count) {
	}

	template<
		typename t_mutable_element,
		bool k_cond = std::is_const_v<t_element> && std::is_same_v<t_mutable_element, std::remove_const_t<t_element>>,
		typename = std::enable_if_t<k_cond>>
	c_wrapped_array(const c_wrapped_array<t_mutable_element> &other)
		: m_pointer(other.get_pointer())
		, m_count(other.get_count()) {
	}

	template<
		typename t_mutable_element,
		bool k_cond = std::is_const_v<t_element> &&std::is_same_v<t_mutable_element, std::remove_const_t<t_element>>,
		typename = std::enable_if_t<k_cond>>
	c_wrapped_array &operator=(const c_wrapped_array<t_mutable_element> &other) {
		m_pointer = other.get_pointer();
		m_count = other.get_count();
		return *this;
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

	c_wrapped_array<t_element> get_range(size_t index, size_t count) {
		wl_assert(count == 0 || VALID_INDEX(index, m_count));
		wl_assert(index + count <= m_count);
		return c_wrapped_array<t_element>(m_pointer + index, count);
	}

	c_wrapped_array<const t_element> get_range(size_t index, size_t count) const {
		wl_assert(count == 0 || VALID_INDEX(index, m_count));
		wl_assert(index + count <= m_count);
		return c_wrapped_array<const t_element>(m_pointer + index, count);
	}

	t_element &operator[](size_t index) {
		wl_assert(VALID_INDEX(index, m_count));
		return m_pointer[index];
	}

	const t_element &operator[](size_t index) const {
		wl_assert(VALID_INDEX(index, m_count));
		return m_pointer[index];
	}

	operator c_wrapped_array<const t_element>() const {
		return c_wrapped_array<const t_element>(m_pointer, m_count);
	}

	// For loop iteration syntax

	t_element *begin() {
		return m_pointer;
	}

	const t_element *begin() const {
		return m_pointer;
	}

	t_element *end() {
		return m_pointer + m_count;
	}

	const t_element *end() const {
		return m_pointer + m_count;
	}

private:
	t_element *m_pointer = nullptr;
	size_t m_count = 0;
};

