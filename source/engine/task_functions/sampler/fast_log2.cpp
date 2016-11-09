#include "engine/task_functions/sampler/fast_log2.h"

#define FIND_FAST_LOG2_COEFFICIENTS 0
#define TEST_FAST_LOG2 0

#if IS_TRUE(FIND_FAST_LOG2_COEFFICIENTS)

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

#endif // IS_TRUE(FIND_FAST_LOG2_COEFFICIENTS)

#if IS_TRUE(TEST_FAST_LOG2)

#include <iostream>

void test_fast_log2() {
	for (real32 f = 0.0f; f < 10.0f; f += 0.05f) {
		c_real32_4 x(f);
		c_real32_4 frac_part;
		c_int32_4 int_part = fast_log2(x, frac_part);

		ALIGNAS_SSE real32 frac_out[4];
		ALIGNAS_SSE int32 int_out[4];

		frac_part.store(frac_out);
		int_part.store(int_out);

		real32 log2_approx = static_cast<real32>(int_out[0]) + frac_out[0];
		real32 log2_true = std::log2(f);

		real32 diff = std::abs(log2_approx - log2_true);
		std::cout << diff << "\n";
	}
}

int main(int argc, char **argv) {
	test_fast_log2();
	return 0;
}

#endif // IS_TRUE(TEST_FAST_LOG2)
