#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "native_module/native_module.h"

const uint32 k_time_library_id = 6;

namespace time_native_modules wl_library(k_time_library_id, "time", 0) {

	wl_native_module(0xfa05392c, "period")
	wl_runtime_only
	void period(wl_in wl_const real32 duration, wl_out_return real32 &result);

}

