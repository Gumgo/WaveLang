#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_ARRAY_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_ARRAY_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_module.h"

const uint32 k_array_library_id = 1;

namespace array_native_modules wl_library(k_array_library_id, "array", 0) {

	wl_native_module(0x11630660, "dereference$real")
	wl_operator(k_native_operator_array_dereference)
	void dereference_real(wl_in_const const c_native_module_real_array &a, wl_in real32 index,
		wl_out_return real32 &result);

	wl_native_module(0x341f28ba, "count$real")
	void count_real(wl_in_const const c_native_module_real_array &a, wl_out_return real32 &result);

	wl_native_module(0x2ef98fa2, "combine$real")
	wl_operator(k_native_operator_addition)
	void combine_real(wl_in_const const c_native_module_real_array &a,
		wl_in_const const c_native_module_real_array &b, wl_out_return c_native_module_real_array &result);

	wl_native_module(0x00731053, "repeat$real")
	wl_operator(k_native_operator_multiplication)
	void repeat_real(wl_in_const const c_native_module_real_array &a, wl_in_const real32 b,
		wl_out_return c_native_module_real_array &result);

	wl_native_module(0xb789d511, "repeat_rev$real")
	wl_operator(k_native_operator_multiplication)
	void repeat_rev_real(wl_in_const real32 a, wl_in_const const c_native_module_real_array &b,
		wl_out_return c_native_module_real_array &result);

	wl_native_module(0xf846b84e, "dereference$bool")
	wl_operator(k_native_operator_array_dereference)
	void dereference_bool(wl_in_const const c_native_module_bool_array &a, wl_in real32 index,
		wl_out_return bool &result);

	wl_native_module(0x18543554, "count$bool")
	void count_bool(wl_in_const const c_native_module_bool_array &a, wl_out_return real32 &result);

	wl_native_module(0x87e6d8fb, "combine$bool")
	wl_operator(k_native_operator_addition)
	void combine_bool(wl_in_const const c_native_module_bool_array &a,
		wl_in_const const c_native_module_bool_array &b, wl_out_return c_native_module_bool_array &result);

	wl_native_module(0x7a6d4345, "repeat$bool")
	wl_operator(k_native_operator_multiplication)
	void repeat_bool(wl_in_const const c_native_module_bool_array &a, wl_in_const real32 b,
		wl_out_return c_native_module_bool_array &result);

	wl_native_module(0x0dd0f9b6, "repeat_rev$bool")
	wl_operator(k_native_operator_multiplication)
	void repeat_rev_bool(wl_in_const real32 a, wl_in_const const c_native_module_bool_array &b,
		wl_out_return c_native_module_bool_array &result);

	wl_native_module(0xf1c6de7f, "dereference$string")
	void dereference_string(wl_in_const const c_native_module_string_array &a, wl_in real32 index,
		wl_out_return c_native_module_string &result);

	wl_native_module(0xa49c681c, "count$string")
	void count_string(wl_in_const const c_native_module_string_array &a, wl_out_return real32 &result);

	wl_native_module(0x2a92132b, "combine$string")
	wl_operator(k_native_operator_addition)
	void combine_string(wl_in_const const c_native_module_string_array &a,
		wl_in_const const c_native_module_string_array &b, wl_out_return c_native_module_string_array &result);

	wl_native_module(0xfd5d5305, "repeat$string")
	wl_operator(k_native_operator_multiplication)
	void repeat_string(wl_in_const const c_native_module_string_array &a, wl_in_const real32 b,
		wl_out_return c_native_module_string_array &result);

	wl_native_module(0xfcd4a1d5, "repeat_rev$string")
	wl_operator(k_native_operator_multiplication)
	void repeat_rev_string(wl_in_const real32 a, wl_in_const const c_native_module_string_array &b,
		wl_out_return c_native_module_string_array &result);

	struct

	wl_optimization_rule(dereference$real(const x, const y) -> x[y])
	wl_optimization_rule(dereference$bool(const x, const y) -> x[y])
	wl_optimization_rule(dereference$string(const x, const y) -> x[y])

	s_optimization_rules;

}

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_ARRAY_H__
