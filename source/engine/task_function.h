#ifndef WAVELANG_ENGINE_TASK_FUNCTION_H__
#define WAVELANG_ENGINE_TASK_FUNCTION_H__

#include "common/common.h"

#include "engine/buffer.h"

#include "execution_graph/native_module.h"

class c_array_dereference_interface;
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

enum e_task_qualifier {
	k_task_qualifier_in,
	k_task_qualifier_out,
	k_task_qualifier_inout,

	k_task_qualifier_count
};

enum e_task_primitive_type {
	k_task_primitive_type_real,
	k_task_primitive_type_bool,
	k_task_primitive_type_string,

	k_task_primitive_type_count
};

struct s_task_primitive_type_traits {
	bool is_dynamic;	// Whether this is a runtime-dynamic type
};

class c_task_data_type {
public:
	c_task_data_type();
	c_task_data_type(e_task_primitive_type primitive_type, bool is_array = false);
	static c_task_data_type invalid();

	bool is_valid() const;

	e_task_primitive_type get_primitive_type() const;
	const s_task_primitive_type_traits &get_primitive_type_traits() const;
	bool is_array() const;
	c_task_data_type get_element_type() const;
	c_task_data_type get_array_type() const;

	bool operator==(const c_task_data_type &other) const;
	bool operator!=(const c_task_data_type &other) const;

private:
	enum e_flag {
		k_flag_is_array,

		k_flag_count
	};

	e_task_primitive_type m_primitive_type;
	uint32 m_flags;
};

class c_task_qualified_data_type {
public:
	c_task_qualified_data_type();
	c_task_qualified_data_type(c_task_data_type data_type, e_task_qualifier qualifier);
	static c_task_qualified_data_type invalid();

	bool is_valid() const;
	c_task_data_type get_data_type() const;
	e_task_qualifier get_qualifier() const;

	bool operator==(const c_task_qualified_data_type &other) const;
	bool operator!=(const c_task_qualified_data_type &other) const;

	bool is_legal() const;

private:
	c_task_data_type m_data_type;
	e_task_qualifier m_qualifier;
};

const s_task_primitive_type_traits &get_task_primitive_type_traits(e_task_primitive_type primitive_type);

struct s_real_array_element {
	bool is_constant;

	union {
		real32 constant_value;
		uint32 buffer_index_value;
	};
};

struct s_bool_array_element {
	bool is_constant;

	union {
		bool constant_value;
		uint32 buffer_index_value;
	};
};

struct s_string_array_element {
	const char *constant_value;
};

typedef c_wrapped_array_const<s_real_array_element> c_real_array;
typedef c_wrapped_array_const<s_bool_array_element> c_bool_array;
typedef c_wrapped_array_const<s_string_array_element> c_string_array;

struct s_task_function_argument {
	// Do not access these directly
	struct {
#if IS_TRUE(ASSERTS_ENABLED)
		c_task_qualified_data_type type;
#endif // IS_TRUE(ASSERTS_ENABLED)
		bool is_constant;

		union u_value {
			u_value() {} // Allows for c_wrapped_array

			c_buffer *buffer; // Shared accessor for any buffer, no accessor function for this

			const c_real_buffer *real_buffer_in;
			c_real_buffer *real_buffer_out;
			c_real_buffer *real_buffer_inout;
			real32 real_constant_in;
			c_real_array real_array_in;

			const c_bool_buffer *bool_buffer_in;
			c_bool_buffer *bool_buffer_out;
			c_bool_buffer *bool_buffer_inout;
			bool bool_constant_in;
			c_bool_array bool_array_in;

			const char *string_constant_in;
			c_string_array string_array_in;
		} value;
	} data;

	// For arrays, the array itself is always constant so if this is true, it means all elements are constants
	bool is_constant() const {
		return data.is_constant;
	}

