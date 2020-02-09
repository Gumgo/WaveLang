struct s_op_select_real {
	real32x4 operator()(
		const int32x4 &condition,
		const real32x4 &true_value,
		const real32x4 &false_value) const {
		int32x4 result =
			(condition & reinterpret_bits<int32x4>(true_value))
			| (~condition & reinterpret_bits<int32x4>(false_value));
		return reinterpret_bits<real32x4>(result);
	}
};

struct s_op_select_bool {
	int32x4 operator()(
		const int32x4 &condition,
		const int32x4 &true_value,
		const int32x4 &false_value) const {
		// Operates on 128 bits at a time
		return (condition & true_value) | (~condition & false_value);
	}
};

namespace core_task_functions {

	void select_real_in_in_in_out(
		const s_task_function_context &context,
		c_bool_const_buffer_or_constant condition,
		c_real_const_buffer_or_constant true_value,
		c_real_const_buffer_or_constant false_value,
		c_real_buffer *result) {
		if (condition.is_constant()) {
			if (condition.get_constant()) {
				if (true_value.is_constant()) {
					*result->get_data<real32>() = true_value.get_constant();
				} else {
					memcpy(
						result->get_data<real32>(),
						true_value.get_buffer()->get_data<real32>(),
						context.buffer_size * sizeof(real32));
				}

				result->set_constant(true_value.is_constant());
			} else {
				if (false_value.is_constant()) {
					*result->get_data<real32>() = false_value.get_constant();
				} else {
					memcpy(
						result->get_data<real32>(),
						false_value.get_buffer()->get_data<real32>(),
						context.buffer_size * sizeof(real32));
				}

				result->set_constant(false_value.is_constant());
			}
		} else {
			buffer_operator_in_in_in_out(
				s_op_select_real(),
				context.buffer_size,
				c_iterable_buffer_bool_4_in(condition),
				c_iterable_buffer_real_in(true_value),
				c_iterable_buffer_real_in(false_value),
				c_iterable_buffer_real_out(result));
		}
	}

	void select_real_in_inout_in(
		const s_task_function_context &context,
		c_bool_const_buffer_or_constant condition,
		c_real_buffer *true_value_result,
		c_real_const_buffer_or_constant false_value) {
		if (condition.is_constant()) {
			if (condition.get_constant()) {
				// The true_value is already in the output buffer
			} else {
				if (false_value.is_constant()) {
					*true_value_result->get_data<real32>() = false_value.get_constant();
				} else {
					memcpy(
						true_value_result->get_data<real32>(),
						false_value.get_buffer()->get_data<real32>(),
						context.buffer_size * sizeof(real32));
				}

				true_value_result->set_constant(false_value.is_constant());
			}
		} else {
			buffer_operator_in_inout_in(
				s_op_select_real(),
				context.buffer_size,
				c_iterable_buffer_bool_4_in(condition),
				c_iterable_buffer_real_inout(true_value_result),
				c_iterable_buffer_real_in(false_value));
		}
	}

	void select_real_in_in_inout(
		const s_task_function_context &context,
		c_bool_const_buffer_or_constant condition,
		c_real_const_buffer_or_constant true_value,
		c_real_buffer *false_value_result) {
		if (condition.is_constant()) {
			if (condition.get_constant()) {
				if (true_value.is_constant()) {
					*false_value_result->get_data<real32>() = true_value.get_constant();
				} else {
					memcpy(
						false_value_result->get_data<real32>(),
						true_value.get_buffer()->get_data<real32>(),
						context.buffer_size * sizeof(real32));
				}

				false_value_result->set_constant(true_value.is_constant());
			} else {
				// The true_value is already in the output buffer
			}
		} else {
			buffer_operator_in_in_inout(
				s_op_select_real(),
				context.buffer_size,
				c_iterable_buffer_bool_4_in(condition),
				c_iterable_buffer_real_in(true_value),
				c_iterable_buffer_real_inout(false_value_result));
		}
	}

