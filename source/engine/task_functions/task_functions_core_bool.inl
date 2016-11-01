struct s_op_not {
	c_int32_4 operator()(const c_int32_4 &a) const { return ~a; }
};

struct s_op_equal_real {
	c_int32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a == b; }
};

struct s_op_not_equal_real {
	c_int32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return a != b; }
};

struct s_op_equal_bool {
	c_int32_4 operator()(const c_int32_4 &a, const c_int32_4 &b) const { return a == b; }
};

struct s_op_not_equal_bool {
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
typedef s_buffer_operation_real_real_bool<s_op_equal_real> s_buffer_operation_equal_real;
typedef s_buffer_operation_real_real_bool<s_op_not_equal_real> s_buffer_operation_not_equal_real;
typedef s_buffer_operation_bool_bool_bool<s_op_equal_bool> s_buffer_operation_equal_bool;
typedef s_buffer_operation_bool_bool_bool<s_op_not_equal_bool> s_buffer_operation_not_equal_bool;
typedef s_buffer_operation_real_real_bool<s_op_greater> s_buffer_operation_greater;
typedef s_buffer_operation_real_real_bool<s_op_less> s_buffer_operation_less;
typedef s_buffer_operation_real_real_bool<s_op_greater_equal> s_buffer_operation_greater_equal;
typedef s_buffer_operation_real_real_bool<s_op_less_equal> s_buffer_operation_less_equal;
typedef s_buffer_operation_bool_bool_bool<s_op_and> s_buffer_operation_and;
typedef s_buffer_operation_bool_bool_bool<s_op_or> s_buffer_operation_or;

namespace core_task_functions {

	void not_in_out(const s_task_function_context &context,
		c_bool_const_buffer_or_constant a,
		c_bool_buffer *result) {
		s_buffer_operation_not::in_out(context.buffer_size, a, result);
	}

	void not_inout(const s_task_function_context &context,
		c_bool_buffer *a_result) {
		s_buffer_operation_not::inout(context.buffer_size, a_result);
	}

	void equal_real_in_in_out(const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_bool_buffer *result) {
		s_buffer_operation_equal_real::in_in_out(context.buffer_size, a, b, result);
	}

	void not_equal_real_in_in_out(const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_bool_buffer *result) {
		s_buffer_operation_not_equal_real::in_in_out(context.buffer_size, a, b, result);
	}

	void equal_bool_in_in_out(const s_task_function_context &context,
		c_bool_const_buffer_or_constant a,
		c_bool_const_buffer_or_constant b,
		c_bool_buffer *result) {
		s_buffer_operation_equal_bool::in_in_out(context.buffer_size, a, b, result);
	}

	void equal_bool_inout_in(const s_task_function_context &context,
		c_bool_buffer *a_result,
		c_bool_const_buffer_or_constant b) {
		s_buffer_operation_equal_bool::inout_in(context.buffer_size, a_result, b);
	}

	void equal_bool_in_inout(const s_task_function_context &context,
		c_bool_const_buffer_or_constant a,
		c_bool_buffer *b_result) {
		s_buffer_operation_equal_bool::in_inout(context.buffer_size, a, b_result);
	}

	void not_equal_bool_in_in_out(const s_task_function_context &context,
		c_bool_const_buffer_or_constant a,
		c_bool_const_buffer_or_constant b,
		c_bool_buffer *result) {
		s_buffer_operation_not_equal_bool::in_in_out(context.buffer_size, a, b, result);
	}

	void not_equal_bool_inout_in(const s_task_function_context &context,
		c_bool_buffer *a_result,
		c_bool_const_buffer_or_constant b) {
		s_buffer_operation_not_equal_bool::inout_in(context.buffer_size, a_result, b);
	}

	void not_equal_bool_in_inout(const s_task_function_context &context,
		c_bool_const_buffer_or_constant a,
		c_bool_buffer *b_result) {
		s_buffer_operation_not_equal_bool::in_inout(context.buffer_size, a, b_result);
	}

	void greater_in_in_out(const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_bool_buffer *result) {
		s_buffer_operation_greater::in_in_out(context.buffer_size, a, b, result);
	}

	void less_in_in_out(const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_bool_buffer *result) {
		s_buffer_operation_less::in_in_out(context.buffer_size, a, b, result);
	}

	void greater_equal_in_in_out(const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_bool_buffer *result) {
		s_buffer_operation_greater_equal::in_in_out(context.buffer_size, a, b, result);
	}

	void less_equal_in_in_out(const s_task_function_context &context,
		c_real_const_buffer_or_constant a,
		c_real_const_buffer_or_constant b,
		c_bool_buffer *result) {
		s_buffer_operation_less_equal::in_in_out(context.buffer_size, a, b, result);
	}

	void and_in_in_out(const s_task_function_context &context,
		c_bool_const_buffer_or_constant a,
		c_bool_const_buffer_or_constant b,
		c_bool_buffer *result) {
		s_buffer_operation_and::in_in_out(context.buffer_size, a, b, result);
	}

	void and_inout_in(const s_task_function_context &context,
		c_bool_buffer *a_result,
		c_bool_const_buffer_or_constant b) {
		s_buffer_operation_and::inout_in(context.buffer_size, a_result, b);
	}

	void and_in_inout(const s_task_function_context &context,
		c_bool_const_buffer_or_constant a,
		c_bool_buffer *b_result) {
		s_buffer_operation_and::in_inout(context.buffer_size, a, b_result);
	}

	void or_in_in_out(const s_task_function_context &context,
		c_bool_const_buffer_or_constant a,
		c_bool_const_buffer_or_constant b,
		c_bool_buffer *result) {
		s_buffer_operation_or::in_in_out(context.buffer_size, a, b, result);
	}

	void or_inout_in(const s_task_function_context &context,
		c_bool_buffer *a_result,
		c_bool_const_buffer_or_constant b) {
		s_buffer_operation_or::inout_in(context.buffer_size, a_result, b);
	}

	void or_in_inout(const s_task_function_context &context,
		c_bool_const_buffer_or_constant a,
		c_bool_buffer *b_result) {
		s_buffer_operation_or::in_inout(context.buffer_size, a, b_result);
	}

}
