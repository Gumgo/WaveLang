#include "common/common.h"
#include "engine/task_functions/task_functions_sampler_sinc_window.h"

#define GENERATE_SINC_WINDOW 0
#define FIND_FAST_LOG2_COEFFICIENTS 0

#if PREDEFINED(GENERATE_SINC_WINDOW)
#include <fstream>
#endif // PREDEFINED(GENERATE_SINC_WINDOW)

#if PREDEFINED(GENERATE_SINC_WINDOW)
namespace sinc_window_generator {
	static const real64 k_pi = 3.1415926535897932384626433832795;
	static const real64 k_alpha = 3.0;

	real64 i_0(real64 x) {
		// I_0(x) = sum_{t=0 to inf} ((x/2)^2t / (t!)^2)

		static const int32 k_iterations = 15;
		real64 half_x = x * 0.5;
		real64 sum = 0.0;
		for (int32 t = 0; t < k_iterations; t++) {
			int64 t_factorial_sum = 1;
			for (int32 f = 2; f <= t; f++) {
				t_factorial_sum *= f;
			}

			real64 t_factorial = static_cast<real64>(t_factorial_sum);
			sum += pow(half_x, 2.0 * static_cast<real64>(t)) / (t_factorial * t_factorial);
		}

		return sum;
	}

	real64 kaiser(real64 alpha, uint32 n, uint32 i) {
		wl_assert(i >= 0 && i <= n);

		// w(i) = I_0(pi * alpha * sqrt(1 - ((2i)/n - 1)^2)) / I_0(pi * alpha)
		real64 sqrt_arg = (2.0 * static_cast<real64>(i)) / static_cast<real64>(n) - 1.0;
		sqrt_arg = 1.0 - (sqrt_arg * sqrt_arg);
		return i_0(k_pi * alpha * std::sqrt(sqrt_arg)) / i_0(k_pi * alpha);
	}

	real64 sinc(uint32 n, uint32 i) {
		wl_assert(i >= 0 && i <= n);
		real64 x = (static_cast<real64>(2 * i) / static_cast<real64>(n) - 1.0) *
			static_cast<real64>(k_sinc_window_size / 2);
		return (x == 0.0) ?
			1.0 :
			std::sin(k_pi * x) / (k_pi * x);
	}

	std::string fixup_real32(real32 v) {
		std::string str = std::to_string(v);
		if (str.find_first_of('.') == std::string::npos) {
			str += ".0";
		}
		str.push_back('f');
		return str;
	}

	void generate() {
		std::ofstream out("sinc_window_coefficients.inl");

		// Generate N arrays, one corresponding to each possible resolution offset (range [0,resolution-1])
		// Each array is window_size-1 in length (rounded up to multiples of SSE)

		size_t sse_array_length = align_size(k_sinc_window_size - 1, k_sse_block_elements) / k_sse_block_elements;
		uint32 total_values = static_cast<uint32>((k_sinc_window_size - 1) * k_sinc_window_sample_resolution);

		out << "static const s_sinc_window_coefficients k_sinc_window_coefficients[][" << sse_array_length << "] = {\n";

		for (size_t resolution_index = 0; resolution_index < k_sinc_window_sample_resolution; resolution_index++) {
			size_t sample_index;
			s_sinc_window_coefficients coefficients;
			ZERO_STRUCT(&coefficients);
			size_t sse_index = 0;

			out << "\t{\n";

			for (sample_index = 0; sample_index < k_sinc_window_size - 1; sample_index++) {
				uint32 window_value_index = static_cast<uint32>(
					(k_sinc_window_size - 2 - sample_index) * k_sinc_window_sample_resolution + resolution_index);

				real64 curr_value =
					sinc(total_values, window_value_index) * kaiser(3.0, total_values, window_value_index);
				real64 next_value =
					sinc(total_values, window_value_index + 1) * kaiser(3.0, total_values, window_value_index + 1);
				coefficients.values[sse_index] = static_cast<real32>(curr_value);
				coefficients.slopes[sse_index] = static_cast<real32>(
					(next_value - curr_value) / static_cast<real64>(k_sinc_window_sample_resolution));

				sse_index++;
				// Output if we're a multiple of 4, or if we're the last index
				if (sse_index == k_sse_block_elements || sample_index == k_sinc_window_size - 1) {
					out << "\t\t{ " <<
						fixup_real32(coefficients.values[0]) << ", " <<
						fixup_real32(coefficients.values[1]) << ", " <<
						fixup_real32(coefficients.values[2]) << ", " <<
						fixup_real32(coefficients.values[3]) << ", " <<
						fixup_real32(coefficients.slopes[0]) << ", " <<
						fixup_real32(coefficients.slopes[1]) << ", " <<
						fixup_real32(coefficients.slopes[2]) << ", " <<
						fixup_real32(coefficients.slopes[3]) << " },\n";

					ZERO_STRUCT(&coefficients);
					sse_index = 0;
				}
			}

			out << "\t},\n";
		}

		out << "};\n";
	}
}

