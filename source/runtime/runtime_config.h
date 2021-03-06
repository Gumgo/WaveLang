#pragma once

#include "common/common.h"

#include "engine/sample_format.h"

#include "runtime/driver/audio_driver_interface.h"
#include "runtime/driver/controller_driver.h"

class c_audio_driver_interface;
class c_controller_driver_interface;

struct s_audio_device_info;
struct s_controller_device_info;

enum class e_runtime_config_result {
	k_success,
	k_does_not_exist,
	k_error,

	k_count
};

class c_runtime_config {
public:
	struct s_settings {
		uint32 audio_input_device_index;
		uint32 audio_input_channel_count;
		s_static_array<uint32, k_max_audio_channels> audio_input_channel_indices;
		uint32 audio_output_device_index;
		uint32 audio_output_channel_count;
		s_static_array<uint32, k_max_audio_channels> audio_output_channel_indices;
		uint32 audio_sample_rate;
		e_sample_format audio_sample_format;
		uint32 audio_frames_per_buffer;

		size_t controller_device_count;
		s_static_array<uint32, k_max_controller_devices> controller_device_indices;
		uint32 controller_event_queue_size;
		uint32 controller_unknown_latency;

		uint32 executor_thread_count;
		uint32 executor_max_controller_parameters;
		bool executor_console_enabled;
		bool executor_profiling_enabled;
		real32 executor_profiling_threshold;
	};

	// Produces a self-documenting settings file
	static bool write_default_settings(const char *fname);

	void initialize(
		const c_audio_driver_interface *audio_driver_interface,
		const c_controller_driver_interface *controller_driver_interface);
	e_runtime_config_result read_settings(
		const c_audio_driver_interface *audio_driver_interface,
		const c_controller_driver_interface *controller_driver_interface,
		const char *fname);

	const s_settings &get_settings() const;

private:
	void set_default_audio_device(const c_audio_driver_interface *audio_driver_interface);
	void set_default_audio(
		const s_audio_device_info *audio_input_device_info,
		const s_audio_device_info *audio_output_device_info);
	void set_default_controller_device(const c_controller_driver_interface *controller_driver_interface);
	void set_default_controller(const s_controller_device_info *controller_device_info);
	void set_default_executor();

	s_settings m_settings;
};

