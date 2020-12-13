#pragma once

#include "common/common.h"
#include "common/scraper_attributes.h"

#include "native_module/native_module.h"

const uint32 k_json_library_id = 9;

namespace json_native_modules wl_library(k_json_library_id, "json", 0) {

	wl_library_compiler_initializer
	void *json_library_compiler_initializer();

	wl_library_compiler_deinitializer
	void json_library_compiler_deinitializer(void *library_context);

	wl_native_module(0x24e6ff0c, "read_real")
	void read_real(
		const s_native_module_context &context,
		wl_in wl_const const c_native_module_string &filename,
		wl_in wl_const const c_native_module_string & path,
		wl_out_return wl_const real32 &result);

	wl_native_module(0x24ed0917, "read_real_array")
	void read_real_array(
		const s_native_module_context &context,
		wl_in wl_const const c_native_module_string &filename,
		wl_in wl_const const c_native_module_string &path,
		wl_out_return wl_const c_native_module_real_array &result);

	wl_native_module(0x77adcb5c, "read_bool")
	void read_bool(
		const s_native_module_context &context,
		wl_in wl_const const c_native_module_string &filename,
		wl_in wl_const const c_native_module_string &path,
		wl_out_return wl_const bool &result);

	wl_native_module(0xf8c9553d, "read_bool_array")
	void read_bool_array(
		const s_native_module_context &context,
		wl_in wl_const const c_native_module_string &filename,
		wl_in wl_const const c_native_module_string &path,
		wl_out_return wl_const c_native_module_bool_array &result);

	wl_native_module(0x4a8c9808, "read_string")
	void read_string(
		const s_native_module_context &context,
		wl_in wl_const const c_native_module_string &filename,
		wl_in wl_const const c_native_module_string &path,
		wl_out_return wl_const c_native_module_string &result);

	wl_native_module(0xa684c942, "read_string_array")
	void read_string_array(
		const s_native_module_context &context,
		wl_in wl_const const c_native_module_string &filename,
		wl_in wl_const const c_native_module_string &path,
		wl_out_return wl_const c_native_module_string_array &result);

}
