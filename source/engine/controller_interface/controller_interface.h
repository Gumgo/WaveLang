#ifndef WAVELANG_ENGINE_CONTROLLER_INTERFACE_CONTROLLER_INTERFACE_H__
#define WAVELANG_ENGINE_CONTROLLER_INTERFACE_CONTROLLER_INTERFACE_H__

#include "common/common.h"

#include "driver/controller.h"

class c_controller_event_manager;

class c_controller_interface {
public:
	c_controller_interface(const c_controller_event_manager *controller_event_manager);
	c_wrapped_array<const s_timestamped_controller_event> get_parameter_change_events(
		uint32 parameter_id, real32 &out_previous_value) const;

private:
	const c_controller_event_manager *m_controller_event_manager;
};

#endif // WAVELANG_ENGINE_CONTROLLER_INTERFACE_CONTROLLER_INTERFACE_H__
