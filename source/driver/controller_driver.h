#ifndef WAVELANG_CONTROLLER_DRIVER_H__
#define WAVELANG_CONTROLLER_DRIVER_H__

#include "common/common.h"

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

struct s_controller_driver_settings {
	uint32 device_index;

	void set_default() {
		device_index = 0;
	}
};

struct s_controller_device_info {
	const char *name;
};

#endif // WAVELANG_CONTROLLER_DRIVER_H__