#pragma once

#include "common/common.h"

// A generic handle class wrapping simple data such as uint32. Handles are immutable once constructed and are type-safe
// - they can only be compared with other handles of the same type. Example handle definition:
// struct s_foo_handle_identifier {};
// using h_foo = c_handle<s_foo_handle_identifier, uint32>;
template<typename t_identifier, typename t_data, t_data k_invalid_data = static_cast<t_data>(-1)>
class c_handle {
public:
	static c_handle construct(t_data data) {
		c_handle result;
		result.m_data = data;
		return result;
	}

	static c_handle invalid() {
		return construct(k_invalid_data);
	}

	bool is_valid() const {
		return m_data != k_invalid_data;
	}

	t_data get_data() const {
		return m_data;
	}

	bool operator==(const c_handle &other) const {
		bool result = m_data == other.m_data;
		return result;
	}

	bool operator!=(const c_handle &other) const {
		return !(*this == other);
	}

private:
	t_data m_data;
};

