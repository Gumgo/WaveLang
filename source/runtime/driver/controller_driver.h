#pragma once

#include "common/common.h"

#include "engine/controller.h"

static constexpr size_t k_max_controller_devices = 16;

enum class e_controller_driver_result {
	k_success,
	k_failed_to_open_stream,

	k_count
};

struct s_controller_driver_result {
	e_controller_driver_result result;
	std::string message;

	void clear() {
		result = e_controller_driver_result::k_success;
		message.clear();
	}
};

using f_controller_clock = real64 (*)(void *context);
using f_controller_event_hook = bool (*)(void *context, const s_controller_event &controller_event);

struct s_controller_driver_settings {
	size_t device_count = 1;

	// Indices of the controller devices to stream data from
	s_static_array<uint32, k_max_controller_devices> device_indices;

	// Maximum number of allowed queued controller events before they start dropping
	size_t controller_event_queue_size = 1024;

	// Amount of "unknown" latency to compensate for things like scheduler jitter
	real64 unknown_latency = 0.015;

	// Controller clock callback
	f_controller_clock clock = nullptr;
	void *clock_context = nullptr;

	// Controller event hook, used to perform custom processing on controller events (e.g. MIDI SysEx messages), returns
	// true if the event was consumed and should not be processed
	f_controller_event_hook event_hook = nullptr;
	void *event_hook_context = nullptr;

	s_controller_driver_settings() {
		zero_type(device_indices.get_elements(), device_indices.get_count());
	}
};

struct s_controller_device_info {
	const char *name;
};

using f_submit_controller_event = void (*)(void *context, const s_controller_event &controller_event);

