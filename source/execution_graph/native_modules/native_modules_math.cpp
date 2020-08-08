#include "execution_graph/native_module_registry.h"
#include "execution_graph/native_modules/native_modules_math.h"

#include <cmath>

namespace math_native_modules {

	void abs(real32 a, real32 &result) {
		result = std::abs(a);
	}

	void floor(real32 a, real32 &result) {
		result = std::floor(a);
	}

	void ceil(real32 a, real32 &result) {
		result = std::ceil(a);
	}

	void round(real32 a, real32 &result) {
		result = std::round(a);
	}

	void min(real32 a, real32 b, real32 &result) {
		result = std::fmin(a, b);
	}

	void max(real32 a, real32 b, real32 &result) {
		result = std::fmax(a, b);
	}

	void exp(real32 a, real32 &result) {
		result = std::exp(a);
	}

	void log(real32 a, real32 &result) {
		result = std::log(a);
	}

	void sqrt(real32 a, real32 &result) {
		result = std::sqrt(a);
	}

	void pow(real32 a, real32 b, real32 &result) {
		result = std::pow(a, b);
	}

	void sin(real32 a, real32 &result) {
		result = std::sin(a);
	}

	void cos(real32 a, real32 &result) {
		result = std::cos(a);
	}

	void sincos(real32 a, real32 &out_sin, real32 &out_cos) {
		out_sin = std::sin(a);
		out_cos = std::cos(a);
	}

}
