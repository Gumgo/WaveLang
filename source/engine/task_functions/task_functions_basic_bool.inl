struct s_op_not {
	c_int32_4 operator()(const c_int32_4 &a) const { return ~a; }
};

struct s_op_real_equal {
	c_int32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a == b; }
};

struct s_op_real_not_equal {
	c_int32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a != b; }
};

struct s_op_bool_equal {
	c_int32_4 operator()(const c_int32_4 &a, const c_int32_4 &b) const { return a == b; }
};

struct s_op_bool_not_equal {
	c_int32_4 operator()(const c_int32_4 &a, const c_int32_4 &b) const { return a != b; }
};

struct s_op_greater {
	c_int32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a > b; }
};

struct s_op_less {
	c_int32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a < b; }
};

struct s_op_greater_equal {
	c_int32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a >= b; }
};

struct s_op_less_equal {
	c_int32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a <= b; }
};

struct s_op_and {
	c_int32_4 operator()(const c_int32_4 &a, const c_int32_4 &b) const { return a & b; }
};

struct s_op_or {
	c_int32_4 operator()(const c_int32_4 &a, const c_int32_4 &b) const { return a | b; }
};

typedef s_buffer_operation_bool_bool<s_op_not> s_buffer_operation_not;
typedef s_buffer_operation_real_real_bool<s_op_real_equal> s_buffer_operation_real_equal;
typedef s_buffer_operation_real_real_bool<s_op_real_not_equal> s_buffer_operation_real_not_equal;
typedef s_buffer_operation_bool_bool_bool<s_op_bool_equal> s_buffer_operation_bool_equal;
typedef s_buffer_operation_bool_bool_bool<s_op_bool_not_equal> s_buffer_operation_bool_not_equal;
typedef s_buffer_operation_real_real_bool<s_op_greater> s_buffer_operation_greater;
typedef s_buffer_operation_real_real_bool<s_op_less> s_buffer_operation_less;
typedef s_buffer_operation_real_real_bool<s_op_greater_equal> s_buffer_operation_greater_equal;
typedef s_buffer_operation_real_real_bool<s_op_less_equal> s_buffer_operation_less_equal;
typedef s_buffer_operation_bool_bool_bool<s_op_and> s_buffer_operation_and;
typedef s_buffer_operation_bool_bool_bool<s_op_or> s_buffer_operation_or;

static void task_function_not_in_out(const s_task_function_context &context) {
	s_buffer_operation_not::in_out(context.buffer_size, context.arguments[0].get_bool_buffer_or_constant_in(), context.arguments[1].get_bool_buffer_out());
}

static void task_function_not_inout(const s_task_function_context &context) {
	s_buffer_operation_not::inout(context.buffer_size, context.arguments[0].get_bool_buffer_inout());
}

static void task_function_real_equal_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_real_equal::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_bool_buffer_out());
}

static void task_function_real_not_equal_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_real_not_equal::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_bool_buffer_out());
}

static void task_function_bool_equal_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_bool_equal::in_in_out(context.buffer_size, context.arguments[0].get_bool_buffer_or_constant_in(), context.arguments[1].get_bool_buffer_or_constant_in(), context.arguments[2].get_bool_buffer_out());
}

static void task_function_bool_equal_inout_in(const s_task_function_context &context) {
	s_buffer_operation_bool_equal::inout_in(context.buffer_size, context.arguments[0].get_bool_buffer_inout(), context.arguments[1].get_bool_buffer_or_constant_in());
}

static void task_function_bool_equal_in_inout(const s_task_function_context &context) {
	s_buffer_operation_bool_equal::in_inout(context.buffer_size, context.arguments[0].get_bool_buffer_or_constant_in(), context.arguments[1].get_bool_buffer_inout());
}

static void task_function_bool_not_equal_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_bool_not_equal::in_in_out(context.buffer_size, context.arguments[0].get_bool_buffer_or_constant_in(), context.arguments[1].get_bool_buffer_or_constant_in(), context.arguments[2].get_bool_buffer_out());
}

static void task_function_bool_not_equal_inout_in(const s_task_function_context &context) {
	s_buffer_operation_bool_not_equal::inout_in(context.buffer_size, context.arguments[0].get_bool_buffer_inout(), context.arguments[1].get_bool_buffer_or_constant_in());
}

static void task_function_bool_not_equal_in_inout(const s_task_function_context &context) {
	s_buffer_operation_bool_not_equal::in_inout(context.buffer_size, context.arguments[0].get_bool_buffer_or_constant_in(), context.arguments[1].get_bool_buffer_inout());
}

static void task_function_greater_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_greater::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_bool_buffer_out());
}

static void task_function_less_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_less::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_bool_buffer_out());
}

static void task_function_greater_equal_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_greater_equal::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_bool_buffer_out());
}

static void task_function_less_equal_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_less_equal::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_bool_buffer_out());
}

static void task_function_and_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_and::in_in_out(context.buffer_size, context.arguments[0].get_bool_buffer_or_constant_in(), context.arguments[1].get_bool_buffer_or_constant_in(), context.arguments[2].get_bool_buffer_out());
}

static void task_function_and_inout_in(const s_task_function_context &context) {
	s_buffer_operation_and::inout_in(context.buffer_size, context.arguments[0].get_bool_buffer_inout(), context.arguments[1].get_bool_buffer_or_constant_in());
}

static void task_function_and_in_inout(const s_task_function_context &context) {
	s_buffer_operation_and::in_inout(context.buffer_size, context.arguments[0].get_bool_buffer_or_constant_in(), context.arguments[1].get_bool_buffer_inout());
}