int main(int argc, char **argv) {
	sinc_window_generator::generate();
	return 0;
}
#endif // PREDEFINED(GENERATE_SINC_WINDOW)

#if FIND_FAST_LOG2_COEFFICIENTS
#include <iostream>
void find_fast_log2_coefficients() {
	// Let x be a floating point number
	// x = M * 2^E
	// log2(x) = log2(M * 2^E)
	//         = log2(M) + log2(2^E)
	//         = log2(M) + E
	// For non-denormalized floats, M is in the range [1,2). For x in this range, log2(x) is in the range [0,1). We
	// wish to split up the result of log2 into an integer and fraction:
	// int = E
	// frac = log2(M)
	// Thus, we wish to find a fast approximation for log2(M) for M in the range [1,2).

	// We approximate using a 2nd degree polynomial, f(x) = ax^2 + bx + c. We will fix the endpoints (1,0) and (2,1),
	// add a third point (0,y). This results in three equations:
	//  a +  b + c = 0
	// 4a + 2b + c = 1
	//           c = y
	// Solving for a, b, c, we determine that:
	// a = 0.5 + 0.5y
	// b = -0.5 - 1.5y
	// c = y
	// and so our equation becomes f_y(x) = (0.5 + 0.5y)x^2 + (-0.5y - 1.5y)x + y

	// We now wish to find a y such that the maximum value of |f_y(x) - log2(x)| over the range x in [1,2] is minimized.
	// Let d_y(x) = f_y(x) - log2(x)
	//            = (0.5 + 0.5y)x^2 + (-0.5 - 1.5y)x + y - log2(x)
	// Taking the derivative, we obtain
	// d_y'(x) = (1 + y)x - (0.5 + 1.5y) - 1/(x ln(2))
	// Solving for d_y'(x) = 0 will give us the x-coordinate of either the d_y(x)'s local minimum or maximum. We find
	// this by plugging into the quadratic formula to obtain:
	// r = ((0.5 + 1.5y) +- sqrt((0.5 + 1.5y)^2 + (1 + y)*(4/ln(2)))) / (2 + 2y)

	// Now let err_0(y) = d_y(r_0) and err_1(y) = d_y(r_1). These formulas give the minimum and maximum possible values
	// of d_y(x) for a given y, i.e. they tell us the minimum and maximum error for a given y in one direction (+ or -).
	// To find the maximum absolute error, we wish to minimize the function:
	// err(y) = max(|err_0(y)|, |err_1(x)|)

	// By plotting err(y), we can see that the desired value lies between -1.75 and -1.65, so we will use those as our
	// bounds and iteratively search for the minimum. We do this by finding the point of intersection between |err_0(y)|
	// and |err_1(y)|.

	struct s_helper {
		static real64 err_0(real64 y) {
			real64 a = 1.0 + y;
			real64 b = -0.5 - 1.5*y;
			static const real64 k_c = -1.0 / std::log(2.0);

			real64 r = (-b + std::sqrt(b*b - 4.0*a*k_c)) / (2.0*a);
			return (0.5 + 0.5*y)*r*r + (-0.5 - 1.5*y)*r + y - std::log2(r);
		}

		static real64 err_1(real64 y) {
			real64 a = 1.0 + y;
			real64 b = -0.5 - 1.5*y;
			static const real64 k_c = -1.0 / std::log(2.0);

			real64 r = (-b - std::sqrt(b*b - 4.0*a*k_c)) / (2.0*a);
			return (0.5 + 0.5*y)*r*r + (-0.5 - 1.5*y)*r + y - std::log2(r);
		}
	};

	real64 left_bound = -1.75;
	real64 right_bound = -1.65;
	real64 y = 0.0;
	static const size_t k_iterations = 1000;
	for (size_t i = 0; i < k_iterations; i++) {
		y = (left_bound + right_bound) * 0.5;

		real64 left_err_0 = std::abs(s_helper::err_0(left_bound));
		real64 left_err_1 = std::abs(s_helper::err_1(left_bound));
		real64 y_err_0 = std::abs(s_helper::err_0(y));
		real64 y_err_1 = std::abs(s_helper::err_1(y));

		// The two functions intersect exactly once. Therefore, if the ordering is the same on one bound as it is in the
		// center, the intersection point must be on the other side.
		bool left_order = (left_err_0 > left_err_1);
		bool y_order = (y_err_0 > y_err_1);

		if (left_order == y_order) {
			// Shift to the right
			left_bound = y;
		} else {
			// Shift to the left
			right_bound = y;
		}
	}

	std::cout << y << "\n";

	// Determine a, b, c
	real64 a = 0.5 + 0.5*y;
	real64 b = -0.5 - 1.5*y;
	real64 c = y;

	real32 a_32 = static_cast<real32>(a);
	real32 b_32 = static_cast<real32>(b);
	real32 c_32 = static_cast<real32>(c);

	std::cout << a_32 << "\n";
	std::cout << b_32 << "\n";
	std::cout << c_32 << "\n";
}

int main(int argc, char **argv) {
	find_fast_log2_coefficients();
	return 0;
}
#endif // FIND_FAST_LOG2_COEFFICIENTS