#pragma once

#include "common/common.h"
#include "common/threading/lock_free.h"

#define BOOL_BUFFER_INT32_COUNT(element_count) (((element_count) + 31) / 32)

enum class e_buffer_type {
	k_real,
	k_bool,

	k_count
};

class ALIGNAS_LOCK_FREE c_buffer {
public:
	UNCOPYABLE(c_buffer);

	bool is_constant() const {
		return m_constant;
	}

	void set_constant(bool constant) {
		m_constant = constant;
	}

	template<typename t_data = void> t_data *get_data() {
		return reinterpret_cast<t_data *>(m_data);
	}

	template<typename t_data = void> const t_data *get_data() const {
		return reinterpret_cast<const t_data *>(m_data);
	}

private:
	c_buffer() {}

	friend class c_buffer_allocator;

	void *m_data;			// Pointer to the data
	bool m_constant;		// Whether this buffer consists of a single value across all elements
	uint32 m_pool_index;	// Which pool this buffer was allocated from
};

// Can be either:
// - Constant (single value)
// - Constant buffer (ignore all but first value)
// - Non-constant buffer
template<typename t_buffer, typename t_constant, typename t_constant_accessor>
class c_buffer_or_constant_base {
public:
	c_buffer_or_constant_base(t_buffer *buffer) {
		m_is_constant = false;
		m_buffer = buffer;
	}

	c_buffer_or_constant_base(t_constant constant) {
		m_is_constant = true;
		m_constant = constant;
	}

	// True if this is either a constant or a constant buffer
	bool is_constant() const {
		return m_is_constant ||
			m_buffer->is_constant();
	}

	bool is_buffer() const {
		return !m_is_constant;
	}

	bool is_constant_buffer() const {
		return !m_is_constant &&
			m_buffer->is_constant();
	}

	t_buffer *get_buffer() const {
		wl_assert(!m_is_constant);
		return m_buffer;
	}

	t_constant get_constant() const {
		if (m_is_constant) {
			return m_constant;
		} else {
			wl_assert(m_buffer->is_constant());
			return t_constant_accessor()(m_buffer);
		}
	}

private:
	bool m_is_constant;

	union {
		t_buffer *m_buffer;
		t_constant m_constant;
	};
};

struct s_constant_accessor_real {
	real32 operator()(const c_buffer *buffer) const {
		return *buffer->get_data<real32>();
	}
};

struct s_constant_accessor_bool {
	bool operator()(const c_buffer *buffer) const {
		return ((*buffer->get_data<uint32>()) & 1) != 0;
	}
};

// Buffer types: these are exactly the same as c_buffer and only exist for type identification/validation, i.e. the
// pointers (and even objects themselves) can be directly cast between each other with no problems
class c_real_buffer : public c_buffer {};
class c_bool_buffer : public c_buffer {};

static_assert(sizeof(c_real_buffer) == sizeof(c_buffer), "Buffer size mismatch");
static_assert(sizeof(c_bool_buffer) == sizeof(c_buffer), "Buffer size mismatch");

// Buffer-or-constant types
using c_real_buffer_or_constant = c_buffer_or_constant_base<c_buffer, real32, s_constant_accessor_real>;
using c_real_const_buffer_or_constant = c_buffer_or_constant_base<const c_buffer, real32, s_constant_accessor_real>;

using c_bool_buffer_or_constant = c_buffer_or_constant_base<c_buffer, bool, s_constant_accessor_bool>;
using c_bool_const_buffer_or_constant = c_buffer_or_constant_base<const c_buffer, bool, s_constant_accessor_bool>;

