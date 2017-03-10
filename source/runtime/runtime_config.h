#ifndef WAVELANG_RUNTIME_CONFIG_H__
#define WAVELANG_RUNTIME_CONFIG_H__

#include "common/common.h"

#include "driver/controller_driver.h"
#include "driver/sample_format.h"

class c_audio_driver_interface;
class c_controller_driver_interface;

struct s_audio_device_info;
struct s_controller_device_info;

enum e_runtime_config_result {
	k_runtime_config_result_success,
	k_runtime_config_result_does_not_exist,
	k_runtime_config_result_error,

	k_runtime_config_result_count
};

class c_runtime_config {
public:
	struct s_settings {
		uint32 audio_device_index;
		uint32 audio_sample_rate;
		uint32 audio_input_channels;
		uint32 audio_output_channels;
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
	void set_default_audio(const s_audio_device_info *audio_device_info);
	void set_default_controller_device(const c_controller_driver_interface *controller_driver_interface);
	void set_default_controller(const s_controller_device_info *controller_device_info);
	void set_default_executor();

	s_settings m_settings;
};

#endif // WAVELANG_RUNTIME_CONFIG_H__