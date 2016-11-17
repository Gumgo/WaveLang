#ifndef WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_CONTROLLER_H__
#define WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_CONTROLLER_H__

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "execution_graph/native_module.h"

const uint32 k_controller_library_id = 7;

namespace controller_native_modules wl_library(k_controller_library_id, "controller", 0) {

	wl_native_module(0x6b31d702, "get_note_id")
	wl_runtime_only
	void get_note_id(wl_out_return real32 &result);

	wl_native_module(0x3b1024b5, "get_note_velocity")
	wl_runtime_only
	void get_note_velocity(wl_out_return real32 &result);

	wl_native_module(0xfdfdc707, "get_note_press_duration")
	wl_runtime_only
	void get_note_press_duration(wl_out_return real32 &result);

	wl_native_module(0xbe234212, "get_note_release_duration")
	wl_runtime_only
	void get_note_release_duration(wl_out_return real32 &result);

	wl_native_module(0x45079d90, "get_parameter_value")
	wl_runtime_only
	void get_parameter_value(wl_in_const real32 parameter_id, wl_out_return real32 &result);

}

#endif // WAVELANG_EXECUTION_GRAPH_NATIVE_MODULES_NATIVE_MODULES_CONTROLLER_H__
