#pragma once

#include "common/common.h"

#include "driver/controller.h"

static const size_t k_max_controller_devices = 16;

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
	size_t device_count;

	// Indices of the controller devices to stream data from
	s_static_array<uint32, k_max_controller_devices> device_indices;

	// Maximum number of allowed queued controller events before they start dropping
	size_t controller_event_queue_size;

	// Amount of "unknown" latency to compensate for things like scheduler jitter
	real64 unknown_latency;

	// Controller clock callback
	f_controller_clock clock;
	void *clock_context;

	// Controller event hook, used to perform custom processing on controller events (e.g. MIDI SysEx messages), returns
	// true if the event was consumed and should not be processed
	f_controller_event_hook event_hook;
	void *event_hook_context;

	void set_default() {
		device_count = 1;
		ZERO_STRUCT(&device_indices);
		controller_event_queue_size = 1024;
		unknown_latency = 0.015;
		clock = nullptr;
		clock_context = nullptr;
		event_hook = nullptr;
		event_hook_context = nullptr;
	}
};

struct s_controller_device_info {
	const char *name;
};

using f_submit_controller_event = void (*)(void *context, const s_controller_event &controller_event);

