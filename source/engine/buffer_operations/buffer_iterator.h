#pragma once

#include "common/common.h"

#include "engine/buffer_operations/buffer_iterator_internal.h"

// Call signature:
// template<
//   size_t k_stride,
//   bool k_cico,
//   typename... t_buffers,
//   typename t_function,
//   typename t_last_iteration_function>
// bool iterate_buffers(
//   size_t count,
//   t_buffers &&... buffers,
//   t_function &&function,
//   t_last_iteration_function &&last_iteration_function);
// Iterates buffers using the provided stride, calling the provided function on each iteration.
// k_stride - the number of elements to iterate by. This can be 1, 4, or 128 in the case of bool buffers.
// k_cico - stands for "constant in, constant out". If true, if all input buffers contain constant values, only a single
//   iteration will be run and all output buffers will be marked as constants.
// count - the number of elements to iterate over. When this is not a multiple of k_stride, the final iteration will
//   contain extra trailing elements at the end. These can be safely read from and written to for the sake of
//   optimizations but should otherwise be ignored.
// buffers - the list of buffers to be iterated over. 'const c_[type]_buffer *' are interpreted as inputs and
//   'c_[type]_buffer *' are interpreted as outputs.
// function - the function to run each iteration. The signature of this function is:
//   t_return function(size_t index, t_value... &&values)
//   where each value comes from the corresponding buffer being iterated, and t_return is either void or bool. Input
//   values are either value or const-reference types, and output values are non-const reference types. If t_return is
//   bool, then the function should return false to indicate that iteration should terminate early.
// last_iteration_function - an optional function which, if provided, is called on the last iteration in the case where
//   count is not a multiple of k_stride. It has the same signature as the normal iteration function but is intended to
//   handle cases where special logic must be applied to handle the final trailing elements.
template<size_t k_stride, bool k_cico, typename... t_args>
bool iterate_buffers(size_t count, t_args &&... args) {
	return buffer_iterator_internal::move_buffers_to_end<k_stride, k_cico>(count, std::forward<t_args>(args)...);
}
