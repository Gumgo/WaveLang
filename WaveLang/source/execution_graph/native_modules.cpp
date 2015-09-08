#include "execution_graph/native_modules.h"
#include <cmath>

#define NATIVE_PREFIX "__native_"

const char *c_native_module_registry::k_operator_noop_name = NATIVE_PREFIX "noop";
const char *c_native_module_registry::k_operator_negation_name = NATIVE_PREFIX "negation";
const char *c_native_module_registry::k_operator_addition_operator_name = NATIVE_PREFIX "addition";
const char *c_native_module_registry::k_operator_subtraction_operator_name = NATIVE_PREFIX "subtraction";
const char *c_native_module_registry::k_operator_multiplication_operator_name = NATIVE_PREFIX "multiplication";
const char *c_native_module_registry::k_operator_division_name = NATIVE_PREFIX "division";
const char *c_native_module_registry::k_operator_modulo_name = NATIVE_PREFIX "modulo";
const char *c_native_module_registry::k_operator_concatenation_name = NATIVE_PREFIX "concatenation";
const char *c_native_module_registry::k_operator_not_name = NATIVE_PREFIX "not";
const char *c_native_module_registry::k_operator_equal_name = NATIVE_PREFIX "equal";
const char *c_native_module_registry::k_operator_not_equal_name = NATIVE_PREFIX "not_equal";
const char *c_native_module_registry::k_operator_less_name = NATIVE_PREFIX "less";
const char *c_native_module_registry::k_operator_greater_name = NATIVE_PREFIX "greater";
const char *c_native_module_registry::k_operator_less_equal_name = NATIVE_PREFIX "less_equal";
const char *c_native_module_registry::k_operator_greater_equal_name = NATIVE_PREFIX "greater_equal";
const char *c_native_module_registry::k_operator_and_name = NATIVE_PREFIX "and";
const char *c_native_module_registry::k_operator_or_name = NATIVE_PREFIX "or";

// List of compile-time native modules
static void native_module_noop(c_native_module_compile_time_argument_list &arguments);
static void native_module_negation(c_native_module_compile_time_argument_list &arguments);
static void native_module_addition(c_native_module_compile_time_argument_list &arguments);
static void native_module_subtraction(c_native_module_compile_time_argument_list &arguments);
static void native_module_multiplication(c_native_module_compile_time_argument_list &arguments);
static void native_module_division(c_native_module_compile_time_argument_list &arguments);
static void native_module_modulo(c_native_module_compile_time_argument_list &arguments);
static void native_module_concatenation(c_native_module_compile_time_argument_list &arguments);

static void native_module_not(c_native_module_compile_time_argument_list &arguments);
static void native_module_real_equal(c_native_module_compile_time_argument_list &arguments);
static void native_module_real_not_equal(c_native_module_compile_time_argument_list &arguments);
static void native_module_bool_equal(c_native_module_compile_time_argument_list &arguments);
static void native_module_bool_not_equal(c_native_module_compile_time_argument_list &arguments);
static void native_module_string_equal(c_native_module_compile_time_argument_list &arguments);
static void native_module_string_not_equal(c_native_module_compile_time_argument_list &arguments);
static void native_module_greater(c_native_module_compile_time_argument_list &arguments);
static void native_module_less(c_native_module_compile_time_argument_list &arguments);
static void native_module_greater_equal(c_native_module_compile_time_argument_list &arguments);
static void native_module_less_equal(c_native_module_compile_time_argument_list &arguments);
static void native_module_and(c_native_module_compile_time_argument_list &arguments);
static void native_module_or(c_native_module_compile_time_argument_list &arguments);

static void native_module_abs(c_native_module_compile_time_argument_list &arguments);
static void native_module_floor(c_native_module_compile_time_argument_list &arguments);
static void native_module_ceil(c_native_module_compile_time_argument_list &arguments);
static void native_module_round(c_native_module_compile_time_argument_list &arguments);
static void native_module_min(c_native_module_compile_time_argument_list &arguments);
static void native_module_max(c_native_module_compile_time_argument_list &arguments);
static void native_module_exp(c_native_module_compile_time_argument_list &arguments);
static void native_module_log(c_native_module_compile_time_argument_list &arguments);
static void native_module_sqrt(c_native_module_compile_time_argument_list &arguments);
static void native_module_pow(c_native_module_compile_time_argument_list &arguments);
static void native_module_real_static_select(c_native_module_compile_time_argument_list &arguments);
static void native_module_string_static_select(c_native_module_compile_time_argument_list &arguments);

