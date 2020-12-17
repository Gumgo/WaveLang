#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "native_module/native_module.h"

const uint32 k_array_library_id = 1;

namespace array_native_modules wl_library(k_array_library_id, "array", 0) {

	wl_native_module(0x11630660, "subscript$real")
	wl_operator(e_native_operator::k_subscript)
	void subscript_real(
		const s_native_module_context &context,
		wl_in wl_dependent_const const c_native_module_real_reference_array &a,
		wl_in wl_dependent_const real32 index,
		wl_out_return wl_dependent_const c_native_module_real_reference &result);

	wl_native_module(0x341f28ba, "count$real")
	void count_real(
		wl_in const c_native_module_real_reference_array &a,
		wl_out_return wl_const real32 &result);

	wl_native_module(0x2ef98fa2, "combine$real")
	wl_operator(e_native_operator::k_addition)
	void combine_real(
		wl_in wl_dependent_const const c_native_module_real_reference_array &a,
		wl_in wl_dependent_const const c_native_module_real_reference_array &b,
		wl_out_return wl_dependent_const c_native_module_real_reference_array &result);

	wl_native_module(0x00731053, "repeat$real")
	wl_operator(e_native_operator::k_multiplication)
	void repeat_real(
		const s_native_module_context & context,
		wl_in wl_dependent_const const c_native_module_real_reference_array &a,
		wl_in wl_const real32 b,
		wl_out_return wl_dependent_const c_native_module_real_reference_array &result);

	wl_native_module(0xb789d511, "repeat_rev$real")
	wl_operator(e_native_operator::k_multiplication)
	void repeat_rev_real(
		const s_native_module_context & context,
		wl_in wl_const real32 a,
		wl_in wl_dependent_const const c_native_module_real_reference_array &b,
		wl_out_return wl_dependent_const c_native_module_real_reference_array &result);

	wl_native_module(0xf846b84e, "subscript$bool")
	wl_operator(e_native_operator::k_subscript)
	void subscript_bool(
		const s_native_module_context & context,
		wl_in wl_dependent_const const c_native_module_bool_reference_array &a,
		wl_in wl_dependent_const real32 index,
		wl_out_return wl_dependent_const c_native_module_bool_reference &result);

	wl_native_module(0x18543554, "count$bool")
	void count_bool(
		wl_in const c_native_module_bool_reference_array &a,
		wl_out_return wl_const real32 &result);

	wl_native_module(0x87e6d8fb, "combine$bool")
	wl_operator(e_native_operator::k_addition)
	void combine_bool(
		wl_in wl_dependent_const const c_native_module_bool_reference_array &a,
		wl_in wl_dependent_const const c_native_module_bool_reference_array &b,
		wl_out_return wl_dependent_const c_native_module_bool_reference_array &result);

	wl_native_module(0x7a6d4345, "repeat$bool")
	wl_operator(e_native_operator::k_multiplication)
	void repeat_bool(
		const s_native_module_context & context,
		wl_in wl_dependent_const const c_native_module_bool_reference_array &a,
		wl_in wl_const real32 b,
		wl_out_return wl_dependent_const c_native_module_bool_reference_array &result);

	wl_native_module(0x0dd0f9b6, "repeat_rev$bool")
	wl_operator(e_native_operator::k_multiplication)
	void repeat_rev_bool(
		const s_native_module_context & context,
		wl_in wl_const real32 a,
		wl_in wl_dependent_const const c_native_module_bool_reference_array &b,
		wl_out_return wl_dependent_const c_native_module_bool_reference_array &result);

	wl_native_module(0xf1c6de7f, "subscript$string")
	void subscript_string(
		const s_native_module_context & context,
		wl_in wl_const const c_native_module_string_reference_array &a,
		wl_in wl_const real32 index,
		wl_out_return wl_const c_native_module_string_reference &result);

	wl_native_module(0xa49c681c, "count$string")
	void count_string(
		wl_in wl_const const c_native_module_string_reference_array &a,
		wl_out_return wl_const real32 &result);

	wl_native_module(0x2a92132b, "combine$string")
	wl_operator(e_native_operator::k_addition)
	void combine_string(
		wl_in wl_const const c_native_module_string_reference_array &a,
		wl_in wl_const const c_native_module_string_reference_array &b,
		wl_out_return wl_const c_native_module_string_reference_array &result);

	wl_native_module(0xfd5d5305, "repeat$string")
	wl_operator(e_native_operator::k_multiplication)
	void repeat_string(
		const s_native_module_context & context,
		wl_in wl_const const c_native_module_string_reference_array &a,
		wl_in wl_const real32 b,
		wl_out_return wl_const c_native_module_string_reference_array &result);

	wl_native_module(0xfcd4a1d5, "repeat_rev$string")
	wl_operator(e_native_operator::k_multiplication)
	void repeat_rev_string(
		const s_native_module_context &context,
		wl_in wl_const real32 a,
		wl_in wl_const const c_native_module_string_reference_array &b,
		wl_out_return wl_const c_native_module_string_reference_array &result);

}

