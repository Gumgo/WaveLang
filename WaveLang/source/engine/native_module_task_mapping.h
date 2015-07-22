#ifndef WAVELANG_NATIVE_MODULE_TASK_MAPPING_H__
#define WAVELANG_NATIVE_MODULE_TASK_MAPPING_H__

#include "common/common.h"
#include "execution_graph/native_modules.h"
#include "engine/task_functions.h"

// Native modules must be convered into tasks. A single native module can potentially be converted into a number of
// different tasks depending on its inputs. Once a task is selected, the inputs and outputs of the native module must be
// mapped to the inputs/outputs of the task.

enum e_task_mapping_location {
	k_task_mapping_location_constant_in,
	k_task_mapping_location_buffer_in,
	k_task_mapping_location_buffer_out,
	k_task_mapping_location_buffer_inout,

	k_task_mapping_location_count
};

// Specifies where on a task to map a native module input/output
struct s_task_mapping {
	// Where to map to on the task
	e_task_mapping_location location;

	// Index for the location
	size_t index;

	s_task_mapping(e_task_mapping_location loc, size_t idx)
		: location(loc)
		, index(idx) {
	}
};

// Maps execution graph native module inputs/outputs to task inputs/outputs/inouts
// The array should first list all inputs for the native module, then list all outputs
typedef c_wrapped_array_const<s_task_mapping> c_task_mapping_array;

// Chooses the appropriate task and input/output mapping for the given native module and inputs, or returns false if no
// mapping is found.

// We use a string shorthand notation to represent possible native module inputs. The Nth character in the string
// represents the Nth input to the native module. The possible input types are:
// - c: a constant input
// - v: a value input
// - V: a value input which does not branch; i.e. this value is only used in this input and nowhere else

// A single native module can map to many different tasks - e.g. for performance reasons, we have different tasks for v
// + v and v + c, and for memory optimization (and performance) we also have tasks which directly modify buffers which
// never need to be reused.

// Task mapping notation examples (NM = native module, see cpp file for meanings of TM_ macros):
// { TM_I(0), TM_I(1), TM_O(0) }
//   NM arg 0 is an input and maps to task input 0
//   NM arg 1 is an input and maps to task input 1
//   NM arg 2 is an output and maps to task output 0
// { TM_IO(0), TM_C(0), TM_IO(0) }
//   NM arg 0 is an input and maps to task inout 0
//   NM arg 1 is a constant and maps to task constant 0
//   NM arg 2 is an output and maps to task inout 0

bool get_task_mapping_for_native_module_and_inputs(
	uint32 native_module_index,
	const char *native_module_inputs,
	e_task_function &out_task_function,
	c_task_mapping_array &out_task_mapping_array);

#endif // WAVELANG_NATIVE_MODULE_TASK_MAPPING_H__