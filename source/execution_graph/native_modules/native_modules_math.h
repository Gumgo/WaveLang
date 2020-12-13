#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "native_module/native_module.h"

const uint32 k_math_library_id = 2;

namespace math_native_modules wl_library(k_math_library_id, "math", 0) {

	wl_native_module(0xb41339b1, "abs")
	void abs(wl_in real32 a, wl_out_return real32 &result);

	wl_native_module(0x9cc8b581, "floor")
	void floor(wl_in wl_dependent_const real32 a, wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0xa6971c1e, "ceil")
	void ceil(wl_in wl_dependent_const real32 a, wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0x302072ec, "round")
	void round(wl_in wl_dependent_const real32 a, wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0x5693a275, "min")
	void min(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0x6de57a78, "max")
	void max(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0x866c290c, "exp")
	void exp(wl_in wl_dependent_const real32 a, wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0x3ca760b9, "log")
	void log(wl_in wl_dependent_const real32 a, wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0xd869e991, "sqrt")
	void sqrt(wl_in wl_dependent_const real32 a, wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0x05476bde, "pow")
	void pow(
		wl_in wl_dependent_const real32 a,
		wl_in wl_dependent_const real32 b,
		wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0x68deeac4, "sin")
	void sin(wl_in wl_dependent_const real32 a, wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0xd14343d3, "cos")
	void cos(wl_in wl_dependent_const real32 a, wl_out_return wl_dependent_const real32 &result);

	wl_native_module(0xb319d4a8, "sincos")
	void sincos(
		wl_in wl_dependent_const real32 a,
		wl_out wl_dependent_const real32 &sin_out,
		wl_out wl_dependent_const real32 &cos_out);
}

