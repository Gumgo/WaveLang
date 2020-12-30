#include "execution_graph/native_module_registration.h"

namespace controller_native_modules {

	void get_note_id(wl_argument(return out real, result));

	void get_note_velocity(wl_argument(return out real, result));

	void get_note_press_duration(wl_argument(return out real, result));

	void get_note_release_duration(wl_argument(return out real, result));

	void get_parameter_value(
		wl_argument(in const real, parameter_id),
		wl_argument(return out real, result));

	static constexpr uint32 k_controller_library_id = 7;
	wl_native_module_library(k_controller_library_id, "controller", 0);

	wl_native_module(0x6b31d702, "get_note_id")
		.set_call_signature<decltype(get_note_id)>();

	wl_native_module(0x3b1024b5, "get_note_velocity")
		.set_call_signature<decltype(get_note_velocity)>();

	wl_native_module(0xfdfdc707, "get_note_press_duration")
		.set_call_signature<decltype(get_note_press_duration)>();

	wl_native_module(0xbe234212, "get_note_release_duration")
		.set_call_signature<decltype(get_note_release_duration)>();

	wl_native_module(0x45079d90, "get_parameter_value")
		.set_call_signature<decltype(get_parameter_value)>();

	wl_end_active_library_native_module_registration();
}
