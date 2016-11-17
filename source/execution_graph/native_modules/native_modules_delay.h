#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_DELAY_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_DELAY_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_module.h"

const uint32 k_delay_library_id = 4;

namespace delay_native_modules wl_library(k_delay_library_id, "delay", 0) {

	wl_native_module(0x95f17597, "delay")
	wl_runtime_only
	void delay(wl_in_const real32 duration, wl_in real32 signal, wl_out_return real32 &result);

}

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_DELAY_H__
