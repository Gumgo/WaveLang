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

#endif // WAVELANG_BUFFER_H__