struct s_op_real_select {
	c_real32_4 operator()(
		const c_int32_4 &condition,
		const c_real32_4 &true_value,
		const c_real32_4 &false_value) const {
		c_int32_4 result =
			(condition & true_value.int32_4_from_bits()) | (~condition & false_value.int32_4_from_bits());
		return result.real32_4_from_bits();
	}
};

struct s_op_bool_select {
	c_int32_4 operator()(
		const c_int32_4 &condition,
		const c_int32_4 &true_value,
		const c_int32_4 &false_value) const {
		// Operates on 128 bits at a time
		return (condition & true_value) | (~condition & false_value);
	}
};

static void task_function_real_select_in_in_in_out(const s_task_function_context &context) {
	c_bool_const_buffer_or_constant condition = context.arguments[0].get_bool_buffer_or_constant_in();
	c_real_const_buffer_or_constant true_value = context.arguments[1].get_real_buffer_or_constant_in();
	c_real_const_buffer_or_constant false_value = context.arguments[2].get_real_buffer_or_constant_in();
	c_real_buffer_out out = context.arguments[3].get_real_buffer_out();

	if (condition.is_constant()) {
		if (condition.get_constant()) {
			if (true_value.is_constant()) {
				*out->get_data<real32>() = true_value.get_constant();
			} else {
				memcpy(out->get_data<real32>(), true_value.get_buffer()->get_data<real32>(),
					context.buffer_size * sizeof(real32));
			}

			out->set_constant(true_value.is_constant());
		} else {
			if (false_value.is_constant()) {
				*out->get_data<real32>() = false_value.get_constant();
			} else {
				memcpy(out->get_data<real32>(), false_value.get_buffer()->get_data<real32>(),
					context.buffer_size * sizeof(real32));
			}

			out->set_constant(false_value.is_constant());
		}
	} else {
		buffer_operator_in_in_in_out(s_op_real_select(), context.buffer_size,
			c_iterable_buffer_bool_4_in(condition),
			c_iterable_buffer_real_in(true_value),
			c_iterable_buffer_real_in(false_value),
			c_iterable_buffer_real_out(out));
	}
}

static void task_function_real_select_in_inout_in(const s_task_function_context &context) {
	c_bool_const_buffer_or_constant condition = context.arguments[0].get_bool_buffer_or_constant_in();
	c_real_buffer_inout true_value_out = context.arguments[1].get_real_buffer_inout();
	c_real_const_buffer_or_constant false_value = context.arguments[2].get_real_buffer_or_constant_in();

	if (condition.is_constant()) {
		if (condition.get_constant()) {
			// The true_value is already in the output buffer
		} else {
			if (false_value.is_constant()) {
				*true_value_out->get_data<real32>() = false_value.get_constant();
			} else {
				memcpy(true_value_out->get_data<real32>(), false_value.get_buffer()->get_data<real32>(),
					context.buffer_size * sizeof(real32));
			}

			true_value_out->set_constant(false_value.is_constant());
		}
	} else {
		buffer_operator_in_inout_in(s_op_real_select(), context.buffer_size,
			c_iterable_buffer_bool_4_in(condition),
			c_iterable_buffer_real_inout(true_value_out),
			c_iterable_buffer_real_in(false_value));
	}
}

static void task_function_real_select_in_in_inout(const s_task_function_context &context) {
	c_bool_const_buffer_or_constant condition = context.arguments[0].get_bool_buffer_or_constant_in();
	c_real_const_buffer_or_constant true_value = context.arguments[1].get_real_buffer_or_constant_in();
	c_real_buffer_inout false_value_out = context.arguments[2].get_real_buffer_inout();

	if (condition.is_constant()) {
		if (condition.get_constant()) {
			if (true_value.is_constant()) {
				*false_value_out->get_data<real32>() = true_value.get_constant();
			} else {
				memcpy(false_value_out->get_data<real32>(), true_value.get_buffer()->get_data<real32>(),
					context.buffer_size * sizeof(real32));
			}

			false_value_out->set_constant(true_value.is_constant());
		} else {
			// The true_value is already in the output buffer
		}
	} else {
		buffer_operator_in_in_inout(s_op_real_select(), context.buffer_size,
			c_iterable_buffer_bool_4_in(condition),
			c_iterable_buffer_real_in(true_value),
			c_iterable_buffer_real_inout(false_value_out));
	}
}

