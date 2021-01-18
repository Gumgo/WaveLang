#include "instrument/native_module_registration.h"

#include <cmath>

namespace math_native_modules {

	void abs(
		wl_argument(in const? real, a),
		wl_argument(return out const? real, result)) {
		*result = std::abs(*a);
	}

	void floor(
		wl_argument(in const? real, a),
		wl_argument(return out const? real, result)) {
		*result = std::floor(*a);
	}

	void ceil(
		wl_argument(in const? real, a),
		wl_argument(return out const? real, result)) {
		*result = std::ceil(*a);
	}

	void round(
		wl_argument(in const? real, a),
		wl_argument(return out const? real, result)) {
		*result = std::round(*a);
	}

	void min(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? real, result)) {
		*result = std::fmin(*a, *b);
	}

	void max(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? real, result)) {
		*result = std::fmax(*a, *b);
	}

	void exp(
		wl_argument(in const? real, a),
		wl_argument(return out const? real, result)) {
		*result = std::exp(*a);
	}

	void log(
		wl_argument(in const? real, a),
		wl_argument(return out const? real, result)) {
		*result = std::log(*a);
	}

	void sqrt(
		wl_argument(in const? real, a),
		wl_argument(return out const? real, result)) {
		*result = std::sqrt(*a);
	}

	void pow(
		wl_argument(in const? real, a),
		wl_argument(in const? real, b),
		wl_argument(return out const? real, result)) {
		*result = std::pow(*a, *b);
	}

	void sin(
		wl_argument(in const? real, a),
		wl_argument(return out const? real, result)) {
		*result = std::sin(*a);
	}

	void cos(
		wl_argument(in const? real, a),
		wl_argument(return out const? real, result)) {
		*result = std::cos(*a);
	}

	void sincos(
		wl_argument(in const? real, a),
		wl_argument(out const? real, sin_out),
		wl_argument(out const? real, cos_out)) {
		*sin_out = std::sin(*a);
		*cos_out = std::cos(*a);
	}

	void scrape_native_modules() {
		static constexpr uint32 k_math_library_id = 2;
		wl_native_module_library(k_math_library_id, "math", 0);

		wl_native_module(0xb41339b1, "abs")
			.set_compile_time_call<abs>();

		wl_native_module(0x9cc8b581, "floor")
			.set_compile_time_call<floor>();

		wl_native_module(0xa6971c1e, "ceil")
			.set_compile_time_call<ceil>();

		wl_native_module(0x302072ec, "round")
			.set_compile_time_call<round>();

		wl_native_module(0x5693a275, "min")
			.set_compile_time_call<min>();

		wl_native_module(0x6de57a78, "max")
			.set_compile_time_call<max>();

		wl_native_module(0x866c290c, "exp")
			.set_compile_time_call<exp>();

		wl_native_module(0x3ca760b9, "log")
			.set_compile_time_call<log>();

		wl_native_module(0xd869e991, "sqrt")
			.set_compile_time_call<sqrt>();

		wl_native_module(0x05476bde, "pow")
			.set_compile_time_call<pow>();

		wl_native_module(0x68deeac4, "sin")
			.set_compile_time_call<sin>();

		wl_native_module(0xd14343d3, "cos")
			.set_compile_time_call<cos>();

		wl_native_module(0xb319d4a8, "sincos")
			.set_compile_time_call<sincos>();

		wl_end_active_library_native_module_registration();
	}

}
