#include "engine/task_functions/task_functions_utility.h"
#include "execution_graph/native_modules/native_modules_utility.h"
#include "engine/task_function_registry.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"

static const uint32 k_task_functions_utility_library_id = 1;

static const s_task_function_uid k_task_function_abs_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 0);
static const s_task_function_uid k_task_function_abs_bufferio_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 1);

static const s_task_function_uid k_task_function_floor_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 10);
static const s_task_function_uid k_task_function_floor_bufferio_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 11);

static const s_task_function_uid k_task_function_ceil_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 20);
static const s_task_function_uid k_task_function_ceil_bufferio_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 21);

static const s_task_function_uid k_task_function_round_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 30);
static const s_task_function_uid k_task_function_round_bufferio_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 31);

static const s_task_function_uid k_task_function_min_buffer_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 40);
static const s_task_function_uid k_task_function_min_bufferio_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 41);
static const s_task_function_uid k_task_function_min_buffer_constant_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 42);
static const s_task_function_uid k_task_function_min_bufferio_constant_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 43);

static const s_task_function_uid k_task_function_max_buffer_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 50);
static const s_task_function_uid k_task_function_max_bufferio_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 51);
static const s_task_function_uid k_task_function_max_buffer_constant_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 52);
static const s_task_function_uid k_task_function_max_bufferio_constant_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 53);

static const s_task_function_uid k_task_function_exp_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 60);
static const s_task_function_uid k_task_function_exp_bufferio_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 61);

static const s_task_function_uid k_task_function_log_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 70);
static const s_task_function_uid k_task_function_log_bufferio_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 71);

static const s_task_function_uid k_task_function_sqrt_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 80);
static const s_task_function_uid k_task_function_sqrt_bufferio_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 81);

static const s_task_function_uid k_task_function_pow_buffer_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 90);
static const s_task_function_uid k_task_function_pow_bufferio_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 91);
static const s_task_function_uid k_task_function_pow_buffer_bufferio_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 92);
static const s_task_function_uid k_task_function_pow_buffer_constant_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 93);
static const s_task_function_uid k_task_function_pow_bufferio_constant_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 94);
static const s_task_function_uid k_task_function_pow_constant_buffer_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 95);
static const s_task_function_uid k_task_function_pow_constant_bufferio_uid = s_task_function_uid::build(k_task_functions_utility_library_id, 96);

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

struct s_op_pow_constant_base {
	c_real32_4 operator()(const c_real32_4 &log_a, const c_real32_4 &b) const { return exp(b * log_a); }
};

struct s_op_pow_reverse {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &b) const { return pow(b, a); }
};

struct s_op_pow_constant_base_reverse {
	c_real32_4 operator()(const c_real32_4 &a, const c_real32_4 &log_b) const { return exp(a * log_b); }
};

typedef s_buffer_operation_1_input<s_op_abs> s_buffer_operation_abs;
typedef s_buffer_operation_1_input<s_op_floor> s_buffer_operation_floor;
typedef s_buffer_operation_1_input<s_op_ceil> s_buffer_operation_ceil;
typedef s_buffer_operation_1_input<s_op_round> s_buffer_operation_round;
typedef s_buffer_operation_2_inputs<s_op_min> s_buffer_operation_min;
typedef s_buffer_operation_2_inputs<s_op_max> s_buffer_operation_max;
typedef s_buffer_operation_1_input<s_op_exp> s_buffer_operation_exp;
typedef s_buffer_operation_1_input<s_op_log > s_buffer_operation_log;
typedef s_buffer_operation_1_input<s_op_sqrt> s_buffer_operation_sqrt;
//typedef s_buffer_operation_2_inputs<s_op_pow, s_op_pow_reverse> s_buffer_operation_pow;