static void task_function_bool_select_in_in_in_out(const s_task_function_context &context) {
	c_bool_const_buffer_or_constant condition = context.arguments[0].get_bool_buffer_or_constant_in();
	c_bool_const_buffer_or_constant true_value = context.arguments[1].get_bool_buffer_or_constant_in();
	c_bool_const_buffer_or_constant false_value = context.arguments[2].get_bool_buffer_or_constant_in();
	c_bool_buffer_out out = context.arguments[3].get_bool_buffer_out();

	if (condition.is_constant()) {
		if (condition.get_constant()) {
			if (true_value.is_constant()) {
				*out->get_data<int32>() = -static_cast<int32>(true_value.get_constant());
			} else {
				memcpy(out->get_data<int32>(), true_value.get_buffer()->get_data<int32>(),
					BOOL_BUFFER_INT32_COUNT(context.buffer_size) * sizeof(int32));
			}

			out->set_constant(true_value.is_constant());
		} else {
			if (false_value.is_constant()) {
				*out->get_data<int32>() = -static_cast<int32>(false_value.get_constant());
			} else {
				memcpy(out->get_data<int32>(), false_value.get_buffer()->get_data<int32>(),
					BOOL_BUFFER_INT32_COUNT(context.buffer_size) * sizeof(int32));
			}

			out->set_constant(false_value.is_constant());
		}
	} else {
		buffer_operator_in_in_in_out(s_op_bool_select(), context.buffer_size,
			c_iterable_buffer_bool_128_in(condition),
			c_iterable_buffer_bool_128_in(true_value),
			c_iterable_buffer_bool_128_in(false_value),
			c_iterable_buffer_bool_128_out(out));
	}
}

static void task_function_bool_select_inout_in_in(const s_task_function_context &context) {
	c_bool_buffer_inout condition_out = context.arguments[0].get_bool_buffer_inout();
	c_bool_const_buffer_or_constant true_value = context.arguments[1].get_bool_buffer_or_constant_in();
	c_bool_const_buffer_or_constant false_value = context.arguments[2].get_bool_buffer_or_constant_in();

	if (condition_out->is_constant()) {
		if ((*condition_out->get_data<int32>() & 1) != 0) {
			if (true_value.is_constant()) {
				*condition_out->get_data<int32>() = -static_cast<int32>(true_value.get_constant());
			} else {
				memcpy(condition_out->get_data<int32>(), true_value.get_buffer()->get_data<int32>(),
					BOOL_BUFFER_INT32_COUNT(context.buffer_size) * sizeof(int32));
			}

			condition_out->set_constant(true_value.is_constant());
		} else {
			if (false_value.is_constant()) {
				*condition_out->get_data<int32>() = -static_cast<int32>(false_value.get_constant());
			} else {
				memcpy(condition_out->get_data<int32>(), false_value.get_buffer()->get_data<int32>(),
					BOOL_BUFFER_INT32_COUNT(context.buffer_size) * sizeof(int32));
			}

			condition_out->set_constant(false_value.is_constant());
		}
	} else {
		buffer_operator_inout_in_in(s_op_bool_select(), context.buffer_size,
			c_iterable_buffer_bool_128_inout(condition_out),
			c_iterable_buffer_bool_128_in(true_value),
			c_iterable_buffer_bool_128_in(false_value));
	}
}

static void task_function_bool_select_in_inout_in(const s_task_function_context &context) {
	c_bool_const_buffer_or_constant condition = context.arguments[0].get_bool_buffer_or_constant_in();
	c_bool_buffer_inout true_value_out = context.arguments[1].get_bool_buffer_inout();
	c_bool_const_buffer_or_constant false_value = context.arguments[2].get_bool_buffer_or_constant_in();

	if (condition.is_constant()) {
		if (condition.get_constant()) {
			// The true_value is already in the output buffer
		} else {
			if (false_value.is_constant()) {
				*true_value_out->get_data<int32>() = -static_cast<int32>(false_value.get_constant());
			} else {
				memcpy(true_value_out->get_data<int32>(), false_value.get_buffer()->get_data<int32>(),
					BOOL_BUFFER_INT32_COUNT(context.buffer_size * sizeof(int32)));
			}

			true_value_out->set_constant(false_value.is_constant());
		}
	} else {
		buffer_operator_in_inout_in(s_op_bool_select(), context.buffer_size,
			c_iterable_buffer_bool_128_in(condition),
			c_iterable_buffer_bool_128_inout(true_value_out),
			c_iterable_buffer_bool_128_in(false_value));
	}
}

