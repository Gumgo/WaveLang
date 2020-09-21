#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_module.h"

const uint32 k_sampler_library_id = 3;

namespace sampler_native_modules wl_library(k_sampler_library_id, "sampler", 0) {

	wl_native_module(0x929e25f0, "sampler")
	wl_runtime_only
	void sampler(
		wl_in_const const c_native_module_string &name,
		wl_in_const real32 channel,
		wl_in real32 speed,
		wl_out_return real32 &result);

	wl_native_module(0x8c1293e3, "sampler_loop")
	wl_runtime_only
	void sampler_loop(
		wl_in_const const c_native_module_string &name,
		wl_in_const real32 channel,
		wl_in_const bool bidi,
		wl_in real32 speed,
		wl_in real32 phase,
		wl_out_return real32 &result);

	wl_native_module(0x5616fda5, "sampler_wavetable")
	wl_runtime_only
	void sampler_wavetable(
		wl_in_const const c_native_module_real_array &harmonic_weights,
		wl_in real32 frequency,
		wl_in real32 phase,
		wl_out_return real32 &result);

}

