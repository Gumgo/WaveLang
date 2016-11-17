#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/task_function_registry.h"
#include "engine/task_functions/task_functions_filter.h"

template<typename t_value>
struct s_biquad_loop_value_reader {
};

template<>
struct s_biquad_loop_value_reader<real32> {
	static real32 read(real32 value, size_t index) {
		return value;
	}
};

template<>
struct s_biquad_loop_value_reader<const real32 *> {
	static real32 read(const real32 *value, size_t index) {
		return value[index];
	}
};

struct s_buffer_operation_biquad {
	static size_t query_memory();
	void voice_initialize();

	template<typename t_a1, typename t_a2, typename t_b0, typename t_b1, typename t_b2, typename t_x0>
	void run_filter_loop(size_t buffer_size, t_a1 a1, t_a2 a2, t_b0 b0, t_b1 b1, t_b2 b2, t_x0 x0, real32 *y);

	real32 run_filter_once(real32 a1, real32 a2, real32 b0, real32 b1, real32 b2, real32 x0);

	void in_in_in_in_in_in_out(
		size_t buffer_size,
		c_real_const_buffer_or_constant a1,
		c_real_const_buffer_or_constant a2,
		c_real_const_buffer_or_constant b0,
		c_real_const_buffer_or_constant b1,
		c_real_const_buffer_or_constant b2,
		c_real_const_buffer_or_constant signal,
		c_real_buffer_out result);

	void inout_in_in_in_in_in(
		size_t buffer_size,
		c_real_buffer *a1_result,
		c_real_const_buffer_or_constant a2,
		c_real_const_buffer_or_constant b0,
		c_real_const_buffer_or_constant b1,
		c_real_const_buffer_or_constant b2,
		c_real_const_buffer_or_constant signal);

	void in_inout_in_in_in_in(
		size_t buffer_size,
		c_real_const_buffer_or_constant a1,
		c_real_buffer *a2_result,
		c_real_const_buffer_or_constant b0,
		c_real_const_buffer_or_constant b1,
		c_real_const_buffer_or_constant b2,
		c_real_const_buffer_or_constant signal);

	void in_in_inout_in_in_in(
		size_t buffer_size,
		c_real_const_buffer_or_constant a1,
		c_real_const_buffer_or_constant a2,
		c_real_buffer *b0_result,
		c_real_const_buffer_or_constant b1,
		c_real_const_buffer_or_constant b2,
		c_real_const_buffer_or_constant signal);

	void in_in_in_inout_in_in(
		size_t buffer_size,
		c_real_const_buffer_or_constant a1,
		c_real_const_buffer_or_constant a2,
		c_real_const_buffer_or_constant b0,
		c_real_buffer *b1_result,
		c_real_const_buffer_or_constant b2,
		c_real_const_buffer_or_constant signal);

	void in_in_in_in_inout_in(
		size_t buffer_size,
		c_real_const_buffer_or_constant a1,
		c_real_const_buffer_or_constant a2,
		c_real_const_buffer_or_constant b0,
		c_real_const_buffer_or_constant b1,
		c_real_buffer *b2_result,
		c_real_const_buffer_or_constant signal);

	void in_in_in_in_in_inout(
		size_t buffer_size,
		c_real_const_buffer_or_constant a1,
		c_real_const_buffer_or_constant a2,
		c_real_const_buffer_or_constant b0,
		c_real_const_buffer_or_constant b1,
		c_real_const_buffer_or_constant b2,
		c_real_buffer *signal_result);

	// See http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt for algorithm details
	real32 x1, x2;
	real32 y1, y2;
};

size_t s_buffer_operation_biquad::query_memory() {
	return sizeof(s_buffer_operation_biquad);
}

void s_buffer_operation_biquad::voice_initialize() {
	ZERO_STRUCT(this);
}