struct s_native_module_argument_list {
	s_native_module_argument arguments[k_max_native_module_arguments];
};

static s_native_module_argument make_arg();

static s_native_module_argument make_arg(
	e_native_module_argument_qualifier qualifier,
	e_native_module_argument_type type);

static s_native_module_argument_list make_args(
	s_native_module_argument arg_0 = make_arg(),
	s_native_module_argument arg_1 = make_arg(),
	s_native_module_argument arg_2 = make_arg(),
	s_native_module_argument arg_3 = make_arg(),
	s_native_module_argument arg_4 = make_arg(),
	s_native_module_argument arg_5 = make_arg(),
	s_native_module_argument arg_6 = make_arg(),
	s_native_module_argument arg_7 = make_arg(),
	s_native_module_argument arg_8 = make_arg(),
	s_native_module_argument arg_9 = make_arg());

static s_native_module make_native_module(
	const char *name,
	bool first_output_is_return,
	const s_native_module_argument_list &arguments,
	f_native_module_compile_time_call compile_time_call);

#define NMAT(qualifier, type) \
	make_arg(k_native_module_argument_qualifier_ ## qualifier, k_native_module_argument_type_ ## type)

// The global list of native modules
static const s_native_module k_native_modules[] = {
	make_native_module(c_native_module_registry::k_operator_noop_name,
		true, make_args(NMAT(in, real), NMAT(out, real)),
		native_module_noop),

	make_native_module(c_native_module_registry::k_operator_negation_name,
		true, make_args(NMAT(in, real), NMAT(out, real)),
		native_module_negation),

	make_native_module(c_native_module_registry::k_operator_addition_operator_name,
		true, make_args(NMAT(in, real), NMAT(in, real), NMAT(out, real)),
		native_module_addition),

	make_native_module(c_native_module_registry::k_operator_subtraction_operator_name,
		true, make_args(NMAT(in, real), NMAT(in, real), NMAT(out, real)),
		native_module_subtraction),

	make_native_module(c_native_module_registry::k_operator_multiplication_operator_name,
		true, make_args(NMAT(in, real), NMAT(in, real), NMAT(out, real)),
		native_module_multiplication),

	make_native_module(c_native_module_registry::k_operator_division_name,
		true, make_args(NMAT(in, real), NMAT(in, real), NMAT(out, real)),
		native_module_division),

	make_native_module(c_native_module_registry::k_operator_modulo_name,
		true, make_args(NMAT(in, real), NMAT(in, real), NMAT(out, real)),
		native_module_modulo),

	make_native_module(c_native_module_registry::k_operator_concatenation_name,
		true, make_args(NMAT(in, string), NMAT(in, string), NMAT(out, string)),
		native_module_concatenation),

	make_native_module(c_native_module_registry::k_operator_not_name,
		true, make_args(NMAT(constant, bool), NMAT(out, bool)),
		native_module_not),

	make_native_module(c_native_module_registry::k_operator_equal_name,
		true, make_args(NMAT(constant, real), NMAT(constant, real), NMAT(out, bool)),
		native_module_real_equal),

	make_native_module(c_native_module_registry::k_operator_not_equal_name,
		true, make_args(NMAT(constant, real), NMAT(constant, real), NMAT(out, bool)),
		native_module_real_not_equal),

	make_native_module(c_native_module_registry::k_operator_equal_name,
		true, make_args(NMAT(constant, bool), NMAT(constant, bool), NMAT(out, bool)),
		native_module_bool_equal),

	make_native_module(c_native_module_registry::k_operator_not_equal_name,
		true, make_args(NMAT(constant, bool), NMAT(constant, bool), NMAT(out, bool)),
		native_module_bool_not_equal),

	make_native_module(c_native_module_registry::k_operator_equal_name,
		true, make_args(NMAT(constant, string), NMAT(constant, string), NMAT(out, bool)),
		native_module_string_equal),

	make_native_module(c_native_module_registry::k_operator_not_equal_name,
		true, make_args(NMAT(constant, string), NMAT(constant, string), NMAT(out, bool)),
		native_module_string_not_equal),

	make_native_module(c_native_module_registry::k_operator_greater_name,
		true, make_args(NMAT(constant, real), NMAT(constant, real), NMAT(out, bool)),
		native_module_greater),

	make_native_module(c_native_module_registry::k_operator_less_name,
		true, make_args(NMAT(constant, real), NMAT(constant, real), NMAT(out, bool)),
		native_module_less),

	make_native_module(c_native_module_registry::k_operator_greater_equal_name,
		true, make_args(NMAT(constant, real), NMAT(constant, real), NMAT(out, bool)),
		native_module_greater_equal),

	make_native_module(c_native_module_registry::k_operator_less_equal_name,
		true, make_args(NMAT(constant, real), NMAT(constant, real), NMAT(out, bool)),
		native_module_less_equal),

	make_native_module(c_native_module_registry::k_operator_and_name,
		true, make_args(NMAT(constant, bool), NMAT(constant, bool), NMAT(out, bool)),
		native_module_and),

	make_native_module(c_native_module_registry::k_operator_or_name,
		true, make_args(NMAT(constant, bool), NMAT(constant, bool), NMAT(out, bool)),
		native_module_or),

	make_native_module(NATIVE_PREFIX "abs",
		true, make_args(NMAT(in, real), NMAT(out, real)),
		native_module_abs),

	make_native_module(NATIVE_PREFIX "floor",
		true, make_args(NMAT(in, real), NMAT(out, real)),
		native_module_floor),

	make_native_module(NATIVE_PREFIX "ceil",
		true, make_args(NMAT(in, real), NMAT(out, real)),
		native_module_ceil),

	make_native_module(NATIVE_PREFIX "round",
		true, make_args(NMAT(in, real), NMAT(out, real)),
		native_module_round),

	make_native_module(NATIVE_PREFIX "min",
		true, make_args(NMAT(in, real), NMAT(in, real), NMAT(out, real)),
		native_module_min),

	make_native_module(NATIVE_PREFIX "max",
		true, make_args(NMAT(in, real), NMAT(in, real), NMAT(out, real)),
		native_module_max),

	make_native_module(NATIVE_PREFIX "exp",
		true, make_args(NMAT(in, real), NMAT(out, real)),
		native_module_exp),

	make_native_module(NATIVE_PREFIX "log",
		true, make_args(NMAT(in, real), NMAT(out, real)),
		native_module_log),

	make_native_module(NATIVE_PREFIX "sqrt",
		true, make_args(NMAT(in, real), NMAT(out, real)),
		native_module_sqrt),

	make_native_module(NATIVE_PREFIX "pow",
		true, make_args(NMAT(in, real), NMAT(in, real), NMAT(out, real)),
		native_module_pow),

	make_native_module(NATIVE_PREFIX "static_select",
		true, make_args(NMAT(constant, bool), NMAT(in, real), NMAT(in, real), NMAT(out, real)),
		native_module_real_static_select),

	make_native_module(NATIVE_PREFIX "static_select",
		true, make_args(NMAT(constant, bool), NMAT(in, string), NMAT(in, string), NMAT(out, string)),
		native_module_string_static_select),

	make_native_module(NATIVE_PREFIX "sampler",
		true, make_args(
			NMAT(constant, string),	// Name
			NMAT(constant, real),	// Channel
			NMAT(in, real),			// Speed
			NMAT(out, real)),		// Result
		nullptr),

	make_native_module(NATIVE_PREFIX "sampler_loop",
		true, make_args(
			NMAT(constant, string),	// Name
			NMAT(constant, real),	// Channel
			NMAT(constant, bool),	// Bidi
			NMAT(in, real),			// Speed
			NMAT(out, real)),		// Result
		nullptr),

	make_native_module(NATIVE_PREFIX "sampler_loop_phase_shift",
		true, make_args(
			NMAT(constant, string),	// Name
			NMAT(constant, real),	// Channel
			NMAT(constant, bool),	// Bidi
			NMAT(in, real),			// Speed
			NMAT(in, real),			// Phase
			NMAT(out, real)),		// Result
		nullptr),
};

static_assert(NUMBEROF(k_native_modules) == k_native_module_count, "Native module definition list mismatch");

uint32 c_native_module_registry::get_native_module_count() {
	return NUMBEROF(k_native_modules);
}

const s_native_module &c_native_module_registry::get_native_module(uint32 index) {
	wl_assert(VALID_INDEX(index, get_native_module_count()));
	return k_native_modules[index];
}

static s_native_module_argument make_arg() {
	return make_arg(k_native_module_argument_qualifier_count, k_native_module_argument_type_count);
}

static s_native_module_argument make_arg(
	e_native_module_argument_qualifier qualifier,
	e_native_module_argument_type type) {
	s_native_module_argument arg = { qualifier, type };
	return arg;
}

static s_native_module_argument_list make_args(
	s_native_module_argument arg_0,
	s_native_module_argument arg_1,
	s_native_module_argument arg_2,
	s_native_module_argument arg_3,
	s_native_module_argument arg_4,
	s_native_module_argument arg_5,
	s_native_module_argument arg_6,
	s_native_module_argument arg_7,
	s_native_module_argument arg_8,
	s_native_module_argument arg_9) {
	s_native_module_argument_list arg_list = {
		arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8, arg_9
	};
	return arg_list;
}

static s_native_module make_native_module(
	const char *name,
	bool first_output_is_return,
	const s_native_module_argument_list &arguments,
	f_native_module_compile_time_call compile_time_call) {
	// First, zero-initialize
	s_native_module native_module;
	ZERO_STRUCT(&native_module);
	native_module.name = name;
	native_module.first_output_is_return = first_output_is_return;
	native_module.in_argument_count = 0;
	native_module.out_argument_count = 0;
	for (native_module.argument_count = 0;
		 native_module.argument_count < k_max_native_module_arguments;
		 native_module.argument_count++) {
		s_native_module_argument arg = arguments.arguments[native_module.argument_count];
		// Loop until we find an k_native_module_argument_qualifier_count
		if (arg.qualifier != k_native_module_argument_qualifier_count) {
			wl_assert(VALID_INDEX(arg.qualifier, k_native_module_argument_qualifier_count));
			wl_assert(VALID_INDEX(arg.type, k_native_module_argument_type_count));
			native_module.arguments[native_module.argument_count] = arg;

			if (arg.qualifier == k_native_module_argument_qualifier_in ||
				arg.qualifier == k_native_module_argument_qualifier_constant) {
				native_module.in_argument_count++;
			} else {
				native_module.out_argument_count++;
			}
		} else {
			break;
		}
	}

	// If the first output is the return value, we must have at least one output
	wl_assert(!first_output_is_return || native_module.out_argument_count > 0);

	native_module.compile_time_call = compile_time_call;

	return native_module;
}

// Native module compile-time implementations

static void native_module_noop(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = arguments[0].get_real();
}

static void native_module_negation(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = -arguments[0].get_real();
}

static void native_module_addition(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() + arguments[1].get_real();
}

static void native_module_subtraction(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() - arguments[1].get_real();
}

static void native_module_multiplication(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() * arguments[1].get_real();
}

static void native_module_division(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() / arguments[1].get_real();
}

static void native_module_modulo(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = std::fmod(arguments[0].get_real(), arguments[1].get_real());
}

static void native_module_concatenation(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_string() + arguments[1].get_string();
}

static void native_module_not(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = !arguments[0].get_bool();
}

static void native_module_real_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() == arguments[1].get_real();
}

