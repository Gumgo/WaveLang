#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "native_module/native_module.h"

const uint32 k_delay_library_id = 4;

namespace delay_native_modules wl_library(k_delay_library_id, "delay", 0) {

	wl_native_module(0x95f17597, "delay")
	wl_runtime_only
	void delay(wl_in wl_const real32 duration, wl_in real32 signal, wl_out_return real32 &result);

	wl_native_module(0x2dcd7ea7, "memory$real")
	wl_runtime_only
	void memory(wl_in real32 value, wl_in bool write, wl_out_return real32 &result);

	wl_native_module(0xe5cd6275, "memory$bool")
	wl_runtime_only
	void memory(wl_in bool value, wl_in bool write, wl_out_return bool &result);

}