template<typename t_a1, typename t_a2, typename t_b0, typename t_b1, typename t_b2, typename t_x0>
void s_buffer_operation_biquad::run_filter_loop(
	size_t buffer_size, t_a1 a1, t_a2 a2, t_b0 b0, t_b1 b1, t_b2 b2, t_x0 x0, real32 *y) {
	for (size_t index = 0; index < buffer_size; index++) {
		y[index] = run_filter_once(
			s_biquad_loop_value_reader<t_a1>::read(a1, index),
			s_biquad_loop_value_reader<t_a2>::read(a2, index),
			s_biquad_loop_value_reader<t_b0>::read(b0, index),
			s_biquad_loop_value_reader<t_b1>::read(b1, index),
			s_biquad_loop_value_reader<t_b2>::read(b2, index),
			s_biquad_loop_value_reader<t_x0>::read(x0, index));
	}
}

real32 s_buffer_operation_biquad::run_filter_once(real32 a1, real32 a2, real32 b0, real32 b1, real32 b2, real32 x0) {
	// Run a single iteration - currently not using SSE because each sample depends on the previous ones
	real32 y0 = b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;

	// Shift the accumulator
	x2 = x1;
	x1 = x0;
	y2 = y1;
	y1 = y0;

	return y0;
}

void s_buffer_operation_biquad::in_in_in_in_in_in_out(
	size_t buffer_size,
	c_real_const_buffer_or_constant a1,
	c_real_const_buffer_or_constant a2,
	c_real_const_buffer_or_constant b0,
	c_real_const_buffer_or_constant b1,
	c_real_const_buffer_or_constant b2,
	c_real_const_buffer_or_constant signal,
	c_real_buffer_out result) {
	real32 *result_val = result->get_data<real32>();

	if (a1.is_constant()) {
		real32 a1_val = a1.get_constant();
		if (a2.is_constant()) {
			real32 a2_val = a2.get_constant();
			if (b0.is_constant()) {
				real32 b0_val = b0.get_constant();
				if (b1.is_constant()) {
					real32 b1_val = b1.get_constant();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				} else {
					const real32 *b1_val = b1.get_buffer()->get_data<real32>();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				}
			} else {
				const real32 *b0_val = b0.get_buffer()->get_data<real32>();
				if (b1.is_constant()) {
					real32 b1_val = b1.get_constant();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				} else {
					const real32 *b1_val = b1.get_buffer()->get_data<real32>();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				}
			}
		} else {
			const real32 *a2_val = a2.get_buffer()->get_data<real32>();
			if (b0.is_constant()) {
				real32 b0_val = b0.get_constant();
				if (b1.is_constant()) {
					real32 b1_val = b1.get_constant();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				} else {
					const real32 *b1_val = b1.get_buffer()->get_data<real32>();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				}
			} else {
				const real32 *b0_val = b0.get_buffer()->get_data<real32>();
				if (b1.is_constant()) {
					real32 b1_val = b1.get_constant();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				} else {
					const real32 *b1_val = b1.get_buffer()->get_data<real32>();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				}
			}
		}
	} else {
		const real32 *a1_val = a1.get_buffer()->get_data<real32>();
		if (a2.is_constant()) {
			real32 a2_val = a2.get_constant();
			if (b0.is_constant()) {
				real32 b0_val = b0.get_constant();
				if (b1.is_constant()) {
					real32 b1_val = b1.get_constant();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				} else {
					const real32 *b1_val = b1.get_buffer()->get_data<real32>();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				}
			} else {
				const real32 *b0_val = b0.get_buffer()->get_data<real32>();
				if (b1.is_constant()) {
					real32 b1_val = b1.get_constant();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				} else {
					const real32 *b1_val = b1.get_buffer()->get_data<real32>();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				}
			}
		} else {
			const real32 *a2_val = a2.get_buffer()->get_data<real32>();
			if (b0.is_constant()) {
				real32 b0_val = b0.get_constant();
				if (b1.is_constant()) {
					real32 b1_val = b1.get_constant();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				} else {
					const real32 *b1_val = b1.get_buffer()->get_data<real32>();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				}
			} else {
				const real32 *b0_val = b0.get_buffer()->get_data<real32>();
				if (b1.is_constant()) {
					real32 b1_val = b1.get_constant();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				} else {
					const real32 *b1_val = b1.get_buffer()->get_data<real32>();
					if (b2.is_constant()) {
						real32 b2_val = b2.get_constant();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					} else {
						const real32 *b2_val = b2.get_buffer()->get_data<real32>();
						if (signal.is_constant()) {
							real32 signal_val = signal.get_constant();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						} else {
							const real32 *signal_val = signal.get_buffer()->get_data<real32>();
							run_filter_loop(
								buffer_size, a1_val, a2_val, b0_val, b1_val, b2_val, signal_val, result_val);
						}
					}
				}
			}
		}
	}

	// $TODO we could possibly set constant to true if all inputs are constant and the accumulator is constant and
	// matches the input signal - does this condition imply convergence?
	result->set_constant(false);
}

