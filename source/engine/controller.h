#pragma once

#include "common/common.h"

enum class e_controller_event_type {
	k_note_on,
	k_note_off,
	k_parameter_change,

	k_count
};

struct s_controller_event {
	static constexpr size_t k_max_controller_event_data_size = 12;
	STATIC_ASSERT(k_max_controller_event_data_size % sizeof(uint32) == 0);

	e_controller_event_type event_type;
	s_static_array<uint32, k_max_controller_event_data_size / sizeof(uint32)> event_data;

	template<typename t_data> void set_data(const t_data *data) {
		STATIC_ASSERT(sizeof(t_data) <= sizeof(event_data));
		STATIC_ASSERT(alignof(t_data) <= alignof(uint32));
		memcpy(event_data, data, sizeof(t_data));
	}

	template<typename t_data> const t_data *get_data() const {
		STATIC_ASSERT(sizeof(t_data) <= sizeof(event_data));
		STATIC_ASSERT(alignof(t_data) <= alignof(uint32));
		return reinterpret_cast<const t_data *>(event_data.get_elements());
	}

	template<typename t_data> t_data *get_data() {
		STATIC_ASSERT(sizeof(t_data) <= sizeof(event_data));
		STATIC_ASSERT(alignof(t_data) <= alignof(uint32));
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

