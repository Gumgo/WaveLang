#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_FILTER_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_FILTER_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_module.h"

const uint32 k_filter_library_id = 5;

namespace filter_native_modules wl_library(k_filter_library_id, "filter", 0) {

	// Biquad filter assumes normalized a0 coefficient
	wl_native_module(0x683cea52, "biquad")
	wl_runtime_only
	void biquad(
		wl_in real32 a1,
		wl_in real32 a2,
		wl_in real32 b0,
		wl_in real32 b1,
		wl_in real32 b2,
		wl_in real32 signal,
		wl_out_return real32 &result);

}

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_FILTER_H__
