#ifndef WAVELANG_BUFFER_H__
#define WAVELANG_BUFFER_H__

#include "common/common.h"
#include "common/threading/lock_free.h"

// $TODO bool buffers?
enum e_buffer_type {
	k_buffer_type_real,

	k_buffer_type_count
};

class ALIGNAS_LOCK_FREE c_buffer : private c_uncopyable {
public:
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

	void *m_data;		// Pointer to the data
	bool m_constant;	// Whether this buffer consists of a single value across all elements
};

// Can be either:
// - Constant (single value)
// - Constant buffer (ignore all but first value)
// - Non-constant buffer
template<typename t_buffer, typename t_constant>
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
			return *m_buffer->get_data<t_constant>();
		}
	}

private:
	bool m_is_constant;

	union {
		t_buffer *m_buffer;
		t_constant m_constant;
	};
};

typedef c_buffer_or_constant_base<c_buffer, real32> c_real_buffer_or_constant;
typedef c_buffer_or_constant_base<const c_buffer, real32> c_real_const_buffer_or_constant;

#endif // WAVELANG_BUFFER_H__
