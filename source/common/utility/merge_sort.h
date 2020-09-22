#pragma once

#include "common/common.h"

template<typename t_value, typename t_comparator>
void merge_sort_merge_helper(
	c_wrapped_array<t_value> left_values,
	c_wrapped_array<t_value> right_values,
	c_wrapped_array<t_value> scratch_buffer,
	const t_comparator &comparator) {
	wl_assert(left_values.get_count() + right_values.get_count() == scratch_buffer.get_count());

	size_t left = 0;
	size_t right = 0;

	for (size_t scratch_index = 0; scratch_index < scratch_buffer.get_count(); scratch_index++) {
		if ((left < left_values.get_count())
			&& (right >= right_values.get_count() || !comparator(right_values[right], left_values[left]))) {
			// Take left value
			scratch_buffer[scratch_index] = left_values[left];
			left++;
		} else {
			// Take right value
			scratch_buffer[scratch_index] = right_values[right];
			right++;
		}
	}

	wl_assert(left + right == scratch_buffer.get_count());
}

template<typename t_value, typename t_comparator>
void merge_sort(
	c_wrapped_array<t_value> values,
	c_wrapped_array<t_value> scratch_buffer,
	const t_comparator &comparator) {
	wl_assert(values.get_count() == scratch_buffer.get_count());

	if (values.get_count() <= 1) {
		return;
	}

	size_t left_count = values.get_count() / 2;
	size_t right_count = values.get_count() - left_count;

	// Apply recursively to left and right halves
	c_wrapped_array<t_value> left_values(values.get_pointer(), left_count);
	c_wrapped_array<t_value> left_scratch_buffer(scratch_buffer.get_pointer(), left_count);

	c_wrapped_array<t_value> right_values(&values[left_count], right_count);
	c_wrapped_array<t_value> right_scratch_buffer(&scratch_buffer[left_count], right_count);

	merge_sort(left_values, left_scratch_buffer, comparator);
	merge_sort(right_values, right_scratch_buffer, comparator);

	// Merge the two sorted halves into the scratch buffer
	merge_sort_merge_helper(left_values, right_values, scratch_buffer, comparator);

	// Copy back into values
	for (size_t index = 0; index < values.get_count(); index++) {
		values[index] = scratch_buffer[index];
	}
}