static void native_module_real_not_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() != arguments[1].get_real();
}

static void native_module_bool_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_bool() == arguments[1].get_bool();
}

static void native_module_bool_not_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_bool() != arguments[1].get_bool();
}

static void native_module_string_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_string() == arguments[1].get_string();
}

static void native_module_string_not_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_string() != arguments[1].get_string();
}

static void native_module_greater(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() > arguments[1].get_real();
}

static void native_module_less(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() < arguments[1].get_real();
}

static void native_module_greater_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() >= arguments[1].get_real();
}

static void native_module_less_equal(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_real() <= arguments[1].get_real();
}

static void native_module_and(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_bool() && arguments[1].get_bool();
}

static void native_module_or(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0].get_bool() || arguments[1].get_bool();
}

static void native_module_abs(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = std::abs(arguments[0].get_real());
}

static void native_module_floor(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = std::floor(arguments[0].get_real());
}

static void native_module_ceil(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = std::ceil(arguments[0].get_real());
}

static void native_module_round(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = std::round(arguments[0].get_real());
}

static void native_module_min(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = std::fmin(arguments[0].get_real(), arguments[1].get_real());
}

static void native_module_max(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = std::fmax(arguments[0].get_real(), arguments[1].get_real());
}

static void native_module_exp(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = std::exp(arguments[0].get_real());
}

static void native_module_log(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = std::log(arguments[0].get_real());
}

static void native_module_sqrt(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = std::sqrt(arguments[0].get_real());
}

static void native_module_pow(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = std::pow(arguments[0].get_real(), arguments[1].get_real());
}

static void native_module_real_static_select(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 4);
	if (arguments[0].get_bool()) {
		arguments[3] = arguments[1].get_real();
	} else {
		arguments[3] = arguments[2].get_real();
	}
}

static void native_module_string_static_select(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 4);
	if (arguments[0].get_bool()) {
		arguments[3] = arguments[1].get_string();
	} else {
		arguments[3] = arguments[2].get_string();
	}
}