	const c_real_buffer *get_real_buffer_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		wl_assert(!is_constant());
		return data.value.real_buffer_in;
	}

	c_real_buffer *get_real_buffer_out() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_out);
		wl_assert(!is_constant());
		return data.value.real_buffer_out;
	}

	c_real_buffer *get_real_buffer_inout() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_inout);
		wl_assert(!is_constant());
		return data.value.real_buffer_inout;
	}

	real32 get_real_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		wl_assert(is_constant());
		return data.value.real_constant_in;
	}

	c_real_const_buffer_or_constant get_real_buffer_or_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);

		if (data.is_constant) {
			return c_real_const_buffer_or_constant(data.value.real_constant_in);
		} else {
			return c_real_const_buffer_or_constant(data.value.real_buffer_in);
		}
	}

	c_real_array get_real_array_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_real, true));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		return data.value.real_array_in;
	}

	const c_bool_buffer *get_bool_buffer_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		wl_assert(!is_constant());
		return data.value.bool_buffer_in;
	}

	c_bool_buffer *get_bool_buffer_out() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_out);
		wl_assert(!is_constant());
		return data.value.bool_buffer_out;
	}

	c_bool_buffer *get_bool_buffer_inout() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_inout);
		wl_assert(!is_constant());
		return data.value.bool_buffer_inout;
	}

	bool get_bool_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		wl_assert(is_constant());
		return data.value.bool_constant_in;
	}

	c_bool_const_buffer_or_constant get_bool_buffer_or_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);

		if (data.is_constant) {
			return c_bool_const_buffer_or_constant(data.value.bool_constant_in);
		} else {
			return c_bool_const_buffer_or_constant(data.value.bool_buffer_in);
		}
	}

	c_bool_array get_bool_array_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_bool, true));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		return data.value.bool_array_in;
	}

	const char *get_string_constant_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_string));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		wl_assert(is_constant());
		return data.value.string_constant_in;
	}

	c_string_array get_string_array_in() const {
		wl_assert(data.type.get_data_type() == c_task_data_type(k_task_primitive_type_string, true));
		wl_assert(data.type.get_qualifier() == k_task_qualifier_in);
		return data.value.string_array_in;
	}
};

typedef c_wrapped_array_const<s_task_function_argument> c_task_function_arguments;

struct s_task_function_context {
	c_array_dereference_interface *array_dereference_interface;
	c_event_interface *event_interface;
	c_sample_library_accessor *sample_accessor;
	c_sample_library_requester *sample_requester;
	c_voice_interface *voice_interface;
	c_controller_interface *controller_interface;

	uint32 sample_rate;
	uint32 buffer_size;
	void *task_memory;

	// $TODO more things like timing

	c_task_function_arguments arguments;
};

// This function takes a partially-filled-in context and returns the amount of memory the task requires
// Basic data such as sample rate is available, as well as any constant arguments - any buffer arguments are null
typedef size_t (*f_task_memory_query)(const s_task_function_context &context);

// Called after task memory has been allocated - should be used to initialize this memory
// Basic data such as sample rate is available, as well as any constant arguments - any buffer arguments are null
// In addition, this is the time where samples should be requested, as that interface is provided on the context
typedef void (*f_task_initializer)(const s_task_function_context &context);

// Called when a voice becomes active
typedef void (*f_task_voice_initializer)(const s_task_function_context &context);

// Function executed for the task
typedef void (*f_task_function)(const s_task_function_context &context);

static const size_t k_max_task_function_name_length = 64;
static const size_t k_max_task_function_arguments = 10;

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
};

// Task function mappings

// Native modules must be convered into tasks. A single native module can potentially be converted into a number of
// different tasks depending on its inputs. Once a task is selected, the inputs and outputs of the native module must be
// mapped to the inputs/outputs of the task.

// The following describe the possible types of input arguments for a native module
enum e_task_function_mapping_native_module_input_type {
	// The input is a variable
	k_task_function_mapping_native_module_input_type_variable,

	// The input is a variable which is not used as any other input
	k_task_function_mapping_native_module_input_type_branchless_variable,

	// This argument is an output, not an input
	k_task_function_mapping_native_module_input_type_none,

	k_task_function_mapping_native_module_input_type_count
};

// A single native module can map to many different tasks. For memory optimization (and performance) we have the
// "branchless" distinction because if an input is only used once, we can directly modify that buffer in-place and reuse
// it as the output.

struct s_task_function_native_module_argument_mapping {
	// The argument type that must be matched for the native module in order for this mapping to be used
	e_task_function_mapping_native_module_input_type input_type;

	// The task function argument index being mapped to
	uint32 task_function_argument_index;
};

struct s_task_function_mapping {
	// Task function to map to
	s_task_function_uid task_function_uid;

	// Mapping for each native module argument
	s_static_array<s_task_function_native_module_argument_mapping, k_max_native_module_arguments>
		native_module_argument_mapping;
};

// Task mapping notation examples (NM = native module):
// native_module(in 0, in 1, out 2) => task(out 0, in 1, in 2)
// mapping = { 1, 2, 0 }
//   NM arg 0 is an input and maps to task argument 1, which is an input
//   NM arg 1 is an input and maps to task argument 2, which is an input
//   NM arg 2 is an output and maps to task argument 0, which is an output
// native_module(in 0, in 1, out 2) => task(inout 0, in 1)
// mapping = { 0, 1, 0 }
//   NM arg 0 is an input and maps to task argument 0, which is an inout
//   NM arg 1 is an input and maps to task argument 1, which is an input
//   NM arg 2 is an output and maps to task argument 0, which is an inout
// Notice that in the last example, the inout task argument is hooked up to both an input and an output from the NM

// A list of task function mappings. The first valid mapping encountered is used, so mappings using the branchless
// optimization should come first.
typedef c_wrapped_array_const<s_task_function_mapping> c_task_function_mapping_list;

#endif // WAVELANG_ENGINE_TASK_FUNCTION_H__
