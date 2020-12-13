#pragma once

#include "common/common.h"
#include "common/math/math.h"
#include "common/threading/lock_free.h"

#include "task_function/task_data_type.h"

enum class e_buffer_data_state {
	k_dynamic,					// The buffer contains dynamic data
	k_constant,					// The buffer contains a single value at the beginning
	k_compile_time_constant,	// The buffer contains a compile-time constant which cannot change

	k_count
};

class c_buffer {
public:
	UNCOPYABLE(c_buffer);

	c_buffer(c_buffer &&other)
		: m_data_type(other.m_data_type)
		, m_data_state(other.m_data_state) {
		if (m_data_state == e_buffer_data_state::k_compile_time_constant) {
			m_pointer = m_compile_time_constant;
			memcpy(m_compile_time_constant, other.m_compile_time_constant, sizeof(m_compile_time_constant));
		} else {
			m_pointer = other.m_pointer;
		}
	}

	c_buffer &operator=(c_buffer &&other) {
		m_data_type = other.m_data_type;
		m_data_state = other.m_data_state;
		if (m_data_state == e_buffer_data_state::k_compile_time_constant) {
			m_pointer = m_compile_time_constant;
			memcpy(m_compile_time_constant, other.m_compile_time_constant, sizeof(m_compile_time_constant));
		} else {
			m_pointer = other.m_pointer;
		}

		return *this;
	}

	static c_buffer construct(c_task_data_type data_type) {
		wl_assert(data_type.is_valid());
		wl_assert(!data_type.is_array());
		wl_assert(!data_type.get_primitive_type_traits().constant_only);
		c_buffer buffer(data_type);
		return buffer;
	}

	template<typename t_element, e_task_primitive_type k_task_primitive_type, typename t_constant_accessor>
	static c_buffer construct_compile_time_constant_internal(c_task_data_type data_type, t_element constant) {
		wl_assert(data_type.get_primitive_type() == k_task_primitive_type);
		c_buffer buffer = construct(data_type);
		buffer.m_data_state = e_buffer_data_state::k_compile_time_constant;
		buffer.m_pointer = buffer.m_compile_time_constant;
		t_constant_accessor::set(buffer.m_pointer, constant);
		return buffer;
	}

	c_task_data_type get_data_type() const {
		return m_data_type;
	}

	void *get_data_untyped() {
		return m_pointer;
	}

	const void *get_data_untyped() const {
		return m_pointer;
	}

	bool is_constant() const {
		return m_data_state != e_buffer_data_state::k_dynamic;
	}

	bool is_compile_time_constant() const {
		return m_data_state == e_buffer_data_state::k_compile_time_constant;
	}

	void set_memory(void *data) {
		wl_assert(m_data_state != e_buffer_data_state::k_compile_time_constant);
		m_data_state = e_buffer_data_state::k_dynamic;
		m_pointer = data;
	}

	// Be careful with calling set_is_constant(true). A constant buffer must contain an entire SIMD block worth of
	// constant data, so it is invalid to call set_is_constant(true) after only assigning a single value. Prefer calling
	// extend_constant(), which ensures that the first element is copied across the entire SIMD block.
	void set_is_constant(bool constant) {
		wl_assert(m_data_state != e_buffer_data_state::k_compile_time_constant);
		m_data_state = constant ? e_buffer_data_state::k_constant : e_buffer_data_state::k_dynamic;
	}

	template<typename t_buffer> t_buffer &get_as() {
		wl_assert(m_data_type.get_primitive_type() == t_buffer::k_primitive_type);
		return static_cast<t_buffer &>(*this);
	}

	template<typename t_buffer> const t_buffer &get_as() const {
		wl_assert(m_data_type.get_primitive_type() == t_buffer::k_primitive_type);
		return static_cast<const t_buffer &>(*this);
	}

protected:
	c_buffer(c_task_data_type data_type)
		: m_data_type(data_type) {}

	c_task_data_type m_data_type = c_task_data_type::invalid();
	e_buffer_data_state m_data_state = e_buffer_data_state::k_dynamic;

	// Pointer to the data. If this is a compile-time constant, this points to m_compile_time_constant.
	void *m_pointer = nullptr;
	ALIGNAS_SIMD uint8 m_compile_time_constant[k_simd_size];
};

// Typed buffers act as interfaces only. c_buffer can be safely cast to and from these types.

template<typename t_element, typename t_data, e_task_primitive_type k_task_primitive_type, typename t_constant_accessor>
class c_typed_buffer : public c_buffer {
public:
	static constexpr e_task_primitive_type k_primitive_type = k_task_primitive_type;

	c_typed_buffer() = delete;

	static c_buffer construct_compile_time_constant(c_task_data_type data_type, t_element constant) {
		return construct_compile_time_constant_internal<t_element, k_primitive_type, t_constant_accessor>(
			data_type,
			constant);
	}

	t_data *get_data() {
		return static_cast<t_data *>(m_pointer);
	}

	const t_data *get_data() const {
		return static_cast<const t_data *>(m_pointer);
	}

	t_element get_constant() const {
		wl_assert(m_data_state != e_buffer_data_state::k_dynamic);
		return t_constant_accessor::get(m_pointer);
	}

	// Assigns the value provided to the first SIMD block and sets the buffer state to constant
	void assign_constant(t_element constant) {
		wl_assert(m_data_state != e_buffer_data_state::k_compile_time_constant);
		m_data_state = e_buffer_data_state::k_constant;
		t_constant_accessor::set(m_pointer, constant);
	}

	// Copies the first element into the first SIMD block and sets the buffer state to constant
	void extend_constant() {
		assign_constant(t_constant_accessor::get(m_pointer));
	}
};

struct s_constant_accessor_real {
	static real32 get(const void *data) {
		return *static_cast<const real32 *>(data); // Grab the 0th element
	}

	static void set(void *data, real32 value) {
		real32xN(value).store(static_cast<real32 *>(data));
	}
};

struct s_constant_accessor_bool {
	static bool get(const void *data) {
		return (*static_cast<const int32 *>(data) & 1) != 0; // Grab the 0th bit of the 0th element
	}

	static void set(void *data, bool value) {
		// Duplicate the value across all 128 bits
		int32xN(-static_cast<int32>(value)).store(static_cast<int32 *>(data));
	}
};

class c_real_buffer : public c_typed_buffer<real32, real32, e_task_primitive_type::k_real, s_constant_accessor_real> {};
class c_bool_buffer : public c_typed_buffer<bool, int32, e_task_primitive_type::k_bool, s_constant_accessor_bool> {};

constexpr size_t bool_buffer_int32_count(size_t element_count) {
	return (element_count + 31) / 32;
}
