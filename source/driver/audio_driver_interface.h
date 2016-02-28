#ifndef WAVELANG_AUDIO_DRIVER_INTERFACE_H__
#define WAVELANG_AUDIO_DRIVER_INTERFACE_H__

#include "common/common.h"
#include "driver/sample_format.h"
#include <string>

// $TODO VST integration will eventually just act as another driver

// Forward declarations for PA
typedef void PaStream;
struct PaStreamCallbackTimeInfo;
typedef unsigned long PaStreamCallbackFlags;

enum e_audio_driver_result {
	k_audio_driver_result_success,
	k_audio_driver_result_initialization_failed,
	k_audio_driver_result_failed_to_query_devices,
	k_audio_driver_result_settings_not_supported,
	k_audio_driver_result_failed_to_open_stream,
	k_audio_driver_result_failed_to_start_stream,

	k_audio_driver_result_count
};

struct s_audio_driver_result {
	e_audio_driver_result result;
	std::string message;

	inline void clear() {
		result = k_audio_driver_result_success;
		message.clear();
	}
};

struct s_audio_driver_settings;

struct s_audio_driver_stream_callback_context {
	const s_audio_driver_settings *driver_settings;
	void *output_buffers;
	// $TODO add more data

	void *user_data;
};

typedef void (*f_audio_driver_stream_callback)(const s_audio_driver_stream_callback_context &context);

struct s_audio_driver_settings {
	// Index of the device to use for the stream
	uint32 device_index;

	// Sample rate of the stream
	real64 sample_rate;

	// Number of output channels (1 = mono, 2 = stereo, etc.)
	uint32 output_channels;

	// Format of each sample
	e_sample_format sample_format;

	// Number of frames in each requested buffer
	uint32 frames_per_buffer;

	// Stream callback
	f_audio_driver_stream_callback stream_callback;
	void *stream_callback_user_data;

	void set_default() {
		device_index = 0;
		sample_rate = 44100.0;
		output_channels = 2;
		sample_format = k_sample_format_float32;
		frames_per_buffer = 512;
		stream_callback = nullptr;
		stream_callback_user_data = nullptr;
	}
};

struct s_audio_device_info {
	const char *name;
	// $TODO host API identifier?
	uint32 max_output_channels;
	real64 default_low_output_latency;
	real64 default_high_output_latency;
	real64 default_sample_rate;
};

class c_audio_driver_interface {
public:
	c_audio_driver_interface();
	~c_audio_driver_interface();

	s_audio_driver_result initialize();
	void shutdown();

	uint32 get_device_count() const;
	uint32 get_default_device_index() const;
	s_audio_device_info get_device_info(uint32 device_index) const;

	bool are_settings_supported(const s_audio_driver_settings &settings) const;

	s_audio_driver_result start_stream(const s_audio_driver_settings &settings);
	void stop_stream();
	bool is_stream_running() const;
	const s_audio_driver_settings &get_settings() const;

private:
	static int stream_callback_internal(
		const void *input, void *output, unsigned long frame_count,
		const PaStreamCallbackTimeInfo *time_info, PaStreamCallbackFlags status_flags, void *user_data);

	bool m_initialized;

	uint32 m_device_count;
	uint32 m_default_device_index;
	PaStream *m_stream;
	s_audio_driver_settings m_settings;
};

#endif // WAVELANG_AUDIO_DRIVER_INTERFACE_H__