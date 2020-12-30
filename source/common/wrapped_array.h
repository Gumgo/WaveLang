#pragma once

#include "common/asserts.h"
#include "common/common_utilities.h"
#include "common/macros.h"
#include "common/types.h"

#include <vector>

// Wrapped arrays can safely be zero_type'd

// $TODO rename to c_array_view
template<typename t_element>
class c_wrapped_array {
public:
	constexpr c_wrapped_array() = default;
	constexpr c_wrapped_array(const c_wrapped_array &) = default;
	c_wrapped_array &operator=(const c_wrapped_array &) = default;

	constexpr c_wrapped_array(t_element *pointer, size_t count)
		: m_pointer(pointer)
		, m_count(count) {}

	template<
		typename t_mutable_element,
		CONDITION_DECLARATION(
			std::is_const_v<t_element> && std::is_same_v<t_mutable_element, std::remove_const_t<t_element>>)>
	constexpr c_wrapped_array(const c_wrapped_array<t_mutable_element> &other)
		: m_pointer(other.get_pointer())
		, m_count(other.get_count()) {}

	// A const vector will always return const elements, so make sure const t_vector_element == t_element
	template<
		typename t_vector_element,
		CONDITION_DECLARATION(std::is_same_v<std::add_const_t<t_vector_element>, t_element>)>
	c_wrapped_array(const std::vector<t_vector_element> &vector)
		: m_pointer(vector.empty() ? nullptr : vector.data())
		, m_count(vector.size()) {}

	// To support adding const, make sure t_vector_element == <remove const> t_element
	template<
		typename t_vector_element,
		CONDITION_DECLARATION(std::is_same_v<t_vector_element, std::remove_const_t<t_element>>)>
		c_wrapped_array(std::vector<t_vector_element> &vector)
		: m_pointer(vector.empty() ? nullptr : vector.data())
		, m_count(vector.size()) {}

	template<
		typename t_vector_element,
		CONDITION_DECLARATION(std::is_same_v<std::add_const_t<t_vector_element>, t_element>)>
		c_wrapped_array(const std::vector<t_vector_element> &vector, size_t offset, size_t count)
		: m_pointer(count == 0 ? nullptr : &vector[offset])
		, m_count(count) {
		wl_assert(offset + count <= vector.size());
	}

	template<
		typename t_vector_element,
		CONDITION_DECLARATION(std::is_same_v<t_vector_element, std::remove_const_t<t_element>>)>
		c_wrapped_array(std::vector<t_vector_element> &vector, size_t offset, size_t count)
		: m_pointer(count == 0 ? nullptr : &vector[offset])
		, m_count(count) {
		wl_assert(offset + count <= vector.size());
	}

	template<
		typename t_mutable_element,
		CONDITION_DECLARATION(
			std::is_const_v<t_element> &&std::is_same_v<t_mutable_element, std::remove_const_t<t_element>>)>
	c_wrapped_array &operator=(const c_wrapped_array<t_mutable_element> &other) {
		m_pointer = other.get_pointer();
		m_count = other.get_count();
		return *this;
	}

	template<size_t k_count>
	static constexpr c_wrapped_array<t_element> construct(t_element(&arr)[k_count]) {
		return c_wrapped_array<t_element>(arr, k_count);
	}

	constexpr size_t get_count() const {
		return m_count;
	}

	constexpr t_element *get_pointer() const {
		return m_pointer;
	}

	constexpr c_wrapped_array<t_element> get_range(size_t index, size_t count) const {
		wl_assert(count == 0 || valid_index(index, m_count));
		wl_assert(index + count <= m_count);
		return c_wrapped_array<t_element>(m_pointer + index, count);
	}

	constexpr t_element &operator[](size_t index) const {
		wl_assert(valid_index(index, m_count));
		return m_pointer[index];
	}

	constexpr operator c_wrapped_array<const t_element>() const {
		return c_wrapped_array<const t_element>(m_pointer, m_count);
	}

	// For loop iteration syntax

	constexpr t_element *begin() const {
		return m_pointer;
	}

	constexpr t_element *end() const {
		return m_pointer + m_count;
	}

private:
	t_element *m_pointer = nullptr;
	size_t m_count = 0;
};
