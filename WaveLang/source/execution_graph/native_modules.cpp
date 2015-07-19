#include "execution_graph/native_modules.h"

#define NATIVE_PREFIX "__native_"

const char *c_native_module_registry::k_operator_noop_name = NATIVE_PREFIX "noop";
const char *c_native_module_registry::k_operator_negation_name = NATIVE_PREFIX "negation";
const char *c_native_module_registry::k_operator_addition_operator_name = NATIVE_PREFIX "addition";
const char *c_native_module_registry::k_operator_subtraction_operator_name = NATIVE_PREFIX "subtraction";
const char *c_native_module_registry::k_operator_multiplication_operator_name = NATIVE_PREFIX "multiplication";
const char *c_native_module_registry::k_operator_division_name = NATIVE_PREFIX "division";
const char *c_native_module_registry::k_operator_modulo_name = NATIVE_PREFIX "modulo";

// List of compile-time native modules
static void native_module_noop(c_native_module_compile_time_argument_list &arguments);
static void native_module_negation(c_native_module_compile_time_argument_list &arguments);
static void native_module_addition(c_native_module_compile_time_argument_list &arguments);
static void native_module_subtraction(c_native_module_compile_time_argument_list &arguments);
static void native_module_multiplication(c_native_module_compile_time_argument_list &arguments);
static void native_module_division(c_native_module_compile_time_argument_list &arguments);
static void native_module_modulo(c_native_module_compile_time_argument_list &arguments);

struct s_native_module_argument_list {
	e_native_module_argument_type argument_types[k_max_native_module_arguments];
};

static s_native_module_argument_list make_args(
	e_native_module_argument_type arg_0 = k_native_module_argument_type_count,
	e_native_module_argument_type arg_1 = k_native_module_argument_type_count,
	e_native_module_argument_type arg_2 = k_native_module_argument_type_count,
	e_native_module_argument_type arg_3 = k_native_module_argument_type_count,
	e_native_module_argument_type arg_4 = k_native_module_argument_type_count,
	e_native_module_argument_type arg_5 = k_native_module_argument_type_count,
	e_native_module_argument_type arg_6 = k_native_module_argument_type_count,
	e_native_module_argument_type arg_7 = k_native_module_argument_type_count,
	e_native_module_argument_type arg_8 = k_native_module_argument_type_count,
	e_native_module_argument_type arg_9 = k_native_module_argument_type_count);

static s_native_module make_native_module(
	const char *name,
	bool first_output_is_return,
	const s_native_module_argument_list &arguments,
	f_native_module_compile_time_call compile_time_call);

#define NMAT(x) k_native_module_argument_type_ ## x

// The global list of native modules
static const s_native_module k_native_modules[] = {
	make_native_module(c_native_module_registry::k_operator_noop_name,
	true, make_args(NMAT(in), NMAT(out)),
	native_module_noop),

	make_native_module(c_native_module_registry::k_operator_negation_name,
	true, make_args(NMAT(in), NMAT(out)),
	native_module_negation),

	make_native_module(c_native_module_registry::k_operator_addition_operator_name,
	true, make_args(NMAT(in), NMAT(in), NMAT(out)),
	native_module_addition),

	make_native_module(c_native_module_registry::k_operator_subtraction_operator_name,
	true, make_args(NMAT(in), NMAT(in), NMAT(out)),
	native_module_subtraction),

	make_native_module(c_native_module_registry::k_operator_multiplication_operator_name,
	true, make_args(NMAT(in), NMAT(in), NMAT(out)),
	native_module_multiplication),

	make_native_module(c_native_module_registry::k_operator_division_name,
	true, make_args(NMAT(in), NMAT(in), NMAT(out)),
	native_module_division),

	make_native_module(c_native_module_registry::k_operator_modulo_name,
	true, make_args(NMAT(in), NMAT(in), NMAT(out)),
	native_module_modulo),

	// Non-constant test module
	make_native_module(NATIVE_PREFIX "test",
	true, make_args(NMAT(in), NMAT(out)),
	nullptr),

	make_native_module(NATIVE_PREFIX "delay_test",
	true, make_args(NMAT(in), NMAT(constant), NMAT(out)),
	nullptr)
};

static_assert(NUMBEROF(k_native_modules) == k_native_module_count, "Native module definition list mismatch");

uint32 c_native_module_registry::get_native_module_count() {
	return NUMBEROF(k_native_modules);
}

const s_native_module &c_native_module_registry::get_native_module(uint32 index) {
	wl_assert(VALID_INDEX(index, get_native_module_count()));
	return k_native_modules[index];
}

uint32 c_native_module_registry::get_native_module_index(const char *name) {
	for (uint32 index = 0; index < get_native_module_count(); index++) {
		if (strcmp(name, get_native_module(index).name) == 0) {
			return index;
		}
	}

	wl_vhalt("Native module not found");
	return static_cast<uint32>(-1);
}

static s_native_module_argument_list make_args(
	e_native_module_argument_type arg_0,
	e_native_module_argument_type arg_1,
	e_native_module_argument_type arg_2,
	e_native_module_argument_type arg_3,
	e_native_module_argument_type arg_4,
	e_native_module_argument_type arg_5,
	e_native_module_argument_type arg_6,
	e_native_module_argument_type arg_7,
	e_native_module_argument_type arg_8,
	e_native_module_argument_type arg_9) {
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
		// Loop until we find an k_native_module_argument_type_count
		if (arguments.argument_types[native_module.argument_count] != k_native_module_argument_type_count) {
			wl_assert(VALID_INDEX(arguments.argument_types[native_module.argument_count],
				k_native_module_argument_type_count));
			native_module.argument_types[native_module.argument_count] =
				arguments.argument_types[native_module.argument_count];

			if (arguments.argument_types[native_module.argument_count] == k_native_module_argument_type_in ||
				arguments.argument_types[native_module.argument_count] == k_native_module_argument_type_constant) {
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
	arguments[1] = static_cast<real32>(arguments[0]);
}

static void native_module_negation(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 2);
	arguments[1] = -arguments[0];
}

static void native_module_addition(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0] + arguments[1];
}

static void native_module_subtraction(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0] - arguments[1];
}

static void native_module_multiplication(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0] * arguments[1];
}

static void native_module_division(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = arguments[0] / arguments[1];
}

static void native_module_modulo(c_native_module_compile_time_argument_list &arguments) {
	wl_assert(arguments.size() == 3);
	arguments[2] = std::fmod(arguments[0], arguments[1]);
}
