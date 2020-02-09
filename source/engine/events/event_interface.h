#pragma once

#include "common/common.h"

class c_async_event_handler;

const size_t k_max_event_string_length = 256;
typedef c_static_string<k_max_event_string_length> c_event_string;

enum class e_event_level {
	k_invalid = -1,
	k_verbose,
	k_message,
	k_warning,
	k_error,
	k_critical,

	k_count
};

class c_event_data_stack {
public:
	static const size_t k_event_data_stack_size = 256;

	c_event_data_stack() {
		m_event_size = 0;
	}

	bool has_space(size_t desired_space) const {
		return (m_event_size + desired_space) <= m_event_data.get_count();
	}

	template<typename t_value> bool try_append(const t_value &value) {
		return try_append(sizeof(value), &value);
	}

	bool try_append(size_t size, const void *value) {
		if (!has_space(size)) {
			return false;
		}

		memcpy(m_event_data.get_elements() + m_event_size, value, size);
		m_event_size += size;
		return true;
	}

	c_wrapped_array<const uint8> get_event_data() const {
		return c_wrapped_array<const uint8>(m_event_data.get_elements(), m_event_size);
	}

private:
	void save_state() {
		m_state = m_event_size;
	}

	void restore_state() {
		m_event_size = m_state;
	}

	friend class c_event;
	size_t m_event_size;
	size_t m_state; // Used to restore old size in case we run out of space while pushing event data
	s_static_array<uint8, k_event_data_stack_size> m_event_data;
};

template<typename t_value>
struct s_event_data_type_definition {
	// Specialize this class and write the following functions:
	static uint8 id;
	static bool build_event(c_event_data_stack &event_data_stack, t_value value);
	static size_t build_event_string(c_event_string &event_string, const void *data_pointer);
};

class c_event {
public:
	c_event(e_event_level event_level) {
		wl_assert(valid_enum_index(event_level));
		IF_ASSERTS_ENABLED(bool success = ) m_event_data_stack.try_append(static_cast<uint8>(event_level));
		wl_assert(success);
	}

	template<typename t_value> c_event &operator<<(t_value value) {
		m_event_data_stack.save_state();
		if (m_event_data_stack.try_append(s_event_data_type_definition<t_value>::id)) {
			if (!s_event_data_type_definition<t_value>::build_event(m_event_data_stack, value)) {
				m_event_data_stack.restore_state();
			}
		}
		return *this;
	}

	c_wrapped_array<const uint8> get_event_data() const {
		return m_event_data_stack.get_event_data();
	}

	static size_t calculate_max_event_size() {
		return c_event_data_stack::k_event_data_stack_size;
	}

private:
	c_event_data_stack m_event_data_stack;
};

typedef size_t (*f_build_event_string)(c_event_string &event_string, const void *data_pointer);

uint8 register_event_data_type_internal(f_build_event_string build_event_string);

template<typename t_value> void register_event_data_type() {
	uint8 id = register_event_data_type_internal(s_event_data_type_definition<t_value>::build_event_string);
	s_event_data_type_definition<t_value>::id = id;
}

class c_event_interface {
public:
	// Pass null to disable events
	void initialize(c_async_event_handler *event_handler);
	void submit(const c_event &e);

	// Constructs an event string from event data
	static e_event_level build_event_string(
		size_t event_size, const void *event_data, c_event_string &out_event_string);

private:
	c_async_event_handler *m_event_handler;
};

// Macros for convenience. Example usage:
// EVENT_ERROR << "There was an error"
#define EVENT_VERBOSE	c_event(e_event_level::k_verbose)
#define EVENT_MESSAGE	c_event(e_event_level::k_message)
#define EVENT_WARNING	c_event(e_event_level::k_warning)
#define EVENT_ERROR		c_event(e_event_level::k_error)
#define EVENT_CRITICAL	c_event(e_event_level::k_critical)