	void select_bool_in_in_in_out(
		const s_task_function_context &context,
		wl_in_source("condition") c_bool_const_buffer_or_constant condition,
		wl_in_source("true_value") c_bool_const_buffer_or_constant true_value,
		wl_in_source("false_value") c_bool_const_buffer_or_constant false_value,
		wl_out_source("result") c_bool_buffer *result) {
		if (condition.is_constant()) {
			if (condition.get_constant()) {
				if (true_value.is_constant()) {
					*result->get_data<int32>() = -static_cast<int32>(true_value.get_constant());
				} else {
					memcpy(
						result->get_data<int32>(),
						true_value.get_buffer()->get_data<int32>(),
						BOOL_BUFFER_INT32_COUNT(context.buffer_size) * sizeof(int32));
				}

				result->set_constant(true_value.is_constant());
			} else {
				if (false_value.is_constant()) {
					*result->get_data<int32>() = -static_cast<int32>(false_value.get_constant());
				} else {
					memcpy(
						result->get_data<int32>(),
						false_value.get_buffer()->get_data<int32>(),
						BOOL_BUFFER_INT32_COUNT(context.buffer_size) * sizeof(int32));
				}

				result->set_constant(false_value.is_constant());
			}
		} else {
			buffer_operator_in_in_in_out(
				s_op_select_bool(),
				context.buffer_size,
				c_iterable_buffer_bool_128_in(condition),
				c_iterable_buffer_bool_128_in(true_value),
				c_iterable_buffer_bool_128_in(false_value),
				c_iterable_buffer_bool_128_out(result));
		}
	}

	void select_bool_inout_in_in(
		const s_task_function_context &context,
		c_bool_buffer *condition_result,
		c_bool_const_buffer_or_constant true_value,
		c_bool_const_buffer_or_constant false_value) {
		if (condition_result->is_constant()) {
			if ((*condition_result->get_data<int32>() & 1) != 0) {
				if (true_value.is_constant()) {
					*condition_result->get_data<int32>() = -static_cast<int32>(true_value.get_constant());
				} else {
					memcpy(
						condition_result->get_data<int32>(),
						true_value.get_buffer()->get_data<int32>(),
						BOOL_BUFFER_INT32_COUNT(context.buffer_size) * sizeof(int32));
				}

				condition_result->set_constant(true_value.is_constant());
			} else {
				if (false_value.is_constant()) {
					*condition_result->get_data<int32>() = -static_cast<int32>(false_value.get_constant());
				} else {
					memcpy(
						condition_result->get_data<int32>(),
						false_value.get_buffer()->get_data<int32>(),
						BOOL_BUFFER_INT32_COUNT(context.buffer_size) * sizeof(int32));
				}

				condition_result->set_constant(false_value.is_constant());
			}
		} else {
			buffer_operator_inout_in_in(
				s_op_select_bool(),
				context.buffer_size,
				c_iterable_buffer_bool_128_inout(condition_result),
				c_iterable_buffer_bool_128_in(true_value),
				c_iterable_buffer_bool_128_in(false_value));
		}
	}

	void select_bool_in_inout_in(
		const s_task_function_context &context,
		c_bool_const_buffer_or_constant condition,
		c_bool_buffer *true_value_result,
		c_bool_const_buffer_or_constant false_value) {
		if (condition.is_constant()) {
			if (condition.get_constant()) {
				// The true_value is already in the output buffer
			} else {
				if (false_value.is_constant()) {
					*true_value_result->get_data<int32>() = -static_cast<int32>(false_value.get_constant());
				} else {
					memcpy(
						true_value_result->get_data<int32>(),
						false_value.get_buffer()->get_data<int32>(),
						BOOL_BUFFER_INT32_COUNT(context.buffer_size * sizeof(int32)));
				}

				true_value_result->set_constant(false_value.is_constant());
			}
		} else {
			buffer_operator_in_inout_in(
				s_op_select_bool(),
				context.buffer_size,
				c_iterable_buffer_bool_128_in(condition),
				c_iterable_buffer_bool_128_inout(true_value_result),
				c_iterable_buffer_bool_128_in(false_value));
		}
	}

	void select_bool_in_in_inout(
		const s_task_function_context &context,
		c_bool_const_buffer_or_constant condition,
		c_bool_const_buffer_or_constant true_value,
		c_bool_buffer *false_value_result) {
		if (condition.is_constant()) {
			if (condition.get_constant()) {
				if (true_value.is_constant()) {
					*false_value_result->get_data<int32>() = -static_cast<int32>(true_value.get_constant());
				} else {
					memcpy(
						false_value_result->get_data<int32>(),
						true_value.get_buffer()->get_data<int32>(),
						BOOL_BUFFER_INT32_COUNT(context.buffer_size * sizeof(int32)));
				}

				false_value_result->set_constant(true_value.is_constant());
			} else {
				// The true_value is already in the output buffer
			}
		} else {
			buffer_operator_in_in_inout(
				s_op_select_bool(),
				context.buffer_size,
				c_iterable_buffer_bool_128_in(condition),
				c_iterable_buffer_bool_128_in(true_value),
				c_iterable_buffer_bool_128_inout(false_value_result));
		}
	}

}
