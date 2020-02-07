namespace core_task_functions {

	void negation_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *result) {
		s_buffer_operation_negation::in_out(context.buffer_size, a, result);
	}

	void negation_inout(
		const s_task_function_context &context,
		c_real_buffer *a_result) {
		s_buffer_operation_negation::inout(context.buffer_size, a_result);
	}

	void addition_in_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_real_buffer *result) {
		s_buffer_operation_addition::in_in_out(context.buffer_size, a, b, result);
	}

	void addition_inout_in(
		const s_task_function_context &context,
		c_real_buffer *a_result,
		c_real_const_buffer_or_constant b) {
		s_buffer_operation_addition::inout_in(context.buffer_size, a_result, b);
	}

	void addition_in_inout(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *b_result) {
		s_buffer_operation_addition::in_inout(context.buffer_size, a, b_result);
	}

	void subtraction_in_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_real_buffer *result) {
		s_buffer_operation_subtraction::in_in_out(context.buffer_size, a, b, result);
	}

	void subtraction_inout_in(
		const s_task_function_context &context,
		c_real_buffer *a_result,
		c_real_const_buffer_or_constant b) {
		s_buffer_operation_subtraction::inout_in(context.buffer_size, a_result, b);
	}

	void subtraction_in_inout(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *b_result) {
		s_buffer_operation_subtraction::in_inout(context.buffer_size, a, b_result);
	}


	void multiplication_in_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_real_buffer *result) {
		s_buffer_operation_multiplication::in_in_out(context.buffer_size, a, b, result);
	}

	void multiplication_inout_in(
		const s_task_function_context &context,
		c_real_buffer *a_result,
		c_real_const_buffer_or_constant b) {
		s_buffer_operation_multiplication::inout_in(context.buffer_size, a_result, b);
	}

	void multiplication_in_inout(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *b_result) {
		s_buffer_operation_multiplication::in_inout(context.buffer_size, a, b_result);
	}

	void division_in_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_real_buffer *result) {
		s_buffer_operation_division::in_in_out(context.buffer_size, a, b, result);
	}

	void division_inout_in(
		const s_task_function_context &context,
		c_real_buffer *a_result,
		c_real_const_buffer_or_constant b) {
		s_buffer_operation_division::inout_in(context.buffer_size, a_result, b);
	}

	void division_in_inout(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *b_result) {
		s_buffer_operation_division::in_inout(context.buffer_size, a, b_result);
	}

	void modulo_in_in_out(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_real_buffer *result) {
		s_buffer_operation_modulo::in_in_out(context.buffer_size, a, b, result);
	}

	void modulo_inout_in(
		const s_task_function_context &context,
		c_real_buffer *a_result,
		c_real_const_buffer_or_constant b) {
		s_buffer_operation_modulo::inout_in(context.buffer_size, a_result, b);
	}

	void modulo_in_inout(
		const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_buffer *b_result) {
		s_buffer_operation_modulo::in_inout(context.buffer_size, a, b_result);
	}

}
