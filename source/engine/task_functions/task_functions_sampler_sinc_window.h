#ifndef WAVELANG_TASK_FUNCTIONS_SAMPLER_SINC_WINDOW_H__
#define WAVELANG_TASK_FUNCTIONS_SAMPLER_SINC_WINDOW_H__

#include "common/common.h"

#include "engine/math/math.h"
#include "engine/sample/sample.h"

// The size in samples of the sinc window, inclusive of the first and last sample
static const size_t k_sinc_window_size = 9;

// The number of windowed sinc values computed per sample
// The total number of sinc window samples (excluding padding) is (window_size-1) * resolution
static const size_t k_sinc_window_sample_resolution = 32;

static_assert(k_sinc_window_size % 2 == 1, "Window must be odd in order to be centered on a sample and symmetrical");
static_assert(k_sinc_window_size / 2 <= k_max_sample_padding, "Window half-size cannot exceeds max sample padding");

// To be more cache-friendly, we precompute the slopes and place them adjacent to the values. Otherwise we would need to
// traverse two arrays in parallel. The cost is that we use 2x memory (which could ultimately hurt caching).
struct ALIGNAS_SSE s_sinc_window_coefficients {
	// Four sinc window values, each spaced apart by 1 sample distance
	real32 values[k_sse_block_elements];

	// Slopes corresponding to each value for linear interpolation
	real32 slopes[k_sse_block_elements];
};

#include "sinc_window_coefficients.inl"

static_assert(NUMBEROF(k_sinc_window_coefficients) == k_sinc_window_sample_resolution,
	"Windowed sinc filter coefficient count mismatch");
static_assert(NUMBEROF(k_sinc_window_coefficients[0]) ==
	ALIGN_SIZE(k_sinc_window_size - 1, k_sse_block_elements) / k_sse_block_elements,
	"Windowed sinc filter coefficient count mismatch");

#endif // WAVELANG_TASK_FUNCTIONS_SAMPLER_SINC_WINDOW_H__