#pragma once

#include "common/common.h"

#include "engine/buffer.h"
#include "engine/task_data_type.h"

#include "execution_graph/native_module.h"

class c_controller_interface;
class c_event_interface;
class c_sample_library_accessor;
class c_sample_library_requester;
class c_voice_interface;

// Unique identifier for each task function
struct s_task_function_uid {
	// Unique identifier consists of 8 bytes
	// First 4 bytes is "library_id", stored in big endian order. IDs 0-255 are reserved for core libraries.
	// Last 4 bytes is "task_function_id", stored in big endian order. Used to identify a task function within the
	// library.
	union {
		uint8 data[8];
		uint64 data_uint64;
		struct {
			uint32 library_id;
			uint32 task_function_id;
		};
	};

	bool operator==(const s_task_function_uid &other) const {
		return data_uint64 == other.data_uint64;
	}

	bool operator!=(const s_task_function_uid &other) const {
		return data_uint64 != other.data_uint64;
	}

	uint32 get_library_id() const {
		return big_to_native_endian(library_id);
	}

	uint32 get_task_function_id() const {
		return big_to_native_endian(task_function_id);
	}

	static s_task_function_uid build(uint32 library_id, uint32 task_function_id) {
		s_task_function_uid result;
		result.library_id = native_to_big_endian(library_id);
		result.task_function_id = native_to_big_endian(task_function_id);
		return result;
	}

	static const s_task_function_uid k_invalid;
};

// Array types
using c_buffer_array = c_wrapped_array<c_buffer *const>;
using c_real_buffer_array_in = c_wrapped_array<const c_real_buffer *const>;
using c_bool_buffer_array_in = c_wrapped_array<const c_bool_buffer *const>;
using c_real_buffer_array_out = c_wrapped_array<c_real_buffer *const>;
using c_bool_buffer_array_out = c_wrapped_array<c_bool_buffer *const>;

// Compile-time constant array types
using c_real_constant_array = c_wrapped_array<const real32>;
using c_bool_constant_array = c_wrapped_array<const bool>;
using c_string_constant_array = c_wrapped_array<const char *const>;

struct s_task_function_argument {
	// Do not access these directly
	struct {
		c_task_qualified_data_type type;

		union u_value {
			u_value() {} // Allows for c_wrapped_array

			c_buffer *buffer; // Note: this is cast to the appropriate type
			c_buffer_array buffer_array; // Note: this is cast to the appropriate type

			real32 real_constant;
			c_real_constant_array real_constant_array;

			bool bool_constant;
			c_bool_constant_array bool_constant_array;

			const char *string_constant;
			c_string_constant_array string_constant_array;
		} value;
	} data;

	const c_real_buffer *get_real_buffer_in() const {
		wl_assert(validate(e_task_primitive_type::k_real, false, e_task_qualifier::k_in));
		return static_cast<const c_real_buffer *>(data.value.buffer);
	}

	c_real_buffer *get_real_buffer_out() const {
		wl_assert(validate(e_task_primitive_type::k_real, false, e_task_qualifier::k_out));
		return static_cast<c_real_buffer *>(data.value.buffer);
	}

	real32 get_real_constant_in() const {
		wl_assert(validate(e_task_primitive_type::k_real, false, e_task_qualifier::k_constant));
		return data.value.real_constant;
	}

	c_real_buffer_array_in get_real_buffer_array_in() const {
		wl_assert(validate(e_task_primitive_type::k_real, true, e_task_qualifier::k_in));
		return c_real_buffer_array_in(
			reinterpret_cast<const c_real_buffer *const *>(data.value.buffer_array.get_pointer()),
			data.value.buffer_array.get_count());
	}

	c_real_buffer_array_out get_real_buffer_array_out() const {
		wl_assert(validate(e_task_primitive_type::k_real, true, e_task_qualifier::k_out));
		return c_real_buffer_array_out(
			reinterpret_cast<c_real_buffer *const *>(data.value.buffer_array.get_pointer()),
			data.value.buffer_array.get_count());
	}

	c_real_constant_array get_real_constant_array_in() const {
		wl_assert(validate(e_task_primitive_type::k_real, true, e_task_qualifier::k_constant));
		return data.value.real_constant_array;
	}

	const c_bool_buffer *get_bool_buffer_in() const {
		wl_assert(validate(e_task_primitive_type::k_bool, false, e_task_qualifier::k_in));
		return static_cast<const c_bool_buffer *>(data.value.buffer);
	}

