#include "instrument/native_module_registration.h"

#include <algorithm>
#include <cmath>

template<typename t_element>
static bool array_subscript(
	const s_native_module_context &context,
	const std::vector<t_element> &array,
	real32 array_index_real,
	t_element &element_out) {
	size_t array_index;
	if (!get_and_validate_native_module_array_index(array_index_real, array.size(), array_index)) {
		context.diagnostic_interface->error(
			"Array index '%f' is out of bounds for array of size '%llu'",
			array_index_real,
			array.size());
		return false;
	}

	element_out = array[array_index];
	return true;
}

template<typename t_element>
static std::vector<t_element> array_combine(
	const std::vector<t_element> &array_0,
	const std::vector<t_element> &array_1) {
	std::vector<t_element> result;
	result.resize(array_0.size() + array_1.size());
	std::copy(array_0.begin(), array_0.end(), result.begin());
	std::copy(array_1.begin(), array_1.end(), result.begin() + array_0.size());
	return result;
}

template<typename t_element>
static std::vector<t_element> array_repeat(
	const s_native_module_context &context,
	const std::vector<t_element> &arr,
	real32 real_repeat_count) {
	if (real_repeat_count < 0.0f || std::isnan(real_repeat_count) || std::isinf(real_repeat_count)) {
		context.diagnostic_interface->error("Illegal array repeat count '%f'", real_repeat_count);
		return std::vector<t_element>();
	}

	// Floor the value automatically in the cast
	int32 repeat_count_signed = std::max(0, static_cast<int32>(real_repeat_count));
	size_t repeat_count = cast_integer_verify<size_t>(repeat_count_signed);
	std::vector<t_element> result;
	result.resize(arr.size() * repeat_count);
	for (size_t rep = 0; rep < repeat_count; rep++) {
		std::copy(arr.begin(), arr.end(), result.begin() + (rep * arr.size()));
	}

	return result;
}

static std::vector<real32> array_range(const s_native_module_context &context, real32 start, real32 stop, real32 step) {
	std::vector<real32> result;

	if (std::isnan(start) || std::isinf(start)) {
		context.diagnostic_interface->error("Illegal array range start value '%f'", start);
		return result;
	}

	if (std::isnan(stop) || std::isinf(stop)) {
		context.diagnostic_interface->error("Illegal array range stop value '%f'", stop);
		return result;
	}

	if (std::isnan(step) || std::isinf(step) || step == 0.0f) {
		context.diagnostic_interface->error("Illegal array range step value '%f'", step);
		return result;
	}

	if (step > 0.0f) {
		for (real32 value = start; value < stop; value += step) {
			result.push_back(value);
		}
	} else {
		for (real32 value = start; value > stop; value += step) {
			result.push_back(value);
		}
	}

	return result;
}

namespace array_native_modules {

	void subscript_real(
		const s_native_module_context &context,
		wl_argument(in ref const? real[], array),
		wl_argument(in const? real, index),
		wl_argument(return out ref const? real, result)) {
		if (!array_subscript(context, array->get_array(), *index, *result)) {
			*result = context.reference_interface->create_constant_reference(0.0f);
		}
	}

	void count_real(
		wl_argument(in ref real[], a),
		wl_argument(return out const real, result)) {
		*result = static_cast<real32>(a->get_array().size());
	}

	void combine_real(
		wl_argument(in ref const? real[], a),
		wl_argument(in ref const? real[], b),
		wl_argument(return out ref const? real[], result)) {
		result->get_array() = array_combine(a->get_array(), b->get_array());
	}

	void repeat_real(
		const s_native_module_context &context,
		wl_argument(in ref const? real[], a),
		wl_argument(in const real, b),
		wl_argument(return out ref const? real[], result)) {
		result->get_array() = array_repeat(context, a->get_array(), *b);
	}

	void repeat_rev_real(
		const s_native_module_context &context,
		wl_argument(in const real, a),
		wl_argument(in ref const? real[], b),
		wl_argument(return out ref const? real[], result)) {
		result->get_array() = array_repeat(context, b->get_array(), *a);
	}

	void subscript_bool(
		const s_native_module_context &context,
		wl_argument(in ref const? bool[], array),
		wl_argument(in const? real, index),
		wl_argument(return out ref const? bool, result)) {
		if (!array_subscript(context, array->get_array(), *index, *result)) {
			*result = context.reference_interface->create_constant_reference(false);
		}
	}

	void count_bool(
		wl_argument(in ref bool[], a),
		wl_argument(return out const real, result)) {
		*result = static_cast<real32>(a->get_array().size());
	}

	void combine_bool(
		wl_argument(in ref const? bool[], a),
		wl_argument(in ref const? bool[], b),
		wl_argument(return out ref const? bool[], result)) {
		result->get_array() = array_combine(a->get_array(), b->get_array());
	}

	void repeat_bool(
		const s_native_module_context &context,
		wl_argument(in ref const? bool[], a),
		wl_argument(in const real, b),
		wl_argument(return out ref const? bool[], result)) {
		result->get_array() = array_repeat(context, a->get_array(), *b);
	}

