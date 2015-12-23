#ifndef WAVELANG_STATIC_ARRAY_H__
#define WAVELANG_STATIC_ARRAY_H__

#include "common/types.h"
#include "common/asserts.h"
#include "common/macros.h"

template<typename t_element, size_t k_element_count>
class s_static_array {
public:
	static const size_t k_count = k_element_count;

	size_t get_count() const {
		return k_count;
	}

	t_element *get_elements() {
		return elements;
	}

	const t_element *get_elements() const {
		return elements;
	}

	t_element &operator[](size_t index) {
		wl_assert(VALID_INDEX(index, k_count));
		return elements[index];
	}

	const t_element &operator[](size_t index) const {
		wl_assert(VALID_INDEX(index, k_count));
		return elements[index];
	}

private:
	t_element elements[k_count];
};

#endif // WAVELANG_WRAPPED_ARRAY_H__