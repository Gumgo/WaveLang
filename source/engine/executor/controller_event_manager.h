#ifndef WAVELANG_CONTROLLER_EVENT_MANAGER_H__
#define WAVELANG_CONTROLLER_EVENT_MANAGER_H__

#include "common/common.h"
#include "driver/controller.h"

#include <vector>

class c_controller_event_manager {
public:
	void initialize(size_t max_controller_events);

	c_wrapped_array<s_timestamped_controller_event> get_writable_controller_events();
	void process_controller_events(size_t controller_event_count);
	c_wrapped_array_const<s_timestamped_controller_event> get_note_events() const;

private:
	std::vector<s_timestamped_controller_event> m_controller_events;
	std::vector<s_timestamped_controller_event> m_sort_scratch_buffer;

	c_wrapped_array_const<s_timestamped_controller_event> m_note_events;
};

#endif // WAVELANG_CONTROLLER_EVENT_MANAGER_H__