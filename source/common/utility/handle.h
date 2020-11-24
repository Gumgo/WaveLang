#pragma once

#include "common/common.h"

// A generic handle class wrapping simple data such as uint32. Handles are immutable once constructed and are type-safe
// - they can only be compared with other handles of the same type. Example handle definition:
// struct s_foo_handle_identifier {};
// using h_foo = c_handle<s_foo_handle_identifier, uint32>;
template<typename t_identifier, typename t_data, t_data k_invalid_data = static_cast<t_data>(-1)>
class c_handle {
public:
	using c_data = t_data;

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

// A simple handle iterator which assumes that the handle data represents an index
template<typename t_handle>
class c_index_handle_iterator {
public:
	using c_data = typename t_handle::c_data;

	c_index_handle_iterator(c_data index_count)
		: m_index_count(index_count)
		, m_index(0) {}

	bool is_valid() const {
		return m_index < m_index_count;
	}

	void next() {
		m_index++;
	}

	t_handle get_handle() const {
		wl_assert(is_valid());
		return t_handle::construct(m_index);
	}

	// For compatibility with range-based for loops:

	class c_iterand {
	public:
		c_iterand(c_data index)
			: m_index(index) {}

		bool operator!=(const c_iterand &other) const {
			return m_index != other.m_index;
		}

		c_iterand &operator++() {
			++m_index;
			return *this;
		}

		t_handle operator*() const {
			return t_handle::construct(m_index);
		}

	private:
		c_data m_index;
	};

	c_iterand begin() const {
		return c_iterand(0);
	}

	c_iterand end() const {
		return c_iterand(m_index_count);
	}

private:
	c_data m_index_count;
	c_data m_index;
};