	void repeat_rev_bool(
		const s_native_module_context &context,
		wl_argument(in const real, a),
		wl_argument(in ref const? bool[], b),
		wl_argument(return out ref const? bool[], result)) {
		result->get_array() = array_repeat(context, b->get_array(), *a);
	}

	void subscript_string(
		const s_native_module_context &context,
		wl_argument(in ref const string[], array),
		wl_argument(in const real, index),
		wl_argument(return out ref const string, result)) {
		if (!array_subscript(context, array->get_array(), *index, *result)) {
			*result = context.reference_interface->create_constant_reference("");
		}
	}

	void count_string(
		wl_argument(in ref const string[], a),
		wl_argument(return out const real, result)) {
		*result = static_cast<real32>(a->get_array().size());
	}

	void combine_string(
		wl_argument(in ref const string[], a),
		wl_argument(in ref const string[], b),
		wl_argument(return out ref const string[], result)) {
		result->get_array() = array_combine(a->get_array(), b->get_array());
	}

	void repeat_string(
		const s_native_module_context &context,
		wl_argument(in ref const string[], a),
		wl_argument(in const real, b),
		wl_argument(return out ref const string[], result)) {
		result->get_array() = array_repeat(context, a->get_array(), *b);
	}

	void repeat_rev_string(
		const s_native_module_context &context,
		wl_argument(in const real, a),
		wl_argument(in ref const string[], b),
		wl_argument(return out ref const string[], result)) {
		result->get_array() = array_repeat(context, b->get_array(), *a);
	}

	void range_stop(
		const s_native_module_context &context,
		wl_argument(in const real, stop),
		wl_argument(return out const real[], result)) {
		result->get_array() = array_range(context, 0.0f, stop, 1.0f);
	}

	void range_start_stop(
		const s_native_module_context &context,
		wl_argument(in const real, start),
		wl_argument(in const real, stop),
		wl_argument(return out const real[], result)) {
		result->get_array() = array_range(context, start, stop, 1.0f);
	}

	void range_start_stop_step(
		const s_native_module_context &context,
		wl_argument(in const real, start),
		wl_argument(in const real, stop),
		wl_argument(in const real, step),
		wl_argument(return out const real[], result)) {
		result->get_array() = array_range(context, start, stop, step);
	}

	void scrape_native_modules() {
		static constexpr uint32 k_array_library_id = 1;
		wl_native_module_library(k_array_library_id, "array", 0);

		wl_native_module(0x11630660, "subscript$real")
			.set_native_operator(e_native_operator::k_subscript)
			.set_compile_time_call<subscript_real>();

		wl_native_module(0x341f28ba, "count$real")
			.set_compile_time_call<count_real>();

		wl_native_module(0x2ef98fa2, "combine$real")
			.set_native_operator(e_native_operator::k_addition)
			.set_compile_time_call<combine_real>();

		wl_native_module(0x00731053, "repeat$real")
			.set_native_operator(e_native_operator::k_multiplication)
			.set_compile_time_call<repeat_real>();

		wl_native_module(0xb789d511, "repeat_rev$real")
			.set_native_operator(e_native_operator::k_multiplication)
			.set_compile_time_call<repeat_rev_real>();

		wl_native_module(0xf846b84e, "subscript$bool")
			.set_native_operator(e_native_operator::k_subscript)
			.set_compile_time_call<subscript_bool>();

		wl_native_module(0x18543554, "count$bool")
			.set_compile_time_call<count_bool>();

		wl_native_module(0x87e6d8fb, "combine$bool")
			.set_native_operator(e_native_operator::k_addition)
			.set_compile_time_call<combine_bool>();

		wl_native_module(0x7a6d4345, "repeat$bool")
			.set_native_operator(e_native_operator::k_multiplication)
			.set_compile_time_call<repeat_bool>();

		wl_native_module(0x0dd0f9b6, "repeat_rev$bool")
			.set_native_operator(e_native_operator::k_multiplication)
			.set_compile_time_call<repeat_rev_bool>();

		wl_native_module(0xf1c6de7f, "subscript$string")
			.set_native_operator(e_native_operator::k_subscript)
			.set_compile_time_call<subscript_string>();

		wl_native_module(0xa49c681c, "count$string")
			.set_compile_time_call<count_string>();

		wl_native_module(0x2a92132b, "combine$string")
			.set_native_operator(e_native_operator::k_addition)
			.set_compile_time_call<combine_string>();

		wl_native_module(0xfd5d5305, "repeat$string")
			.set_native_operator(e_native_operator::k_multiplication)
			.set_compile_time_call<repeat_string>();

		wl_native_module(0xfcd4a1d5, "repeat_rev$string")
			.set_native_operator(e_native_operator::k_multiplication)
			.set_compile_time_call<repeat_rev_string>();

		wl_native_module(0x1ee13a09, "range$stop")
			.set_compile_time_call<range_stop>();

		wl_native_module(0x2e3fc3b0, "range$start_stop")
			.set_compile_time_call<range_start_stop>();

		wl_native_module(0x913c2452, "range$start_stop_step")
			.set_compile_time_call<range_start_stop_step>();

		wl_end_active_library_native_module_registration();
	}
}
