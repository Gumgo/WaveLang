#ifndef WAVELANG_ENGINE_EXECUTOR_CONTROLLER_EVENT_MANAGER_H__
#define WAVELANG_ENGINE_EXECUTOR_CONTROLLER_EVENT_MANAGER_H__

#include "common/common.h"
#include "common/utility/hash_table.h"

#include "driver/controller.h"

#include <vector>

class c_controller_event_manager {
public:
	void initialize(size_t max_controller_events, size_t max_parameter_count);

	c_wrapped_array<s_timestamped_controller_event> get_writable_controller_events();
	void process_controller_events(size_t controller_event_count);
	c_wrapped_array_const<s_timestamped_controller_event> get_note_events() const;
	c_wrapped_array_const<s_timestamped_controller_event> get_parameter_change_events(
		uint32 parameter_id, real32 &out_previous_value) const;

private:
	struct s_parameter_state {
		// The last buffer this parameter was updated. If this value is not equal to m_buffer, the current_events array
		// should be considered invalid.
		int64 last_buffer_active;

		// The previous value of this parameter at the end of the last buffer
		real32 previous_value;

		// The value of previous_value for the next buffer
		real32 next_previous_value;

		// Sorted list of parameter change events for this parameter for this buffer
		c_wrapped_array_const<s_timestamped_controller_event> current_events;
	};

	struct s_parameter_id_hash {
		size_t operator()(uint32 parameter_id) const {
			return parameter_id;
		}
	};

	void update_parameter_state(uint32 parameter_id, size_t event_start_index, size_t event_count);

	// The buffer index, which gets incremented by 1 each time we process events
	int64 m_buffer;

	std::vector<s_timestamped_controller_event> m_controller_events;
	std::vector<s_timestamped_controller_event> m_sort_scratch_buffer;

	// Sorted list of note events for this buffer
	c_wrapped_array_const<s_timestamped_controller_event> m_note_events;

	// Hash table mapping parameter ID to parameter state
	c_hash_table<uint32, s_parameter_state, s_parameter_id_hash> m_parameter_state_table;
};

#endif // WAVELANG_ENGINE_EXECUTOR_CONTROLLER_EVENT_MANAGER_H__
