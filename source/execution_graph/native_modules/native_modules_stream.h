#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_module.h"

const uint32 k_stream_library_id = 8;

namespace stream_native_modules wl_library(k_stream_library_id, "stream", 0) {

	wl_native_module(0x2e1c0616, "get_sample_rate")
	void get_sample_rate(const s_native_module_context &context, wl_out_return real32 &result);
}

