#ifndef WAVELANG_TASK_FUNCTION_H__
#define WAVELANG_TASK_FUNCTION_H__

#include "common/common.h"
#include "execution_graph/native_module.h"

class c_buffer;
class c_sample_library_accessor;
class c_sample_library_requester;

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

enum e_task_data_type {
	k_task_data_type_real_buffer_in,
	k_task_data_type_real_buffer_out,
	k_task_data_type_real_buffer_inout,
	k_task_data_type_real_constant_in,
	k_task_data_type_bool_constant_in,
	k_task_data_type_string_constant_in,

	k_task_data_type_count
};

struct s_task_function_argument {
#if PREDEFINED(ASSERTS_ENABLED)
	e_task_data_type type;
#endif // PREDEFINED(ASSERTS_ENABLED)

	// Do not access these directly
	union {
		const c_buffer *real_buffer_in;
		c_buffer *real_buffer_out;
		c_buffer *real_buffer_inout;
		real32 real_constant_in;
		bool bool_constant_in;
		const char *string_constant_in;
	} data;

	const c_buffer *get_real_buffer_in() const {
		wl_assert(type == k_task_data_type_real_buffer_in);
		return data.real_buffer_in;
	}

	c_buffer *get_real_buffer_out() const {
		wl_assert(type == k_task_data_type_real_buffer_out);
		return data.real_buffer_out;
	}

	c_buffer *get_real_buffer_inout() const {
		wl_assert(type == k_task_data_type_real_buffer_inout);
		return data.real_buffer_inout;
	}

	real32 get_real_constant_in() const {
		wl_assert(type == k_task_data_type_real_constant_in);
		return data.real_constant_in;
	}

	bool get_bool_constant_in() const {
		wl_assert(type == k_task_data_type_bool_constant_in);
		return data.bool_constant_in;
	}

	const char *get_string_constant_in() const {
		wl_assert(type == k_task_data_type_string_constant_in);
		return data.string_constant_in;
	}
};

typedef c_wrapped_array_const<s_task_function_argument> c_task_function_arguments;

struct s_task_function_context {
	c_sample_library_accessor *sample_accessor;
	c_sample_library_requester *sample_requester;
	// $TODO error reporting interface

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

// Function executed for the task
typedef void (*f_task_function)(const s_task_function_context &context);

static const size_t k_max_task_function_arguments = 10;

struct s_task_function_argument_list {
	static const e_task_data_type k_argument_none = k_task_data_type_count;
	e_task_data_type arguments[k_max_task_function_arguments];

	static s_task_function_argument_list build(
		e_task_data_type arg_0 = k_argument_none,
		e_task_data_type arg_1 = k_argument_none,
		e_task_data_type arg_2 = k_argument_none,
		e_task_data_type arg_3 = k_argument_none,
		e_task_data_type arg_4 = k_argument_none,
		e_task_data_type arg_5 = k_argument_none,
		e_task_data_type arg_6 = k_argument_none,
		e_task_data_type arg_7 = k_argument_none,
		e_task_data_type arg_8 = k_argument_none,
		e_task_data_type arg_9 = k_argument_none);
};

// Shorthand for argument specification
#define TDT(type) k_task_data_type_ ## type

struct s_task_function {
	// Unique identifier for this task function
	s_task_function_uid uid;

	// Memory query function, or null
	f_task_memory_query memory_query;

	// Function for initializing task memory
	f_task_initializer initializer;

	// Function to execute
	f_task_function function;

	// Number of arguments
	uint32 argument_count;

	// Type of each argument
	e_task_data_type argument_types[k_max_task_function_arguments];

	static s_task_function build(
		s_task_function_uid uid,
		f_task_memory_query memory_query,
		f_task_initializer initializer,
		f_task_function function,
		const s_task_function_argument_list &arguments);
};

// Task function mappings

// Native modules must be convered into tasks. A single native module can potentially be converted into a number of
// different tasks depending on its inputs. Once a task is selected, the inputs and outputs of the native module must be
// mapped to the inputs/outputs of the task.

// The following describe the possible types of input arguments for a native module
enum e_task_function_mapping_native_module_input_type {
	// The input is a constant
	k_task_function_mapping_native_module_input_type_constant,

	// The input is a variable (non-constant)
	k_task_function_mapping_native_module_input_type_variable,

	// The input is a variable which is not used as any other input
	k_task_function_mapping_native_module_input_type_branchless_variable,

	// This argument is an output, not an input
	k_task_function_mapping_native_module_input_type_none,

	k_task_function_mapping_native_module_input_type_count
};

// A single native module can map to many different tasks - e.g. for performance reasons, we have different tasks for a
// native module call taking two variable inputs versus one taking a variable and a constant input. In addition, for
// memory optimization (and performance) we also have the "branchless" distinction because if an input is only used
// once, we can directly modify that buffer in-place and reuse it as the output.

struct s_task_function_native_module_argument_mapping {
	static const uint32 k_argument_none = static_cast<uint32>(-1);

	uint32 mapping[k_max_native_module_arguments];

	static s_task_function_native_module_argument_mapping build(
		uint32 arg_0 = k_argument_none,
		uint32 arg_1 = k_argument_none,
		uint32 arg_2 = k_argument_none,
		uint32 arg_3 = k_argument_none,
		uint32 arg_4 = k_argument_none,
		uint32 arg_5 = k_argument_none,
		uint32 arg_6 = k_argument_none,
		uint32 arg_7 = k_argument_none,
		uint32 arg_8 = k_argument_none,
		uint32 arg_9 = k_argument_none);
};

struct s_task_function_mapping {
	static const e_task_function_mapping_native_module_input_type k_input_type_none =
		k_task_function_mapping_native_module_input_type_count;

	// Task function to map to
	s_task_function_uid task_function_uid;

	// The pattern of argument types that must be matched for the native module in order for this mapping to be used
	e_task_function_mapping_native_module_input_type native_module_input_types[k_max_native_module_arguments];

	// Maps each native module argument to a task function in/out/inout argument
	s_task_function_native_module_argument_mapping native_module_argument_mapping;

	// We use a string shorthand notation to represent possible native module arguments. The Nth character in the string
	// represents the Nth argument to the native module. The possible input types are:
	// - c: a constant input
	// - v: a variable input
	// - b: a variable input which does not branch; i.e. this variable is only used in this input and nowhere else
	// - .: this argument is an output and should be ignored
	static s_task_function_mapping build(
		s_task_function_uid task_function_uid,
		const char *input_pattern,
		const s_task_function_native_module_argument_mapping &mapping);
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

#endif // WAVELANG_TASK_FUNCTION_H__