#include "native_module/native_module.h"

const s_native_module_uid s_native_module_uid::k_invalid = s_native_module_uid::build(0xffffffff, 0xffffffff);

static constexpr const char *k_native_operator_names[] = {
	"operator_u+",
	"operator_u-",
	"operator_+",
	"operator_-",
	"operator_*",
	"operator_/",
	"operator_%",
	"operator_!",
	"operator_==",
	"operator_!=",
	"operator_<",
	"operator_>",
	"operator_<=",
	"operator_>=",
	"operator_&&",
	"operator_||",
	"operator_[]"
};
STATIC_ASSERT(is_enum_fully_mapped<e_native_operator>(k_native_operator_names));

const char *get_native_operator_native_module_name(e_native_operator native_operator) {
	wl_assert(valid_enum_index(native_operator));
	return k_native_operator_names[enum_index(native_operator)];
}

void get_native_module_compile_time_properties(
	const s_native_module &native_module,
	bool &always_runs_at_compile_time_out,
	bool &always_runs_at_compile_time_if_dependent_constants_are_constant_out) {
	// Use the input arguments to determine whether this native module is always capable of running at compile-time.
	always_runs_at_compile_time_out = true;
	always_runs_at_compile_time_if_dependent_constants_are_constant_out = true;
	for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
		const s_native_module_argument &argument = native_module.arguments[argument_index];
		if (argument.argument_direction != e_native_module_argument_direction::k_in) {
			continue;
		}

		// If this argument is a reference type, we can always provide it at compile-time. If it's a value type, we can
		// only provide it if the input value happens to be a constant. Therefore, variable and dependent-constant value
		// type arguments imply that the native module can't always run at compile-time.
		if (argument.type.get_data_mutability() != e_native_module_data_mutability::k_constant
			&& argument.data_access == e_native_module_data_access::k_value) {
			always_runs_at_compile_time_out = false;
		}

		// Dependent-constant inputs will never prevent a module from running at compile-time if they are all provided
		// with constants.
		if (argument.type.get_data_mutability() == e_native_module_data_mutability::k_variable
			&& argument.data_access == e_native_module_data_access::k_value) {
			always_runs_at_compile_time_if_dependent_constants_are_constant_out = false;
		}
	}

	always_runs_at_compile_time_out &= (native_module.compile_time_call != nullptr);
	always_runs_at_compile_time_if_dependent_constants_are_constant_out &= (native_module.compile_time_call != nullptr);
}

bool validate_native_module(const s_native_module &native_module) {
	for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
		const s_native_module_argument &argument = native_module.arguments[argument_index];

		if (!argument.type.is_legal()) {
			return false;
		}
	}

	bool always_runs_at_compile_time;
	bool always_runs_at_compile_time_if_dependent_constants_are_constant;
	get_native_module_compile_time_properties(
		native_module,
		always_runs_at_compile_time,
		always_runs_at_compile_time_if_dependent_constants_are_constant);

	// Constant output arguments are only valid at compile-time. Therefore, if this native module can produce a constant
	// output in a situation where it can't run at compile-time, it is invalid.
	for (size_t argument_index = 0; argument_index < native_module.argument_count; argument_index++) {
		const s_native_module_argument &argument = native_module.arguments[argument_index];
		if (argument.argument_direction != e_native_module_argument_direction::k_out) {
			continue;
		}

		// Array outputs must be evaluated at compile-time
		if (argument.type.is_array() && !always_runs_at_compile_time) {
			return false;
		}

		// If this is a constant output, we need to always be able to run at compile-time because constants cannot be
		// produced at runtime.
		if (argument.type.get_data_mutability() == e_native_module_data_mutability::k_constant
			&& !always_runs_at_compile_time) {
			return false;
		}

		// If this is a dependent-constant output, then it becomes constant if all dependent-constant inputs are
		// provided with constants. If that situation occurs and we can't run at compile-time (because some other
		// argument takes a value that's not available at compile-time), then the native module is invalid.
		if (argument.type.get_data_mutability() == e_native_module_data_mutability::k_dependent_constant
			&& !always_runs_at_compile_time_if_dependent_constants_are_constant) {
			return false;
		}
	}

	return true;
}
