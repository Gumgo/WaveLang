#include "engine/task_functions/task_functions_utility.h"
#include "execution_graph/native_modules/native_modules_utility.h"
#include "engine/task_function_registry.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"

static const uint32 k_task_functions_utility_library_id = 1;

static const s_task_function_uid k_task_function_abs_in_out_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 0);
static const s_task_function_uid k_task_function_abs_inout_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 1);

static const s_task_function_uid k_task_function_floor_in_out_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 10);
static const s_task_function_uid k_task_function_floor_inout_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 11);

static const s_task_function_uid k_task_function_ceil_in_out_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 20);
static const s_task_function_uid k_task_function_ceil_inout_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 21);

static const s_task_function_uid k_task_function_round_in_out_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 30);
static const s_task_function_uid k_task_function_round_inout_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 31);

static const s_task_function_uid k_task_function_min_in_in_out_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 40);
static const s_task_function_uid k_task_function_min_inout_in_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 41);

static const s_task_function_uid k_task_function_max_in_in_out_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 50);
static const s_task_function_uid k_task_function_max_inout_in_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 51);

static const s_task_function_uid k_task_function_exp_in_out_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 60);
static const s_task_function_uid k_task_function_exp_inout_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 61);

static const s_task_function_uid k_task_function_log_in_out_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 70);
static const s_task_function_uid k_task_function_log_inout_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 71);

static const s_task_function_uid k_task_function_sqrt_in_out_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 80);
static const s_task_function_uid k_task_function_sqrt_inout_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 81);

static const s_task_function_uid k_task_function_pow_in_in_out_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 90);
static const s_task_function_uid k_task_function_pow_inout_in_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 91);
static const s_task_function_uid k_task_function_pow_in_inout_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 92);

struct s_op_abs {
	c_real32_4 operator()(const c_real32_4 &a) const { return abs(a); }
};

struct s_op_floor {
	c_real32_4 operator()(const c_real32_4 &a) const { return floor(a); }
};

struct s_op_ceil {
	c_real32_4 operator()(const c_real32_4 &a) const { return ceil(a); }
};

struct s_op_round {
	c_real32_4 operator()(const c_real32_4 &a) const { return round(a); }
};

struct s_op_min {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return min(a, b); }
};

struct s_op_max {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return max(a, b); }
};

struct s_op_exp {
	c_real32_4 operator()(const c_real32_4 &a) const { return exp(a); }
};

struct s_op_log {
	c_real32_4 operator()(const c_real32_4 &a) const { return log(a); }
};

struct s_op_sqrt {
	c_real32_4 operator()(const c_real32_4 &a) const { return sqrt(a); }
};

struct s_op_pow {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return pow(a, b); }
};

struct s_op_pow_reverse {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return pow(b, a); }
};

struct s_op_pow_constant_base {
	c_real32_4 log_a;
	s_op_pow_constant_base(real32 a) : log_a(std::log(a)) {}
	c_real32_4 operator()(const c_real32_4 &b) const { return exp(log_a * b); }
};

typedef s_buffer_operation_real<s_op_abs> s_buffer_operation_abs;
typedef s_buffer_operation_real <s_op_floor> s_buffer_operation_floor;
typedef s_buffer_operation_real <s_op_ceil> s_buffer_operation_ceil;
typedef s_buffer_operation_real<s_op_round> s_buffer_operation_round;
typedef s_buffer_operation_real_real<s_op_min> s_buffer_operation_min;
typedef s_buffer_operation_real_real<s_op_max> s_buffer_operation_max;
typedef s_buffer_operation_real<s_op_exp> s_buffer_operation_exp;
typedef s_buffer_operation_real<s_op_log > s_buffer_operation_log;
typedef s_buffer_operation_real<s_op_sqrt> s_buffer_operation_sqrt;
//typedef s_buffer_operation_2_inputs<s_op_pow, s_op_pow_reverse> s_buffer_operation_pow;

// Exponentiation is special because it is cheaper if the base is constant
struct s_buffer_operation_pow {
	static void in_in_out(size_t buffer_size, c_real_buffer_or_constant_in in_a, c_real_buffer_or_constant_in in_b,
		c_real_buffer_out out) {
		if (in_a.is_constant()) {
			real32 base = in_a.get_constant();
			buffer_operator_real_in_real_out(s_op_pow_constant_base(base), buffer_size, in_b, out);
		} else {
			buffer_operator_real_in_real_in_real_out(s_op_pow(), buffer_size, in_a, in_b, out);
		}
	}

	static void inout_in(size_t buffer_size, c_real_buffer_inout inout, c_real_buffer_or_constant_in in) {
		if (inout->is_constant()) {
			real32 base = *inout->get_data<real32>();
			buffer_operator_real_in_real_out(s_op_pow_constant_base(base), buffer_size, in, inout);
		} else {
			buffer_operator_real_inout_real_in(s_op_pow(), buffer_size, inout, in);
		}
	}

	static void in_inout(size_t buffer_size, c_real_buffer_or_constant_in in, c_real_buffer_inout inout) {
		if (in.is_constant()) {
			real32 base = in.get_constant();
			buffer_operator_real_inout(s_op_pow_constant_base(base), buffer_size, inout);
		} else {
			buffer_operator_real_inout_real_in(s_op_pow_reverse(), buffer_size, inout, in);
		}
	}
};

static void task_function_abs_in_out(const s_task_function_context &context) {
	s_buffer_operation_abs::in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_out());
}