// For the inout versions, just split the inout buffer into in and out

void s_buffer_operation_biquad::inout_in_in_in_in_in(
	size_t buffer_size,
	c_real_buffer *a1_result,
	c_real_const_buffer_or_constant a2,
	c_real_const_buffer_or_constant b0,
	c_real_const_buffer_or_constant b1,
	c_real_const_buffer_or_constant b2,
	c_real_const_buffer_or_constant signal) {
	in_in_in_in_in_in_out(
		buffer_size, c_real_const_buffer_or_constant(a1_result), a2, b0, b1, b2, signal, a1_result);
}

void s_buffer_operation_biquad::in_inout_in_in_in_in(
	size_t buffer_size,
	c_real_const_buffer_or_constant a1,
	c_real_buffer *a2_result,
	c_real_const_buffer_or_constant b0,
	c_real_const_buffer_or_constant b1,
	c_real_const_buffer_or_constant b2,
	c_real_const_buffer_or_constant signal) {
	in_in_in_in_in_in_out(
		buffer_size, a1, c_real_const_buffer_or_constant(a2_result), b0, b1, b2, signal, a2_result);
}

void s_buffer_operation_biquad::in_in_inout_in_in_in(
	size_t buffer_size,
	c_real_const_buffer_or_constant a1,
	c_real_const_buffer_or_constant a2,
	c_real_buffer *b0_result,
	c_real_const_buffer_or_constant b1,
	c_real_const_buffer_or_constant b2,
	c_real_const_buffer_or_constant signal) {
	in_in_in_in_in_in_out(
		buffer_size, a1, a2, c_real_const_buffer_or_constant(b0_result), b1, b2, signal, b0_result);
}

void s_buffer_operation_biquad::in_in_in_inout_in_in(
	size_t buffer_size,
	c_real_const_buffer_or_constant a1,
	c_real_const_buffer_or_constant a2,
	c_real_const_buffer_or_constant b0,
	c_real_buffer *b1_result,
	c_real_const_buffer_or_constant b2,
	c_real_const_buffer_or_constant signal) {
	in_in_in_in_in_in_out(
		buffer_size, a1, a2, b0, c_real_const_buffer_or_constant(b1_result), b2, signal, b1_result);
}

void s_buffer_operation_biquad::in_in_in_in_inout_in(
	size_t buffer_size,
	c_real_const_buffer_or_constant a1,
	c_real_const_buffer_or_constant a2,
	c_real_const_buffer_or_constant b0,
	c_real_const_buffer_or_constant b1,
	c_real_buffer *b2_result,
	c_real_const_buffer_or_constant signal) {
	in_in_in_in_in_in_out(
		buffer_size, a1, a2, b0, b1, c_real_const_buffer_or_constant(b2_result), signal, b2_result);
}

void s_buffer_operation_biquad::in_in_in_in_in_inout(
	size_t buffer_size,
	c_real_const_buffer_or_constant a1,
	c_real_const_buffer_or_constant a2,
	c_real_const_buffer_or_constant b0,
	c_real_const_buffer_or_constant b1,
	c_real_const_buffer_or_constant b2,
	c_real_buffer *signal_result) {
	in_in_in_in_in_in_out(
		buffer_size, a1, a2, b0, b1, b2, c_real_const_buffer_or_constant(signal_result), signal_result);
}

namespace filter_task_functions {

	size_t biquad_memory_query(const s_task_function_context &context) {
		return s_buffer_operation_biquad::query_memory();
	}