static void task_function_bool_select_in_in_inout(const s_task_function_context &context) {
	c_bool_const_buffer_or_constant condition = context.arguments[0].get_bool_buffer_or_constant_in();
	c_bool_const_buffer_or_constant true_value = context.arguments[1].get_bool_buffer_or_constant_in();
	c_bool_buffer_inout false_value_out = context.arguments[2].get_bool_buffer_inout();

	if (condition.is_constant()) {
		if (condition.get_constant()) {
			if (true_value.is_constant()) {
				*false_value_out->get_data<int32>() = -static_cast<int32>(true_value.get_constant());
			} else {
				memcpy(false_value_out->get_data<int32>(), true_value.get_buffer()->get_data<int32>(),
					BOOL_BUFFER_INT32_COUNT(context.buffer_size * sizeof(int32)));
			}

			false_value_out->set_constant(true_value.is_constant());
		} else {
			// The true_value is already in the output buffer
		}
	} else {
		buffer_operator_in_in_inout(s_op_bool_select(), context.buffer_size,
			c_iterable_buffer_bool_128_in(condition),
			c_iterable_buffer_bool_128_in(true_value),
			c_iterable_buffer_bool_128_inout(false_value_out));
	}
}

static void register_task_functions_basic_select() {
	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_real_select_in_in_in_out_uid,
				"real_select_in_in_in_out",
				nullptr, nullptr, nullptr, task_function_real_select_in_in_in_out,
				s_task_function_argument_list::build(TDT(in, bool), TDT(in, real), TDT(in, real), TDT(out, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_real_select_in_inout_in_uid,
				"real_select_in_inout_in",
				nullptr, nullptr, nullptr, task_function_real_select_in_inout_in,
				s_task_function_argument_list::build(TDT(in, bool), TDT(inout, real), TDT(in, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_real_select_in_in_inout_uid,
				"real_select_in_in_inout",
				nullptr, nullptr, nullptr, task_function_real_select_in_in_inout,
				s_task_function_argument_list::build(TDT(in, bool), TDT(in, real), TDT(inout, real))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_real_select_in_inout_in_uid, "vbv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 1)),

			s_task_function_mapping::build(k_task_function_real_select_in_in_inout_uid, "vvb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 2)),

			s_task_function_mapping::build(k_task_function_real_select_in_in_in_out_uid, "vvv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 3)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_real_select_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_bool_select_in_in_in_out_uid,
				"bool_select_in_in_in_out",
				nullptr, nullptr, nullptr, task_function_bool_select_in_in_in_out,
				s_task_function_argument_list::build(TDT(in, bool), TDT(in, bool), TDT(in, bool), TDT(out, bool))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_bool_select_inout_in_in_uid,
				"bool_select_inout_in_in",
				nullptr, nullptr, nullptr, task_function_bool_select_inout_in_in,
				s_task_function_argument_list::build(TDT(inout, bool), TDT(in, bool), TDT(in, bool))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_bool_select_in_inout_in_uid,
				"bool_select_in_inout_in",
				nullptr, nullptr, nullptr, task_function_bool_select_in_inout_in,
				s_task_function_argument_list::build(TDT(in, bool), TDT(inout, bool), TDT(in, bool))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_bool_select_in_in_inout_uid,
				"bool_select_in_in_inout",
				nullptr, nullptr, nullptr, task_function_bool_select_in_in_inout,
				s_task_function_argument_list::build(TDT(in, bool), TDT(in, bool), TDT(inout, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_bool_select_inout_in_in_uid, "bvv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 0)),

			s_task_function_mapping::build(k_task_function_bool_select_in_inout_in_uid, "vbv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 1)),

			s_task_function_mapping::build(k_task_function_bool_select_in_in_inout_uid, "vvb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 2)),

			s_task_function_mapping::build(k_task_function_bool_select_in_in_in_out_uid, "vvv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2, 3)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_bool_select_uid, c_task_function_mapping_list::construct(mappings));
	}
}