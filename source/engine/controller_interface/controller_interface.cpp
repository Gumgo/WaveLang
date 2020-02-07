#include "engine/controller_interface/controller_interface.h"
#include "engine/executor/controller_event_manager.h"

c_controller_interface::c_controller_interface(const c_controller_event_manager *controller_event_manager)
	: m_controller_event_manager(controller_event_manager) {
	wl_assert(controller_event_manager);
}

c_wrapped_array<const s_timestamped_controller_event> c_controller_interface::get_parameter_change_events(
	uint32 parameter_id, real32 &out_previous_value) const {
	return m_controller_event_manager->get_parameter_change_events(parameter_id, out_previous_value);
}
