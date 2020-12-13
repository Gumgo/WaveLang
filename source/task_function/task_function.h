#pragma once

#include "common/common.h"

#include "engine/buffer.h"

#include "native_module/native_module.h"

#include "task_function/task_data_type.h"

#include <variant>

// $TODO perhaps turn these into virtual interfaces?
class c_controller_interface;
class c_event_interface;
class c_voice_interface;

static constexpr size_t k_max_task_function_library_name_length = 64;
static constexpr size_t k_max_task_function_name_length = 64;
static constexpr size_t k_max_task_function_arguments = 10;
static constexpr uint32 k_invalid_task_argument_index = static_cast<uint32>(-1);

using f_library_engine_initializer = void *(*)();
using f_library_engine_deinitializer = void (*)(void *library_context);
using f_library_tasks_pre_initializer = void (*)(void *library_context);
using f_library_tasks_post_initializer = void (*)(void *library_context);

struct s_task_function_library {
	uint32 id;
	c_static_string<k_max_task_function_library_name_length> name;
	uint32 version;

	f_library_engine_initializer engine_initializer;
	f_library_engine_deinitializer engine_deinitializer;
	f_library_tasks_pre_initializer tasks_pre_initializer;
	f_library_tasks_post_initializer tasks_post_initializer;
};

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
// These are never used because we don't support output arrays, but these would be the data types:
//using c_real_buffer_array_out = c_wrapped_array<c_real_buffer *const>;
//using c_bool_buffer_array_out = c_wrapped_array<c_bool_buffer *const>;

// Compile-time constant array types
using c_real_constant_array = c_wrapped_array<const real32>;
using c_bool_constant_array = c_wrapped_array<const bool>;
using c_string_constant_array = c_wrapped_array<const char *const>;

struct s_task_function_runtime_argument {
	e_task_argument_direction argument_direction;
	c_task_qualified_data_type type;

	// Don't use this directly
	std::variant<
		real32,
		bool,
		const char *,
		c_buffer *, // Note: this is cast to the appropriate type
		c_real_constant_array,
		c_bool_constant_array,
		c_string_constant_array,
		c_buffer_array> value; // Note: this is cast to the appropriate type

	// These functions are used to enforce correct usage of input or output

	real32 get_real_constant_in() const {
		wl_assert(argument_direction == e_task_argument_direction::k_in);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_real));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_constant);
		return std::get<real32>(value);
	}

	bool get_bool_constant_in() const {
		wl_assert(argument_direction == e_task_argument_direction::k_in);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_bool));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_constant);
		return std::get<bool>(value);
	}

	const char *get_string_constant_in() const {
		wl_assert(argument_direction == e_task_argument_direction::k_in);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_string));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_constant);
		return std::get<const char *>(value);
	}

	const c_real_buffer *get_real_buffer_in() const {
		wl_assert(argument_direction == e_task_argument_direction::k_in);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_real));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_variable);
		return static_cast<const c_real_buffer *>(std::get<c_buffer *>(value));
	}

	c_real_buffer *get_real_buffer_out() const {
		wl_assert(argument_direction == e_task_argument_direction::k_out);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_real));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_variable);
		return static_cast<c_real_buffer *>(std::get<c_buffer *>(value));
	}

	const c_bool_buffer *get_bool_buffer_in() const {
		wl_assert(argument_direction == e_task_argument_direction::k_in);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_bool));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_variable);
		return static_cast<const c_bool_buffer *>(std::get<c_buffer *>(value));
	}

	c_bool_buffer *get_bool_buffer_out() const {
		wl_assert(argument_direction == e_task_argument_direction::k_out);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_bool));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_variable);
		return static_cast<c_bool_buffer *>(std::get<c_buffer *>(value));
	}

	c_real_constant_array get_real_constant_array_in() const {
		wl_assert(argument_direction == e_task_argument_direction::k_in);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_real, true));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_constant);
		return std::get<c_real_constant_array>(value);
	}

	c_bool_constant_array get_bool_constant_array_in() const {
		wl_assert(argument_direction == e_task_argument_direction::k_in);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_bool, true));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_constant);
		return std::get<c_bool_constant_array>(value);
	}

	c_string_constant_array get_string_constant_array_in() const {
		wl_assert(argument_direction == e_task_argument_direction::k_in);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_string, true));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_constant);
		return std::get<c_string_constant_array>(value);
	}

	c_real_buffer_array_in get_real_buffer_array_in() const {
		wl_assert(argument_direction == e_task_argument_direction::k_in);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_real, true));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_variable);
		const c_buffer_array &buffer_array = std::get<c_buffer_array>(value);
		return c_real_buffer_array_in(
			reinterpret_cast<const c_real_buffer *const *>(buffer_array.get_pointer()),
			buffer_array.get_count());
	}

	c_bool_buffer_array_in get_bool_buffer_array_in() const {
		wl_assert(argument_direction == e_task_argument_direction::k_in);
		wl_assert(type.get_data_type() == c_task_data_type(e_task_primitive_type::k_bool, true));
		wl_assert(type.get_data_mutability() == e_task_data_mutability::k_variable);
		const c_buffer_array &buffer_array = std::get<c_buffer_array>(value);
		return c_bool_buffer_array_in(
			reinterpret_cast<const c_bool_buffer *const *>(buffer_array.get_pointer()),
			buffer_array.get_count());
	}
};

using c_task_function_runtime_arguments = c_wrapped_array<const s_task_function_runtime_argument>;

struct s_task_function_context {
	c_event_interface *event_interface;
	c_voice_interface *voice_interface;
	c_controller_interface *controller_interface;

	uint32 sample_rate;
	uint32 buffer_size;
	void *library_context;
	void *task_memory;

	// $TODO add more fields for things like timing info

	c_task_function_runtime_arguments arguments;
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

struct s_task_function_argument {
	e_task_argument_direction argument_direction;
	c_task_qualified_data_type type;
	bool is_unshared;
};

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

	// List of arguments
	s_static_array<s_argument, k_max_task_function_arguments> arguments;

	// Unique identifier of the native module that this task maps to
	s_native_module_uid native_module_uid;

	// Mapping for each native module argument
	s_static_array<uint32, k_max_native_module_arguments> task_function_argument_indices;
};
