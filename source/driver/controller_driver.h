#ifndef WAVELANG_DRIVER_CONTROLLER_DRIVER_H__
#define WAVELANG_DRIVER_CONTROLLER_DRIVER_H__

#include "common/common.h"

#include "driver/controller.h"

static const size_t k_max_controller_devices = 16;

enum e_controller_driver_result {
	k_controller_driver_result_success,
	k_controller_driver_result_failed_to_open_stream,

	k_controller_driver_result_count
};

struct s_controller_driver_result {
	e_controller_driver_result result;
	std::string message;

	void clear() {
		result = k_controller_driver_result_success;
		message.clear();
	}
};

typedef real64 (*f_controller_clock)(void *context);

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

	void set_default() {
		device_count = 1;
		ZERO_STRUCT(&device_indices);
		controller_event_queue_size = 1024;
		unknown_latency = 0.015;
		clock = nullptr;
		clock_context = nullptr;
	}
};

struct s_controller_device_info {
	const char *name;
};

typedef void (*f_submit_controller_event)(void *context, const s_controller_event &controller_event);

#endif // WAVELANG_DRIVER_CONTROLLER_DRIVER_H__
