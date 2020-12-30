#pragma once

#include "common/asserts.h"
#include "common/macros.h"
#include "common/types.h"

template<typename t_element, size_t k_element_count>
class s_static_array {
public:
	static constexpr size_t k_count = k_element_count;

	constexpr size_t get_count() const {
		return k_count;
	}

	constexpr t_element *get_elements() {
		return elements;
	}

	constexpr const t_element *get_elements() const {
		return elements;
	}

	constexpr t_element &operator[](size_t index) {
		wl_assert(valid_index(index, k_count));
		return elements[index];
	}

	constexpr const t_element &operator[](size_t index) const {
		wl_assert(valid_index(index, k_count));
		return elements[index];
	}

	// For loop iteration syntax

	constexpr t_element *begin() {
		return elements;
	}

	constexpr const t_element *begin() const {
		return elements;
	}

	constexpr t_element *end() {
		return elements + k_count;
	}

	constexpr const t_element *end() const {
		return elements + k_count;
	}

private:
	t_element elements[k_count];
};
