#ifndef WAVELANG_DRIVER_CONTROLLER_DRIVER_H__
#define WAVELANG_DRIVER_CONTROLLER_DRIVER_H__

#include "common/common.h"

#include "driver/controller.h"

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
	// Index of the controller device to stream data from
	uint32 device_index;

	// Maximum number of allowed queued controller events before they start dropping
	size_t controller_event_queue_size;

	// Amount of "unknown" latency to compensate for things like scheduler jitter
	real64 unknown_latency;

	// Controller clock callback
	f_controller_clock clock;
	void *clock_context;

	void set_default() {
		device_index = 0;
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
