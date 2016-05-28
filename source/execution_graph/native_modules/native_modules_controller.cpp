#include "execution_graph/native_modules/native_modules_controller.h"
#include "execution_graph/native_module_registry.h"

static const char *k_native_modules_controller_library_name = "controller";
const s_native_module_uid k_native_module_controller_get_note_id_uid				= s_native_module_uid::build(k_native_modules_controller_library_id, 0);
const s_native_module_uid k_native_module_controller_get_note_velocity_uid			= s_native_module_uid::build(k_native_modules_controller_library_id, 1);
const s_native_module_uid k_native_module_controller_get_note_press_duration_uid	= s_native_module_uid::build(k_native_modules_controller_library_id, 2);
const s_native_module_uid k_native_module_controller_get_note_release_duration_uid	= s_native_module_uid::build(k_native_modules_controller_library_id, 3);

void register_native_modules_controller() {
	c_native_module_registry::register_native_module_library(
		k_native_modules_controller_library_id, k_native_modules_controller_library_name);

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_controller_get_note_id_uid, NATIVE_PREFIX "controller_get_note_id",
		true, s_native_module_argument_list::build(NMA(out, real, "")),
		nullptr));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_controller_get_note_velocity_uid, NATIVE_PREFIX "controller_get_note_velocity",
		true, s_native_module_argument_list::build(NMA(out, real, "")),
		nullptr));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_controller_get_note_press_duration_uid, NATIVE_PREFIX "controller_get_note_press_duration",
		true, s_native_module_argument_list::build(NMA(out, real, "")),
		nullptr));

	c_native_module_registry::register_native_module(s_native_module::build(
		k_native_module_controller_get_note_release_duration_uid, NATIVE_PREFIX "controller_get_note_release_duration",
		true, s_native_module_argument_list::build(NMA(out, real, "")),
		nullptr));
}