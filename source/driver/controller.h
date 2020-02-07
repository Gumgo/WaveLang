#pragma once

#include "common/common.h"

enum e_controller_event_type {
	k_controller_event_type_note_on,
	k_controller_event_type_note_off,
	k_controller_event_type_parameter_change,

	k_controller_event_type_count
};

struct s_controller_event {
	static const size_t k_max_controller_event_data_size = 12;
	static_assert(k_max_controller_event_data_size % sizeof(uint32) == 0,
		"Max event data size not a multiple of 4");

	e_controller_event_type event_type;
	s_static_array<uint32, k_max_controller_event_data_size / sizeof(uint32)> event_data;

	template<typename t_data> void set_data(const t_data *data) {
		static_assert(sizeof(t_data) <= sizeof(event_data), "Event data too big");
		static_assert(alignof(t_data) <= alignof(uint32), "Event data alignment too big");
		memcpy(event_data, data, sizeof(t_data));
	}

	template<typename t_data> const t_data *get_data() const {
		static_assert(sizeof(t_data) <= sizeof(event_data), "Event data too big");
		static_assert(alignof(t_data) <= alignof(uint32), "Event data alignment too big");
		return reinterpret_cast<const t_data *>(event_data.get_elements());
	}

	template<typename t_data> t_data *get_data() {
		static_assert(sizeof(t_data) <= sizeof(event_data), "Event data too big");
		static_assert(alignof(t_data) <= alignof(uint32), "Event data alignment too big");
		return reinterpret_cast<t_data *>(event_data.get_elements());
	}
};

struct s_timestamped_controller_event {
	real64 timestamp_sec;
	s_controller_event controller_event;
};

struct s_controller_event_data_note_on {
	int32 note_id;
	real32 velocity;
};

struct s_controller_event_data_note_off {
	int32 note_id;
	real32 velocity;
};

struct s_controller_event_data_parameter_change {
	uint32 parameter_id;
	real32 value;
};

