#pragma once

#include "common/common.h"

template<typename t_data>
struct s_make_invalid_handle_data {
	constexpr t_data operator()() const {
		return static_cast<t_data>(-1);
	}
};

// A generic handle class wrapping simple data such as uint32. Handles are immutable once constructed and are type-safe
// - they can only be compared with other handles of the same type. Example handle definition:
// struct s_foo_handle_identifier {};
// using h_foo = c_handle<s_foo_handle_identifier, uint32>;
template<
	typename t_identifier,
	typename t_data,
	typename t_make_invalid_data = s_make_invalid_handle_data<t_data>>
class c_handle {
public:
	using c_data = t_data;

	static c_handle construct(t_data data) {
		c_handle result;
		result.m_data = data;
		return result;
	}

	static c_handle invalid() {
		return c_handle();
	}

	c_handle() {
		m_data = t_make_invalid_data()();
	}

	bool is_valid() const {
		return m_data != t_make_invalid_data()();
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

namespace std {
	template<typename t_identifier, typename t_data, typename t_make_invalid_data>
	struct hash<c_handle<t_identifier, t_data, t_make_invalid_data>> {
		size_t operator()(const c_handle<t_identifier, t_data, t_make_invalid_data> &key) const {
			return hash<t_data>()(key.get_data());
		}
	};
}

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