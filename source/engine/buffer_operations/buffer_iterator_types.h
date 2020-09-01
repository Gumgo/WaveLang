#pragma once

#include "engine/buffer.h"
#include "engine/math/math.h"

namespace buffer_iterator_internal {
	enum class e_buffer_iterator_type {
		k_constant,
		k_input,
		k_output
	};

	template<size_t k_stride, e_buffer_iterator_type k_iterator_type, typename t_buffer>
	class c_buffer_iterator; // Unspecialized iterator type cannot be instantiated

	// Base class to implement no-op functions for constant iterators
	class c_constant_buffer_iterator_base {
	public:
		void advance() {}
		void iteration_finished() {}
		void set_is_constant(bool constant) {}
	};

	// Base class to implement no-op functions for input iterators
	class c_input_buffer_iterator_base {
	public:
		void iteration_finished() {}
		void set_is_constant(bool constant) {}
	};

	// Real iterators

	template<>
	class c_buffer_iterator<1, e_buffer_iterator_type::k_constant, const c_real_buffer *>
		: public c_constant_buffer_iterator_base {
	public:
		c_buffer_iterator(const c_real_buffer *buffer) : m_constant(buffer->get_constant()) {}
		real32 get() const { return m_constant; }

	private:
		real32 m_constant;
	};

	template<>
	class c_buffer_iterator<1, e_buffer_iterator_type::k_input, const c_real_buffer *>
		: public c_input_buffer_iterator_base {
	public:
		c_buffer_iterator(const c_real_buffer *buffer) : m_pointer(buffer->get_data()) {}
		real32 get() const { return *m_pointer; }
		void advance() { m_pointer++; }

	private:
		const real32 *m_pointer;
	};

	template<>
	class c_buffer_iterator<1, e_buffer_iterator_type::k_output, c_real_buffer *> {
	public:
		c_buffer_iterator(c_real_buffer *buffer)
			: m_buffer(buffer)
			, m_pointer(buffer->get_data()) {}

		real32 &get() { return *m_pointer; }
		void advance() { m_pointer++; }
		void iteration_finished() {}

		void set_is_constant(bool constant) {
			if (constant) {
				m_buffer->extend_constant();
			} else {
				m_buffer->set_is_constant(false);
			}
		}

	private:
		c_real_buffer *m_buffer;
		real32 *m_pointer;
	};

	template<>
	class c_buffer_iterator<4, e_buffer_iterator_type::k_constant, const c_real_buffer *>
		: public c_constant_buffer_iterator_base {
	public:
		c_buffer_iterator(const c_real_buffer *buffer) : m_constant(buffer->get_constant()) {}
		const real32x4 &get() const { return m_constant; }

	private:
		real32x4 m_constant;
	};

	template<>
	class c_buffer_iterator<4, e_buffer_iterator_type::k_input, const c_real_buffer *>
		: public c_input_buffer_iterator_base {
	public:
		c_buffer_iterator(const c_real_buffer *buffer) : m_pointer(buffer->get_data()) {}
		real32x4 get() const { return real32x4(m_pointer); }
		void advance() { m_pointer += 4; }

	private:
		const real32 *m_pointer;
	};

	template<>
	class c_buffer_iterator<4, e_buffer_iterator_type::k_output, c_real_buffer *> {
	public:
		c_buffer_iterator(c_real_buffer *buffer)
			: m_buffer(buffer)
			, m_pointer(buffer->get_data()) {}

		real32x4 &get() {
			return m_value;
		}

		void advance() {
			m_value.store(m_pointer);
			m_pointer += 4;
		}

		void iteration_finished() {}

		void set_is_constant(bool constant) {
			if (constant) {
				m_buffer->extend_constant();
			} else {
				m_buffer->set_is_constant(false);
			}
		}

	private:
		c_real_buffer *m_buffer;
		real32 *m_pointer;
		real32x4 m_value;
	};

	// Bool iterators

	template<>
	class c_buffer_iterator<1, e_buffer_iterator_type::k_constant, const c_bool_buffer *>
		: public c_constant_buffer_iterator_base {
	public:
		c_buffer_iterator(const c_bool_buffer *buffer) : m_constant(buffer->get_constant()) {}
		bool get() const { return m_constant; }

	private:
		bool m_constant;
	};

	template<>
	class c_buffer_iterator<1, e_buffer_iterator_type::k_input, const c_bool_buffer *>
		: public c_input_buffer_iterator_base {
	public:
		c_buffer_iterator(const c_bool_buffer *buffer)
			: m_pointer(buffer->get_data())
			, m_bit(0) {}

		bool get() const {
			return (*m_pointer & (1 << m_bit)) != 0;
		}

		void advance() {
			m_bit = (m_bit & 31) + 1;
			m_pointer += (m_bit == 0);
			m_bit = m_bit & 31;
		}

	private:
		const int32 *m_pointer;
		uint32 m_bit;
	};

	template<>
	class c_buffer_iterator<1, e_buffer_iterator_type::k_output, c_bool_buffer *> {
	public:
		c_buffer_iterator(c_bool_buffer *buffer)
			: m_buffer(buffer)
			, m_pointer(buffer->get_data())
			, m_bit(0)
			, m_cached_value(0) {}

