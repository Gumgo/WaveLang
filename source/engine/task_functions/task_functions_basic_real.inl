static void task_function_negation_in_out(const s_task_function_context &context) {
	s_buffer_operation_negation::in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_out());
}

static void task_function_negation_inout(const s_task_function_context &context) {
	s_buffer_operation_negation::inout(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_addition_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_addition::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_addition_inout_in(const s_task_function_context &context) {
	s_buffer_operation_addition::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_subtraction_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_subtraction::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_subtraction_inout_in(const s_task_function_context &context) {
	s_buffer_operation_subtraction::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_subtraction_in_inout(const s_task_function_context &context) {
	s_buffer_operation_subtraction::in_inout(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_multiplication_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_multiplication::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_multiplication_inout_in(const s_task_function_context &context) {
	s_buffer_operation_multiplication::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_division_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_division::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_division_inout_in(const s_task_function_context &context) {
	s_buffer_operation_division::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_division_in_inout(const s_task_function_context &context) {
	s_buffer_operation_division::in_inout(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_modulo_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_modulo::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_modulo_inout_in(const s_task_function_context &context) {
	s_buffer_operation_modulo::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_modulo_in_inout(const s_task_function_context &context) {
	s_buffer_operation_modulo::in_inout(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_inout());
}

static void register_task_functions_basic_real() {
	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_negation_in_out_uid,
				"negation_in_out",
				nullptr, nullptr, nullptr, task_function_negation_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(out, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_negation_inout_uid,
				"negation_inout",
				nullptr, nullptr, nullptr, task_function_negation_inout,
				s_task_function_argument_list::build(TDT(inout, real))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_negation_inout_uid, "b.",
				s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_negation_in_out_uid, "v.",
				s_task_function_native_module_argument_mapping::build(0, 1))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_negation_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_addition_in_in_out_uid,
				"addition_in_in_out",
				nullptr, nullptr, nullptr, task_function_addition_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_addition_inout_in_uid,
				"addition_inout_in",
				nullptr, nullptr, nullptr, task_function_addition_inout_in,
				s_task_function_argument_list::build(TDT(inout, real), TDT(in, real))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_addition_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_addition_inout_in_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_addition_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_addition_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_in_in_out_uid,
				"subtraction_in_in_out",
				nullptr, nullptr, nullptr, task_function_subtraction_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_inout_in_uid,
				"subtraction_inout_in",
				nullptr, nullptr, nullptr, task_function_subtraction_inout_in,
				s_task_function_argument_list::build(TDT(inout, real), TDT(in, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_subtraction_in_inout_uid,
				"subtraction_in_inout",
				nullptr, nullptr, nullptr, task_function_subtraction_in_inout,
				s_task_function_argument_list::build(TDT(in, real), TDT(inout, real))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_subtraction_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_subtraction_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_subtraction_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_subtraction_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_multiplication_in_in_out_uid,
				"multiplication_in_in_out",
				nullptr, nullptr, nullptr, task_function_multiplication_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_multiplication_inout_in_uid,
				"multiplication_inout_in",
				nullptr, nullptr, nullptr, task_function_multiplication_inout_in,
				s_task_function_argument_list::build(TDT(inout, real), TDT(in, real))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_multiplication_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_multiplication_inout_in_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_multiplication_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_multiplication_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_in_in_out_uid,
				"division_in_in_out",
				nullptr, nullptr, nullptr, task_function_division_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_inout_in_uid,
				"division_inout_in",
				nullptr, nullptr, nullptr, task_function_division_inout_in,
				s_task_function_argument_list::build(TDT(inout, real), TDT(in, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_division_in_inout_uid,
				"division_in_inout",
				nullptr, nullptr, nullptr, task_function_division_in_inout,
				s_task_function_argument_list::build(TDT(in, real), TDT(inout, real))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_division_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_division_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_division_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_division_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_in_in_out_uid,
				"modulo_in_in_out",
				nullptr, nullptr, nullptr, task_function_modulo_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_inout_in_uid,
				"modulo_inout_in",
				nullptr, nullptr, nullptr, task_function_modulo_inout_in,
				s_task_function_argument_list::build(TDT(inout, real), TDT(in, real))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_modulo_in_inout_uid,
				"modulo_in_inout",
				nullptr, nullptr, nullptr, task_function_modulo_in_inout,
				s_task_function_argument_list::build(TDT(in, real), TDT(inout, real))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_modulo_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_modulo_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_modulo_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_modulo_uid, c_task_function_mapping_list::construct(mappings));
	}
}