// Exponentiation is special because it is cheaper if the base is constant
struct s_buffer_operation_pow {
	static void buffer_buffer(size_t buffer_size, c_buffer_out out, c_buffer_in in_a, c_buffer_in in_b) {
		if (in_a->is_constant()) {
			const real32 *in_a_ptr = in_a->get_data<real32>();
			real32 log_in_a = std::log(*in_a_ptr);
			buffer_operator_out_in_in(s_op_pow_constant_base_reverse(), buffer_size, out, in_b, log_in_a);
		} else {
			buffer_operator_out_in_in(s_op_pow(), buffer_size, out, in_a, in_b);
		}
	}

	static void bufferio_buffer(size_t buffer_size, c_buffer_inout inout, c_buffer_in in) {
		if (inout->is_constant()) {
			const real32 *inout_ptr = inout->get_data<real32>();
			real32 log_inout = std::log(*inout_ptr);
			buffer_operator_out_in_in(s_op_pow_constant_base_reverse(), buffer_size, inout, in, log_inout);
		} else {
			buffer_operator_inout_in(s_op_pow(), buffer_size, inout, in);
		}
	}

	static void buffer_bufferio(size_t buffer_size, c_buffer_in in, c_buffer_inout inout) {
		if (in->is_constant()) {
			const real32 *in_ptr = in->get_data<real32>();
			real32 log_in = std::log(*in_ptr);
			buffer_operator_inout_in(s_op_pow_constant_base_reverse(), buffer_size, inout, log_in);
		} else {
			buffer_operator_inout_in(s_op_pow_reverse(), buffer_size, inout, in);
		}
	}

	static void buffer_constant(size_t buffer_size, c_buffer_out out, c_buffer_in in_a, real32 in_b) {
		// If in_a is constant, we will call the non-constant-base version, but only once, so it's fine
		buffer_operator_out_in_in(s_op_pow(), buffer_size, out, in_a, in_b);
	}

	static void bufferio_constant(size_t buffer_size, c_buffer_inout inout, real32 in) {
		// If inout is constant, we will call the non-constant-base version, but only once, so it's fine
		buffer_operator_inout_in(s_op_pow(), buffer_size, inout, in);
	}

	static void constant_buffer(size_t buffer_size, c_buffer_out out, real32 in_a, c_buffer_in in_b) {
		real32 log_in_a = std::log(in_a);
		buffer_operator_out_in_in(s_op_pow_constant_base_reverse(), buffer_size, out, in_b, log_in_a);
	}

	static void constant_bufferio(size_t buffer_size, real32 in, c_buffer_inout inout) {
		real32 log_in = std::log(in);
		buffer_operator_inout_in(s_op_pow_constant_base_reverse(), buffer_size, inout, log_in);
	}
};

