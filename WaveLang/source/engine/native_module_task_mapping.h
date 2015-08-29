#ifndef WAVELANG_NATIVE_MODULE_TASK_MAPPING_H__
#define WAVELANG_NATIVE_MODULE_TASK_MAPPING_H__

#include "common/common.h"
#include "execution_graph/native_modules.h"
#include "engine/task_functions.h"

// Native modules must be convered into tasks. A single native module can potentially be converted into a number of
// different tasks depending on its inputs. Once a task is selected, the inputs and outputs of the native module must be
// mapped to the inputs/outputs of the task.

// Maps execution graph native module inputs/outputs to task input/output/inout argument indices
// The array should first list all inputs for the native module, then list all outputs
typedef c_wrapped_array_const<uint32> c_task_mapping_array;

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

bool get_task_mapping_for_native_module_and_inputs(
	uint32 native_module_index,
	const char *native_module_inputs,
	e_task_function &out_task_function,
	c_task_mapping_array &out_task_mapping_array);

#endif // WAVELANG_NATIVE_MODULE_TASK_MAPPING_H__