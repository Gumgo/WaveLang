#include "engine/native_module_task_mapping.h"

static bool do_native_module_inputs_match(const char *native_module_inputs, const char *match_string);

bool get_task_mapping_for_native_module_and_inputs(
	uint32 native_module_index,
	const char *native_module_inputs,
	e_task_function &out_task_function,
	c_task_mapping_array &out_task_mapping_array) {
	// Temporaries to store our result
	c_task_mapping_array task_mapping_array;
	e_task_function task_function = k_task_function_count;

	// Task mapping selection logic:
	switch (native_module_index) {
	case k_native_module_noop:
		wl_vhalt("No-ops should have been eliminated by this point");
		return false;

	case k_native_module_negation:
		if (do_native_module_inputs_match(native_module_inputs, "V")) {
			static const uint32 k_mapping[] = { 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_negation_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "v")) {
			static const uint32 k_mapping[] = { 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_negation_buffer;
		}
		break;

	case k_native_module_addition:
		if (do_native_module_inputs_match(native_module_inputs, "Vv")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_addition_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vV")) {
			static const uint32 k_mapping[] = { 1, 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_addition_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_addition_buffer_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "Vc")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_addition_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "vc")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_addition_buffer_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cV")) {
			static const uint32 k_mapping[] = { 1, 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_addition_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cv")) {
			static const uint32 k_mapping[] = { 2, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_addition_buffer_constant;
		}
		break;

	case k_native_module_subtraction:
		if (do_native_module_inputs_match(native_module_inputs, "Vv")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_subtraction_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vV")) {
			static const uint32 k_mapping[] = { 0, 1, 1 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_subtraction_buffer_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "vv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_subtraction_buffer_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "Vc")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_subtraction_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "vc")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_subtraction_buffer_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cV")) {
			static const uint32 k_mapping[] = { 0, 1, 1 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_subtraction_constant_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "cv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_subtraction_constant_buffer;
		}
		break;

	case k_native_module_multiplication:
		if (do_native_module_inputs_match(native_module_inputs, "Vv")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_multiplication_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vV")) {
			static const uint32 k_mapping[] = { 1, 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_multiplication_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_multiplication_buffer_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "Vc")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_multiplication_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "vc")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_multiplication_buffer_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cV")) {
			static const uint32 k_mapping[] = { 1, 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_multiplication_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cv")) {
			static const uint32 k_mapping[] = { 2, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_multiplication_buffer_constant;
		}
		break;

	case k_native_module_division:
		if (do_native_module_inputs_match(native_module_inputs, "Vv")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_division_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vV")) {
			static const uint32 k_mapping[] = { 0, 1, 1 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_division_buffer_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "vv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_division_buffer_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "Vc")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_division_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "vc")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_division_buffer_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cV")) {
			static const uint32 k_mapping[] = { 0, 1, 1 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_division_constant_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "cv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_division_constant_buffer;
		}
		break;

	case k_native_module_modulo:
		if (do_native_module_inputs_match(native_module_inputs, "Vv")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_modulo_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vV")) {
			static const uint32 k_mapping[] = { 0, 1, 1 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_modulo_buffer_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "vv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_modulo_buffer_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "Vc")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_modulo_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "vc")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_modulo_buffer_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cV")) {
			static const uint32 k_mapping[] = { 0, 1, 1 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_modulo_constant_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "cv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_modulo_constant_buffer;
		}
		break;

	case k_native_module_abs:
		if (do_native_module_inputs_match(native_module_inputs, "V")) {
			static const uint32 k_mapping[] = { 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_abs_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "v")) {
			static const uint32 k_mapping[] = { 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_abs_buffer;
		}
		break;

	case k_native_module_floor:
		if (do_native_module_inputs_match(native_module_inputs, "V")) {
			static const uint32 k_mapping[] = { 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_floor_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "v")) {
			static const uint32 k_mapping[] = { 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_floor_buffer;
		}
		break;

	case k_native_module_ceil:
		if (do_native_module_inputs_match(native_module_inputs, "V")) {
			static const uint32 k_mapping[] = { 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_ceil_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "v")) {
			static const uint32 k_mapping[] = { 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_ceil_buffer;
		}
		break;

	case k_native_module_round:
		if (do_native_module_inputs_match(native_module_inputs, "V")) {
			static const uint32 k_mapping[] = { 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_round_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "v")) {
			static const uint32 k_mapping[] = { 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_round_buffer;
		}
		break;

	case k_native_module_min:
		if (do_native_module_inputs_match(native_module_inputs, "Vv")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_min_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vV")) {
			static const uint32 k_mapping[] = { 1, 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_min_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_min_buffer_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "Vc")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_min_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "vc")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_min_buffer_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cV")) {
			static const uint32 k_mapping[] = { 1, 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_min_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cv")) {
			static const uint32 k_mapping[] = { 2, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_min_buffer_constant;
		}
		break;

	case k_native_module_max:
		if (do_native_module_inputs_match(native_module_inputs, "Vv")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_max_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vV")) {
			static const uint32 k_mapping[] = { 1, 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_max_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_max_buffer_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "Vc")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_max_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "vc")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_max_buffer_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cV")) {
			static const uint32 k_mapping[] = { 1, 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_max_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cv")) {
			static const uint32 k_mapping[] = { 2, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_max_buffer_constant;
		}
		break;

	case k_native_module_exp:
		if (do_native_module_inputs_match(native_module_inputs, "V")) {
			static const uint32 k_mapping[] = { 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_exp_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "v")) {
			static const uint32 k_mapping[] = { 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_exp_buffer;
		}
		break;

	case k_native_module_log:
		if (do_native_module_inputs_match(native_module_inputs, "V")) {
			static const uint32 k_mapping[] = { 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_log_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "v")) {
			static const uint32 k_mapping[] = { 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_log_buffer;
		}
		break;

	case k_native_module_sqrt:
		if (do_native_module_inputs_match(native_module_inputs, "V")) {
			static const uint32 k_mapping[] = { 0, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_sqrt_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "v")) {
			static const uint32 k_mapping[] = { 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_sqrt_buffer;
		}
		break;

	case k_native_module_pow:
		if (do_native_module_inputs_match(native_module_inputs, "Vv")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_pow_bufferio_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "vV")) {
			static const uint32 k_mapping[] = { 0, 1, 1 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_pow_buffer_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "vv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_pow_buffer_buffer;
		} else if (do_native_module_inputs_match(native_module_inputs, "Vc")) {
			static const uint32 k_mapping[] = { 0, 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_pow_bufferio_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "vc")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_pow_buffer_constant;
		} else if (do_native_module_inputs_match(native_module_inputs, "cV")) {
			static const uint32 k_mapping[] = { 0, 1, 1 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_pow_constant_bufferio;
		} else if (do_native_module_inputs_match(native_module_inputs, "cv")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_pow_constant_buffer;
		}
		break;

	case k_native_module_test:
		if (do_native_module_inputs_match(native_module_inputs, "v")) {
			static const uint32 k_mapping[] = { 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_test;
		} else if (do_native_module_inputs_match(native_module_inputs, "c")) {
			static const uint32 k_mapping[] = { 1, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_test_c;
		}
		break;

	case k_native_module_delay_test:
		if (do_native_module_inputs_match(native_module_inputs, "vc")) {
			static const uint32 k_mapping[] = { 1, 2, 0 };
			task_mapping_array = c_task_mapping_array::construct(k_mapping);
			task_function = k_task_function_test_delay;
		}
		break;

	default:
		wl_unreachable();
	}

	if (task_mapping_array.get_count() == 0) {
		wl_assert(task_function = k_task_function_count);
		return false;
	} else {
		out_task_function = task_function;
		out_task_mapping_array = task_mapping_array;
		return true;
	}
}

static bool do_native_module_inputs_match(const char *native_module_inputs, const char *match_string) {
#if PREDEFINED(ASSERTS_ENABLED)
	for (const char *ch = native_module_inputs; *ch != '\0'; ch++) {
		wl_assert(*ch == 'c' || *ch == 'v' || *ch == 'V');
	}

	for (const char *ch = match_string; *ch != '\0'; ch++) {
		wl_assert(*ch == 'c' || *ch == 'v' || *ch == 'V');
	}

	wl_assert(strlen(native_module_inputs) == strlen(match_string));
#endif // PREDEFINED(ASSERTS_ENABLED)

	for (const char *in_ptr = native_module_inputs, *match_ptr = match_string;
		 *in_ptr != '\0';
		 in_ptr++, match_ptr++) {
		char in_ch = *in_ptr;
		char match_ch = *match_ptr;

		if (match_ch == 'c') {
			// We expect a constant as input
			if (in_ch != 'c') {
				return false;
			}
		} else if (match_ch == 'v') {
			// We expect a value, either branching or branchless
			if (in_ch != 'v' && in_ch != 'V') {
				return false;
			}
		} else {
			wl_assert(match_ch == 'V');
			// We expect a branchless value
			if (in_ch != 'V') {
				return false;
			}
		}
	}

	return true;
}