	void biquad_voice_initializer(const s_task_function_context &context) {
		static_cast<s_buffer_operation_biquad *>(context.task_memory)->voice_initialize();
	}

	void biquad_in_in_in_in_in_in_out(const s_task_function_context &context,
		c_real_const_buffer_or_constant a1,
		c_real_const_buffer_or_constant a2,
		c_real_const_buffer_or_constant b0,
		c_real_const_buffer_or_constant b1,
		c_real_const_buffer_or_constant b2,
		c_real_const_buffer_or_constant signal,
		c_real_buffer *result) {
		static_cast<s_buffer_operation_biquad *>(context.task_memory)->in_in_in_in_in_in_out(
			context.buffer_size, a1, a2, b0, b1, b2, signal, result);
	}

	void biquad_inout_in_in_in_in_in(const s_task_function_context &context,
		c_real_buffer *a1_result,
		c_real_const_buffer_or_constant a2,
		c_real_const_buffer_or_constant b0,
		c_real_const_buffer_or_constant b1,
		c_real_const_buffer_or_constant b2,
		c_real_const_buffer_or_constant signal) {
		static_cast<s_buffer_operation_biquad *>(context.task_memory)->inout_in_in_in_in_in(
			context.buffer_size, a1_result, a2, b0, b1, b2, signal);
	}

	void biquad_in_inout_in_in_in_in(const s_task_function_context &context,
		c_real_const_buffer_or_constant a1,
		c_real_buffer *a2_result,
		c_real_const_buffer_or_constant b0,
		c_real_const_buffer_or_constant b1,
		c_real_const_buffer_or_constant b2,
		c_real_const_buffer_or_constant signal) {
		static_cast<s_buffer_operation_biquad *>(context.task_memory)->in_inout_in_in_in_in(
			context.buffer_size, a1, a2_result, b0, b1, b2, signal);
	}

	void biquad_in_in_inout_in_in_in(const s_task_function_context &context,
		c_real_const_buffer_or_constant a1,
		c_real_const_buffer_or_constant a2,
		c_real_buffer *b0_result,
		c_real_const_buffer_or_constant b1,
		c_real_const_buffer_or_constant b2,
		c_real_const_buffer_or_constant signal) {
		static_cast<s_buffer_operation_biquad *>(context.task_memory)->in_in_inout_in_in_in(
			context.buffer_size, a1, a2, b0_result, b1, b2, signal);
	}

	void biquad_in_in_in_inout_in_in(const s_task_function_context &context,
		c_real_const_buffer_or_constant a1,
		c_real_const_buffer_or_constant a2,
		c_real_const_buffer_or_constant b0,
		c_real_buffer *b1_result,
		c_real_const_buffer_or_constant b2,
		c_real_const_buffer_or_constant signal) {
		static_cast<s_buffer_operation_biquad *>(context.task_memory)->in_in_in_inout_in_in(
			context.buffer_size, a1, a2, b0, b1_result, b2, signal);
	}

	void biquad_in_in_in_in_inout_in(const s_task_function_context &context,
		c_real_const_buffer_or_constant a1,
		c_real_const_buffer_or_constant a2,
		c_real_const_buffer_or_constant b0,
		c_real_const_buffer_or_constant b1,
		c_real_buffer *b2_result,
		c_real_const_buffer_or_constant signal) {
		static_cast<s_buffer_operation_biquad *>(context.task_memory)->in_in_in_in_inout_in(
			context.buffer_size, a1, a2, b0, b1, b2_result, signal);
	}

	void biquad_in_in_in_in_in_inout(const s_task_function_context &context,
		c_real_const_buffer_or_constant a1,
		c_real_const_buffer_or_constant a2,
		c_real_const_buffer_or_constant b0,
		c_real_const_buffer_or_constant b1,
		c_real_const_buffer_or_constant b2,
		c_real_buffer *signal_result) {
		static_cast<s_buffer_operation_biquad *>(context.task_memory)->in_in_in_in_in_inout(
			context.buffer_size, a1, a2, b0, b1, b2, signal_result);
	}

}