		bool &get() {
			return m_value;
		}

		void advance() {
			m_cached_value |= static_cast<int32>(m_value) << m_bit;
			m_bit = (m_bit + 1) & 31;
			if (m_bit == 0) {
				*m_pointer = m_cached_value;
				m_pointer++;
				m_cached_value = 0;
			}
		}

		void iteration_finished() {
			if (m_bit != 0) {
				*m_pointer = m_cached_value;
			}
		}

		void set_is_constant(bool constant) {
			if (constant) {
				m_buffer->extend_constant();
			} else {
				m_buffer->set_is_constant(false);
			}
		}

	private:
		c_bool_buffer *m_buffer;
		int32 *m_pointer;
		uint32 m_bit;

		bool m_value;
		int32 m_cached_value;
	};

	template<>
	class c_buffer_iterator<4, e_buffer_iterator_type::k_constant, const c_bool_buffer *>
		: public c_constant_buffer_iterator_base {
	public:
		c_buffer_iterator(const c_bool_buffer *buffer) : m_constant(-static_cast<int32>(buffer->get_constant())) {}
		const int32x4 &get() const { return m_constant; }

	private:
		int32x4 m_constant;
	};

	template<>
	class c_buffer_iterator<4, e_buffer_iterator_type::k_input, const c_bool_buffer *>
		: public c_input_buffer_iterator_base {
	public:
		c_buffer_iterator(const c_bool_buffer *buffer)
			: m_pointer(buffer->get_data())
			, m_bit(0) {}

		int32x4 get() const {
			// Duplicate 4 contiguous bits ABCD into elements [AAAA..., BBBB..., CCCC..., DDDD...]
			int32x4 value(static_cast<int32>(static_cast<uint32>(*m_pointer) >> m_bit) & 15);
			// Shift the relevant bit into the LSB position, mask out all the other bits, and negate to extend a 1 to all bits
			return -((value >> int32x4(0, 1, 2, 3)) & int32x4(1));
		}

		void advance() {
			m_bit = (m_bit & 31) + 4;
			m_pointer += (m_bit == 0);
			m_bit = m_bit & 31;
		}

	private:
		const int32 *m_pointer;
		uint32 m_bit;
	};

	template<>
	class c_buffer_iterator<4, e_buffer_iterator_type::k_output, c_bool_buffer *> {
	public:
		c_buffer_iterator(c_bool_buffer *buffer)
			: m_buffer(buffer)
			, m_pointer(buffer->get_data())
			, m_bit(0)
			, m_cached_value(0) {}

		int32x4 &get() {
			return m_value;
		}

		void advance() {
			// Pack LSB from each element into a contiguous block and shift to the proper offset
			int32 value = ((m_value & int32x4(1)) << int32x4(0, 1, 2, 3)).sum_elements().first_element();
			m_cached_value |= value << m_bit;
			m_bit = (m_bit + 4) & 31;
			if (m_bit == 0) {
				*m_pointer = m_cached_value;
				m_pointer++;
				m_cached_value = 0;
			}
		}

		void iteration_finished() {
			if (m_bit != 0) {
				*m_pointer = m_cached_value;
			}
		}

		void set_is_constant(bool constant) {
			if (constant) {
				m_buffer->extend_constant();
			} else {
				m_buffer->set_is_constant(false);
			}
		}

	private:
		c_bool_buffer *m_buffer;
		int32 *m_pointer;
		uint32 m_bit;

		int32x4 m_value;
		int32 m_cached_value;
	};

	template<>
	class c_buffer_iterator<128, e_buffer_iterator_type::k_constant, const c_bool_buffer *>
		: public c_constant_buffer_iterator_base {
	public:
		c_buffer_iterator(const c_bool_buffer *buffer) : m_constant(-static_cast<int32>(buffer->get_constant())) {}
		const int32x4 &get() const { return m_constant; }

	private:
		int32x4 m_constant;
	};

	template<>
	class c_buffer_iterator<128, e_buffer_iterator_type::k_input, const c_bool_buffer *>
		: public c_input_buffer_iterator_base {
	public:
		c_buffer_iterator(const c_bool_buffer *buffer) : m_pointer(buffer->get_data()) {}
		int32x4 get() const { return int32x4(m_pointer); }
		void advance() { m_pointer += 4; }

	private:
		const int32 *m_pointer;
	};

	template<>
	class c_buffer_iterator<128, e_buffer_iterator_type::k_output, c_bool_buffer *> {
	public:
		c_buffer_iterator(c_bool_buffer *buffer)
			: m_buffer(buffer)
			, m_pointer(buffer->get_data()) {}

		int32x4 &get() {
			return m_value;
		}

		void advance() {
			m_value.store(m_pointer);
			m_pointer += 4;
		}

		void iteration_finished() {}

		void set_is_constant(bool constant) {
			if (constant) {
				m_buffer->extend_constant();
			} else {
				m_buffer->set_is_constant(false);
			}
		}

	private:
		c_bool_buffer *m_buffer;
		int32 *m_pointer;
		int32x4 m_value;
	};
}
