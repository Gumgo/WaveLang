#ifndef WAVELANG_COMMON_UTILITY_POD_H__
#define WAVELANG_COMMON_UTILITY_POD_H__

#include "common/common.h"

// Wrapper to turn a non-POD type into a POD type so construction/destruction can be manually controlled
template<typename t_value>
struct alignas(alignof(t_value)) s_pod {
	uint8 data[sizeof(t_value)];

	t_value &get() {
		return *reinterpret_cast<t_value *>(this);
	}

	const t_value &get() const {
		return *reinterpret_cast<const t_value *>(this);
	}

	t_value *get_pointer() {
		return reinterpret_cast<t_value *>(this);
	}

	const t_value *get_pointer() const {
		return reinterpret_cast<const t_value *>(this);
	}
};

#endif // WAVELANG_COMMON_UTILITY_POD_H__
