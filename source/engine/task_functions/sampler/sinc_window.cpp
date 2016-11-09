#include "engine/task_functions/sampler/sinc_window.h"

#define GENERATE_SINC_WINDOW 0

#if IS_TRUE(GENERATE_SINC_WINDOW)

#include <fstream>

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

#endif // IS_TRUE(GENERATE_SINC_WINDOW)