static void task_function_abs_inout(const s_task_function_context &context) {
	s_buffer_operation_abs::inout(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_floor_in_out(const s_task_function_context &context) {
	s_buffer_operation_floor::in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_out());
}

static void task_function_floor_inout(const s_task_function_context &context) {
	s_buffer_operation_floor::inout(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_ceil_in_out(const s_task_function_context &context) {
	s_buffer_operation_ceil::in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_out());
}

static void task_function_ceil_inout(const s_task_function_context &context) {
	s_buffer_operation_ceil::inout(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_round_in_out(const s_task_function_context &context) {
	s_buffer_operation_round::in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_out());
}

static void task_function_round_inout(const s_task_function_context &context) {
	s_buffer_operation_round::inout(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_min_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_min::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_min_inout_in(const s_task_function_context &context) {
	s_buffer_operation_min::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_max_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_max::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_max_inout_in(const s_task_function_context &context) {
	s_buffer_operation_max::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_exp_in_out(const s_task_function_context &context) {
	s_buffer_operation_exp::in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_out());
}

static void task_function_exp_inout(const s_task_function_context &context) {
	s_buffer_operation_exp::inout(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_log_in_out(const s_task_function_context &context) {
	s_buffer_operation_log::in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_out());
}

static void task_function_log_inout(const s_task_function_context &context) {
	s_buffer_operation_log::inout(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_sqrt_in_out(const s_task_function_context &context) {
	s_buffer_operation_sqrt::in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_out());
}

static void task_function_sqrt_inout(const s_task_function_context &context) {
	s_buffer_operation_sqrt::inout(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_pow_in_in_out(const s_task_function_context &context) {
	s_buffer_operation_pow::in_in_out(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_or_constant_in(), context.arguments[2].get_real_buffer_out());
}

static void task_function_pow_inout_in(const s_task_function_context &context) {
	s_buffer_operation_pow::inout_in(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_or_constant_in());
}

static void task_function_pow_in_inout(const s_task_function_context &context) {
	s_buffer_operation_pow::in_inout(context.buffer_size, context.arguments[0].get_real_buffer_or_constant_in(), context.arguments[1].get_real_buffer_inout());
}

void register_task_functions_utility() {
	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_abs_in_out_uid,
				nullptr, nullptr, task_function_abs_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_abs_inout_uid,
				nullptr, nullptr, task_function_abs_inout,
				s_task_function_argument_list::build(TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_abs_inout_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_abs_in_out_uid, "v.",
				s_task_function_native_module_argument_mapping::build(0, 1))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_abs_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_floor_in_out_uid,
				nullptr, nullptr, task_function_floor_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_floor_inout_uid,
				nullptr, nullptr, task_function_floor_inout,
				s_task_function_argument_list::build(TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_floor_inout_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_floor_in_out_uid, "v.",
				s_task_function_native_module_argument_mapping::build(0, 1))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_floor_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_ceil_in_out_uid,
				nullptr, nullptr, task_function_ceil_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_ceil_inout_uid,
				nullptr, nullptr, task_function_ceil_inout,
				s_task_function_argument_list::build(TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_ceil_inout_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_ceil_in_out_uid, "v.",
				s_task_function_native_module_argument_mapping::build(0, 1))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_ceil_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_round_in_out_uid,
				nullptr, nullptr, task_function_round_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_round_inout_uid,
				nullptr, nullptr, task_function_round_inout,
				s_task_function_argument_list::build(TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_round_inout_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_round_in_out_uid, "v.",
				s_task_function_native_module_argument_mapping::build(0, 1))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_round_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_min_in_in_out_uid,
				nullptr, nullptr, task_function_min_in_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_min_inout_in_uid,
				nullptr, nullptr, task_function_min_inout_in,
				s_task_function_argument_list::build(TDT(real_inout), TDT(real_in))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_min_inout_in_uid, "bv.",
			s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_min_inout_in_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_min_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_min_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_max_in_in_out_uid,
				nullptr, nullptr, task_function_max_in_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_max_inout_in_uid,
				nullptr, nullptr, task_function_max_inout_in,
				s_task_function_argument_list::build(TDT(real_inout), TDT(real_in))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_max_inout_in_uid, "bv.",
			s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_max_inout_in_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_max_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_max_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_exp_in_out_uid,
				nullptr, nullptr, task_function_exp_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_exp_inout_uid,
				nullptr, nullptr, task_function_exp_inout,
				s_task_function_argument_list::build(TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_exp_inout_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_exp_in_out_uid, "v.",
				s_task_function_native_module_argument_mapping::build(0, 1))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_exp_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_log_in_out_uid,
				nullptr, nullptr, task_function_log_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_log_inout_uid,
				nullptr, nullptr, task_function_log_inout,
				s_task_function_argument_list::build(TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_log_inout_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_log_in_out_uid, "v.",
				s_task_function_native_module_argument_mapping::build(0, 1))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_log_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_sqrt_in_out_uid,
				nullptr, nullptr, task_function_sqrt_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_sqrt_inout_uid,
				nullptr, nullptr, task_function_sqrt_inout,
				s_task_function_argument_list::build(TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_sqrt_inout_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_sqrt_in_out_uid, "v.",
				s_task_function_native_module_argument_mapping::build(0, 1))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_sqrt_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_pow_in_in_out_uid,
				nullptr, nullptr, task_function_pow_in_in_out,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_in), TDT(real_out))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_pow_inout_in_uid,
				nullptr, nullptr, task_function_pow_inout_in,
				s_task_function_argument_list::build(TDT(real_inout), TDT(real_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_pow_in_inout_uid,
				nullptr, nullptr, task_function_pow_in_inout,
				s_task_function_argument_list::build(TDT(real_in), TDT(real_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_pow_inout_in_uid, "bv.",
			s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_pow_in_inout_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_pow_in_in_out_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(0, 1, 2)),

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_pow_uid, c_task_function_mapping_list::construct(mappings));
	}
}