	c_bool_buffer *get_bool_buffer_out() const {
		wl_assert(validate(e_task_primitive_type::k_bool, false, e_task_qualifier::k_out));
		return static_cast<c_bool_buffer *>(data.value.buffer);
	}

	bool get_bool_constant_in() const {
		wl_assert(validate(e_task_primitive_type::k_bool, false, e_task_qualifier::k_constant));
		return data.value.bool_constant;
	}

	c_bool_buffer_array_in get_bool_buffer_array_in() const {
		wl_assert(validate(e_task_primitive_type::k_bool, true, e_task_qualifier::k_in));
		return c_bool_buffer_array_in(
			reinterpret_cast<const c_bool_buffer *const *>(data.value.buffer_array.get_pointer()),
			data.value.buffer_array.get_count());
	}

	c_bool_buffer_array_out get_bool_buffer_array_out() const {
		wl_assert(validate(e_task_primitive_type::k_bool, true, e_task_qualifier::k_out));
		return c_bool_buffer_array_out(
			reinterpret_cast<c_bool_buffer *const *>(data.value.buffer_array.get_pointer()),
			data.value.buffer_array.get_count());
	}

	c_bool_constant_array get_bool_constant_array_in() const {
		wl_assert(validate(e_task_primitive_type::k_bool, true, e_task_qualifier::k_constant));
		return data.value.bool_constant_array;
	}

	const char *get_string_constant_in() const {
		wl_assert(validate(e_task_primitive_type::k_string, false, e_task_qualifier::k_constant));
		return data.value.string_constant;
	}

	c_string_constant_array get_string_constant_array_in() const {
		wl_assert(validate(e_task_primitive_type::k_string, true, e_task_qualifier::k_constant));
		return data.value.string_constant_array;
	}

#if ASSERTS_ENABLED
	bool validate(e_task_primitive_type primitive_type, bool is_array, e_task_qualifier qualifier) const {
		return data.type.get_data_type() == c_task_data_type(primitive_type, is_array)
			&& data.type.get_qualifier() == qualifier;
	}
#endif // ASSERTS_ENABLED
};

using c_task_function_arguments = c_wrapped_array<const s_task_function_argument>;

struct s_task_function_context {
	c_event_interface *event_interface;
	c_sample_library_accessor *sample_accessor;
	c_sample_library_requester *sample_requester;
	c_voice_interface *voice_interface;
	c_controller_interface *controller_interface;

	uint32 sample_rate;
	uint32 buffer_size;
	void *task_memory;

	// $TODO add more fields for things like timing info

	c_task_function_arguments arguments;
};

// This function takes a partially-filled-in context and returns the amount of memory the task requires
// Basic data such as sample rate is available, as well as any constant arguments - any buffer arguments are null
using f_task_memory_query = size_t (*)(const s_task_function_context &context);

// Called after task memory has been allocated - should be used to initialize this memory
// Basic data such as sample rate is available, as well as any constant arguments - any buffer arguments are null
// In addition, this is the time where samples should be requested, as that interface is provided on the context
using f_task_initializer = void (*)(const s_task_function_context &context);

// Called when a voice becomes active
using f_task_voice_initializer = void (*)(const s_task_function_context &context);

// Function executed for the task
using f_task_function = void (*)(const s_task_function_context &context);

static constexpr size_t k_max_task_function_name_length = 64;
static constexpr size_t k_max_task_function_arguments = 10;
static constexpr uint32 k_invalid_task_argument_index = static_cast<uint32>(-1);

struct s_task_function {
	// Unique identifier for this task function
	s_task_function_uid uid;

	// Name of the task function
	c_static_string<k_max_task_function_name_length> name;

	// Memory query function, or null
	f_task_memory_query memory_query;

	// Function for initializing task memory
	f_task_initializer initializer;

	// Function for initializing task memory when a voice becomes active
	f_task_voice_initializer voice_initializer;

	// Function to execute
	f_task_function function;

	// Number of arguments
	uint32 argument_count;

	// Type of each argument
	s_static_array<c_task_qualified_data_type, k_max_task_function_arguments> argument_types;

	// Whether each argument is unshared
	s_static_array<bool, k_max_task_function_arguments> arguments_unshared;

	// Unique identifier of the native module that this task maps to
	s_native_module_uid native_module_uid;

	// Mapping for each native module argument
	s_static_array<uint32, k_max_native_module_arguments> task_function_argument_indices;
};