static void task_function_abs_buffer(const s_task_function_context &context) {
	s_buffer_operation_abs::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_abs_bufferio(const s_task_function_context &context) {
	s_buffer_operation_abs::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_floor_buffer(const s_task_function_context &context) {
	s_buffer_operation_floor::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_floor_bufferio(const s_task_function_context &context) {
	s_buffer_operation_floor::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_ceil_buffer(const s_task_function_context &context) {
	s_buffer_operation_ceil::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_ceil_bufferio(const s_task_function_context &context) {
	s_buffer_operation_ceil::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_round_buffer(const s_task_function_context &context) {
	s_buffer_operation_round::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_round_bufferio(const s_task_function_context &context) {
	s_buffer_operation_round::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_min_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_min::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_min_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_min::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_min_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_min::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_min_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_min::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_max_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_max::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_max_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_max::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_max_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_max::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_max_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_max::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_exp_buffer(const s_task_function_context &context) {
	s_buffer_operation_exp::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_exp_bufferio(const s_task_function_context &context) {
	s_buffer_operation_exp::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_log_buffer(const s_task_function_context &context) {
	s_buffer_operation_log::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_log_bufferio(const s_task_function_context &context) {
	s_buffer_operation_log::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_sqrt_buffer(const s_task_function_context &context) {
	s_buffer_operation_sqrt::buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in());
}

static void task_function_sqrt_bufferio(const s_task_function_context &context) {
	s_buffer_operation_sqrt::bufferio(context.buffer_size, context.arguments[0].get_real_buffer_inout());
}

static void task_function_pow_buffer_buffer(const s_task_function_context &context) {
	s_buffer_operation_pow::buffer_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_pow_bufferio_buffer(const s_task_function_context &context) {
	s_buffer_operation_pow::bufferio_buffer(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_buffer_in());
}

static void task_function_pow_buffer_bufferio(const s_task_function_context &context) {
	s_buffer_operation_pow::buffer_bufferio(context.buffer_size, context.arguments[0].get_real_buffer_in(), context.arguments[1].get_real_buffer_inout());
}

static void task_function_pow_buffer_constant(const s_task_function_context &context) {
	s_buffer_operation_pow::buffer_constant(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_buffer_in(), context.arguments[2].get_real_constant_in());
}

static void task_function_pow_bufferio_constant(const s_task_function_context &context) {
	s_buffer_operation_pow::bufferio_constant(context.buffer_size, context.arguments[0].get_real_buffer_inout(), context.arguments[1].get_real_constant_in());
}

static void task_function_pow_constant_buffer(const s_task_function_context &context) {
	s_buffer_operation_pow::constant_buffer(context.buffer_size, context.arguments[0].get_real_buffer_out(), context.arguments[1].get_real_constant_in(), context.arguments[2].get_real_buffer_in());
}

static void task_function_pow_constant_bufferio(const s_task_function_context &context) {
	s_buffer_operation_pow::constant_bufferio(context.buffer_size, context.arguments[0].get_real_constant_in(), context.arguments[1].get_real_buffer_inout());
}

void register_task_functions_utility() {
	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_abs_buffer_uid,
				nullptr, nullptr, task_function_abs_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_abs_bufferio_uid,
				nullptr, nullptr, task_function_abs_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_abs_bufferio_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_abs_buffer_uid, "v.",
				s_task_function_native_module_argument_mapping::build(1, 0))
		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_abs_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_floor_buffer_uid,
				nullptr, nullptr, task_function_floor_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_floor_bufferio_uid,
				nullptr, nullptr, task_function_floor_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_floor_bufferio_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_floor_buffer_uid, "v.",
				s_task_function_native_module_argument_mapping::build(1, 0))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_floor_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_ceil_buffer_uid,
				nullptr, nullptr, task_function_ceil_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_ceil_bufferio_uid,
				nullptr, nullptr, task_function_ceil_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_ceil_bufferio_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_ceil_buffer_uid, "v.",
				s_task_function_native_module_argument_mapping::build(1, 0))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_ceil_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_round_buffer_uid,
				nullptr, nullptr, task_function_round_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_round_bufferio_uid,
				nullptr, nullptr, task_function_round_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_round_bufferio_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_round_buffer_uid, "v.",
				s_task_function_native_module_argument_mapping::build(1, 0))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_round_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_min_buffer_buffer_uid,
				nullptr, nullptr, task_function_min_buffer_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_min_bufferio_buffer_uid,
				nullptr, nullptr, task_function_min_bufferio_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_min_buffer_constant_uid,
				nullptr, nullptr, task_function_min_buffer_constant,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_min_bufferio_constant_uid,
				nullptr, nullptr, task_function_min_bufferio_constant,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_constant_in))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_min_bufferio_buffer_uid, "bv.",
			s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_min_bufferio_buffer_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_min_buffer_buffer_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_min_bufferio_constant_uid, "bc.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_min_buffer_constant_uid, "vc.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_min_bufferio_constant_uid, "cb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_min_buffer_constant_uid, "cv.",
				s_task_function_native_module_argument_mapping::build(2, 1, 0))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_min_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_max_buffer_buffer_uid,
				nullptr, nullptr, task_function_max_buffer_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_max_bufferio_buffer_uid,
				nullptr, nullptr, task_function_max_bufferio_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_max_buffer_constant_uid,
				nullptr, nullptr, task_function_max_buffer_constant,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_max_bufferio_constant_uid,
				nullptr, nullptr, task_function_max_bufferio_constant,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_constant_in))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_max_bufferio_buffer_uid, "bv.",
			s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_max_bufferio_buffer_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_max_buffer_buffer_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_max_bufferio_constant_uid, "bc.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_max_buffer_constant_uid, "vc.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_max_bufferio_constant_uid, "cb.",
				s_task_function_native_module_argument_mapping::build(1, 0, 0)),

			s_task_function_mapping::build(k_task_function_max_buffer_constant_uid, "cv.",
				s_task_function_native_module_argument_mapping::build(2, 1, 0))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_max_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_exp_buffer_uid,
				nullptr, nullptr, task_function_exp_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_exp_bufferio_uid,
				nullptr, nullptr, task_function_exp_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_exp_bufferio_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_exp_buffer_uid, "v.",
				s_task_function_native_module_argument_mapping::build(1, 0))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_exp_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_log_buffer_uid,
				nullptr, nullptr, task_function_log_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_log_bufferio_uid,
				nullptr, nullptr, task_function_log_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_log_bufferio_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_log_buffer_uid, "v.",
				s_task_function_native_module_argument_mapping::build(1, 0))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_log_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_sqrt_buffer_uid,
				nullptr, nullptr, task_function_sqrt_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_sqrt_bufferio_uid,
				nullptr, nullptr, task_function_sqrt_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_sqrt_bufferio_uid, "b.",
			s_task_function_native_module_argument_mapping::build(0, 0)),

			s_task_function_mapping::build(k_task_function_sqrt_buffer_uid, "v.",
				s_task_function_native_module_argument_mapping::build(1, 0))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_sqrt_uid, c_task_function_mapping_list::construct(mappings));
	}

	{
		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_pow_buffer_buffer_uid,
				nullptr, nullptr, task_function_pow_buffer_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_pow_bufferio_buffer_uid,
				nullptr, nullptr, task_function_pow_bufferio_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_pow_buffer_bufferio_uid,
				nullptr, nullptr, task_function_pow_buffer_bufferio,
				s_task_function_argument_list::build(TDT(real_buffer_in), TDT(real_buffer_inout))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_pow_buffer_constant_uid,
				nullptr, nullptr, task_function_pow_buffer_constant,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_buffer_in), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_pow_bufferio_constant_uid,
				nullptr, nullptr, task_function_pow_bufferio_constant,
				s_task_function_argument_list::build(TDT(real_buffer_inout), TDT(real_constant_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_pow_constant_buffer_uid,
				nullptr, nullptr, task_function_pow_constant_buffer,
				s_task_function_argument_list::build(TDT(real_buffer_out), TDT(real_constant_in), TDT(real_buffer_in))));

		c_task_function_registry::register_task_function(
			s_task_function::build(k_task_function_pow_constant_bufferio_uid,
				nullptr, nullptr, task_function_pow_constant_bufferio,
				s_task_function_argument_list::build(TDT(real_constant_in), TDT(real_buffer_inout))));

		s_task_function_mapping mappings[] = {
			s_task_function_mapping::build(k_task_function_pow_bufferio_buffer_uid, "bv.",
			s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_pow_buffer_bufferio_uid, "vb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_pow_buffer_buffer_uid, "vv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_pow_bufferio_constant_uid, "bc.",
				s_task_function_native_module_argument_mapping::build(0, 1, 0)),

			s_task_function_mapping::build(k_task_function_pow_buffer_constant_uid, "vc.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0)),

			s_task_function_mapping::build(k_task_function_pow_constant_bufferio_uid, "cb.",
				s_task_function_native_module_argument_mapping::build(0, 1, 1)),

			s_task_function_mapping::build(k_task_function_pow_constant_buffer_uid, "cv.",
				s_task_function_native_module_argument_mapping::build(1, 2, 0))

		};

		c_task_function_registry::register_task_function_mapping_list(
			k_native_module_pow_uid, c_task_function_mapping_list::construct(mappings));
	}
}