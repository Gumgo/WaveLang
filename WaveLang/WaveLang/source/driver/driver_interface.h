#ifndef WAVELANG_DRIVER_INTERFACE_H__
#define WAVELANG_DRIVER_INTERFACE_H__

#include "common/common.h"
#include <string>

// Forward declarations for PA
typedef void PaStream;
struct PaStreamCallbackTimeInfo;
typedef unsigned long PaStreamCallbackFlags;

enum e_driver_result {
	k_driver_result_success,
	k_driver_result_initialization_failed,
	k_driver_result_failed_to_query_devices,
	k_driver_result_settings_not_supported,
	k_driver_result_failed_to_open_stream,
	k_driver_result_failed_to_start_stream,

	k_driver_result_count
};

struct s_driver_result {
	e_driver_result result;
	std::string message;

	inline void clear() {
		result = k_driver_result_success;
		message.clear();
	}
};

// $TODO add more format options
enum e_sample_format {
	k_sample_format_float32,

	k_sample_format_count
};

// $TODO come up with proper arguments
typedef void (*f_driver_stream_callback)();

struct s_driver_settings {
	// Index of the device to use for the stream
	uint32 device_index;

	// Sample rate of the stream
	double sample_rate;

	// Number of output channels (1 = mono, 2 = stereo, etc.)
	uint32 output_channels;

	// Format of each sample
	e_sample_format sample_format;

	// Number of frames in each requested buffer - specify 0 to let the driver decide
	uint32 frames_per_buffer;

	// Stream callback
	f_driver_stream_callback stream_callback;

	void set_default() {
		device_index = 0;
		sample_rate = 44100.0;
		output_channels = 2;
		sample_format = k_sample_format_float32;
		frames_per_buffer = 0;
		stream_callback = nullptr;
	}
};

struct s_device_info {
	const char *name;
	// $TODO host API identifier?
	uint32 max_output_channels;
	double default_low_output_latency;
	double default_high_output_latency;
	double default_sample_rate;
};

class c_driver_interface {
public:
	c_driver_interface();
	~c_driver_interface();

	s_driver_result initialize();
	void shutdown();

	uint32 get_device_count() const;
	uint32 get_default_device_index() const;
	s_device_info get_device_info(uint32 device_index) const;

	bool are_settings_supported(const s_driver_settings &settings) const;

	s_driver_result start_stream(const s_driver_settings &settings);
	void stop_stream();

private:
	static int stream_callback_internal(
		const void *input, void *output, unsigned long frame_count,
		const PaStreamCallbackTimeInfo *time_info, PaStreamCallbackFlags status_flags, void *user_data);

	bool m_initialized;

	uint32 m_device_count;
	uint32 m_default_device_index;
	PaStream *m_stream;
	s_driver_settings m_settings;
};

#endif // WAVELANG_DRIVER_INTERFACE_H__