static void task_function_or_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_or::in_in_out(context.buffer_size, context.arguments[0].get_bool_buffer_or_constant_in(), context.arguments[1].get_bool_buffer_or_constant_in(), context.arguments[2].get_bool_buffer_out());
}

static void task_function_or_inout_in(const s_task_function_context &context) {
	s_buffer_operation_or::inout_in(context.buffer_size, context.arguments[0].get_bool_buffer_inout(), context.arguments[1].get_bool_buffer_or_constant_in());
}

static void task_function_or_in_inout(const s_task_function_context &context) {
	s_buffer_operation_or::in_inout(context.buffer_size, context.arguments[0].get_bool_buffer_or_constant_in(), context.arguments[1].get_bool_buffer_inout());
}

static void register_task_functions_basic_bool() {
	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_not_in_out_uid,
				"not_in_out",
				nullptr, nullptr, nullptr, task_function_not_in_out,
				s_task_function_argument_list::build(TDT(in, bool), TDT(out, bool))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_not_inout_uid,
				"not_inout",
				nullptr, nullptr, nullptr, task_function_not_inout,
				s_task_function_argument_list::build(TDT(inout, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_not_inout_uid, "b.",
				s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_not_in_out_uid, "v.",
				s_task_function_native_module_argument_mapping::build(0, 1))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_not_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_real_equal_in_in_out_uid,
				"real_equal_in_in_out",
				nullptr, nullptr, nullptr, task_function_real_equal_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_real_equal_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_real_equal_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_real_not_equal_in_in_out_uid,
				"real_not_equal_in_in_out",
				nullptr, nullptr, nullptr, task_function_real_not_equal_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_real_not_equal_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_real_not_equal_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_bool_equal_in_in_out_uid,
				"bool_equal_in_in_out",
				nullptr, nullptr, nullptr, task_function_bool_equal_in_in_out,
				s_task_function_argument_list::build(TDT(in, bool), TDT(in, bool), TDT(out, bool))));
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_bool_equal_inout_in_uid,
				"bool_equal_inout_in",
				nullptr, nullptr, nullptr, task_function_bool_equal_inout_in,
				s_task_function_argument_list::build(TDT(inout, bool), TDT(in, bool))));
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_bool_equal_in_inout_uid,
				"bool_equal_in_inout",
				nullptr, nullptr, nullptr, task_function_bool_equal_in_inout,
				s_task_function_argument_list::build(TDT(in, bool), TDT(inout, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_bool_equal_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_bool_equal_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_bool_equal_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_bool_equal_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_bool_not_equal_in_in_out_uid,
				"bool_not_equal_in_in_out",
				nullptr, nullptr, nullptr, task_function_bool_not_equal_in_in_out,
				s_task_function_argument_list::build(TDT(in, bool), TDT(in, bool), TDT(out, bool))));
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_bool_not_equal_inout_in_uid,
				"bool_not_equal_inout_in",
				nullptr, nullptr, nullptr, task_function_bool_not_equal_inout_in,
				s_task_function_argument_list::build(TDT(inout, bool), TDT(in, bool))));
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_bool_not_equal_in_inout_uid,
				"bool_not_equal_in_inout",
				nullptr, nullptr, nullptr, task_function_bool_not_equal_in_inout,
				s_task_function_argument_list::build(TDT(in, bool), TDT(inout, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_bool_not_equal_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_bool_not_equal_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_bool_not_equal_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_bool_not_equal_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_greater_in_in_out_uid,
				"greater_in_in_out",
				nullptr, nullptr, nullptr, task_function_greater_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_greater_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_greater_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_less_in_in_out_uid,
				"less_in_in_out",
				nullptr, nullptr, nullptr, task_function_less_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_less_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_less_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_greater_equal_in_in_out_uid,
				"greater_equal_in_in_out",
				nullptr, nullptr, nullptr, task_function_greater_equal_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_greater_equal_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_greater_equal_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_less_equal_in_in_out_uid,
				"less_equal_in_in_out",
				nullptr, nullptr, nullptr, task_function_less_equal_in_in_out,
				s_task_function_argument_list::build(TDT(in, real), TDT(in, real), TDT(out, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_less_equal_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_less_equal_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_and_in_in_out_uid,
				"and_in_in_out",
				nullptr, nullptr, nullptr, task_function_and_in_in_out,
				s_task_function_argument_list::build(TDT(in, bool), TDT(in, bool), TDT(out, bool))));
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_and_inout_in_uid,
				"and_inout_in",
				nullptr, nullptr, nullptr, task_function_and_inout_in,
				s_task_function_argument_list::build(TDT(inout, bool), TDT(in, bool))));
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_and_in_inout_uid,
				"and_in_inout",
				nullptr, nullptr, nullptr, task_function_and_in_inout,
				s_task_function_argument_list::build(TDT(in, bool), TDT(inout, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_and_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_and_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_and_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_and_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_or_in_in_out_uid,
				"or_in_in_out",
				nullptr, nullptr, nullptr, task_function_or_in_in_out,
				s_task_function_argument_list::build(TDT(in, bool), TDT(in, bool), TDT(out, bool))));
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_or_inout_in_uid,
				"or_inout_in",
				nullptr, nullptr, nullptr, task_function_or_inout_in,
				s_task_function_argument_list::build(TDT(inout, bool), TDT(in, bool))));
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_or_in_inout_uid,
				"or_in_inout",
				nullptr, nullptr, nullptr, task_function_or_in_inout,
				s_task_function_argument_list::build(TDT(in, bool), TDT(inout, bool))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_or_inout_in_uid, "bv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_or_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_or_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_or_uid, c_task_function_mapping_list::construct(mappings));
	}
}