#pragma once

#include "common/common.h"

#include "engine/sample_format.h"

#include <string>

// $TODO VST integration will eventually just act as another driver

// Forward declarations for PA
using PaStream = void;
struct PaStreamCallbackTimeInfo;
using PaStreamCallbackFlags = unsigned long;

static constexpr size_t k_max_audio_channels = 16;

enum class e_audio_driver_result {
	k_success,
	k_initialization_failed,
	k_failed_to_query_devices,
	k_settings_not_supported,
	k_failed_to_open_stream,
	k_failed_to_start_stream,

	k_count
};

struct s_audio_driver_result {
	e_audio_driver_result result;
	std::string message;

	inline void clear() {
		result = e_audio_driver_result::k_success;
		message.clear();
	}
};

struct s_audio_driver_settings;

struct s_audio_driver_stream_callback_context {
	const s_audio_driver_settings *driver_settings;
	real64 buffer_time_sec;
	c_wrapped_array<const uint8> input_buffer;
	c_wrapped_array<uint8> output_buffer;
	// $TODO add more data

	void *user_data;
};

using f_audio_driver_stream_callback = void (*)(const s_audio_driver_stream_callback_context &context);

using f_audio_driver_stream_clock = real64 (*)(void *context);

struct s_audio_driver_settings {
	// Index of the device to use for the input stream
	uint32 input_device_index = 0;

	// Number of input channels
	uint32 input_channel_count = 1;

	// Input channel indices
	s_static_array<uint32, k_max_audio_channels> input_channel_indices;

	// Index of the device to use for the output stream
	uint32 output_device_index = 0;

	// Number of output channels (1 = mono, 2 = stereo, etc.)
	uint32 output_channel_count = 2;

	// Output channel indices
	s_static_array<uint32, k_max_audio_channels> output_channel_indices;

	// Sample rate of the stream
	real64 sample_rate = 44100.0;

	// Format of each sample
	e_sample_format sample_format = e_sample_format::k_float32;

	// Number of frames in each requested buffer
	uint32 frames_per_buffer = 512;

	// Stream callback
	f_audio_driver_stream_callback stream_callback = nullptr;
	void *stream_callback_user_data = nullptr;
};

struct s_audio_device_info {
	const char *name;
	const char *host_api_name;

	uint32 max_input_channels;
	real64 default_low_input_latency;
	real64 default_high_input_latency;

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
	uint32 get_default_input_device_index() const;
	uint32 get_default_output_device_index() const;
	s_audio_device_info get_device_info(uint32 device_index) const;

	bool are_settings_supported(const s_audio_driver_settings &settings) const;

	s_audio_driver_result start_stream(const s_audio_driver_settings &settings);
	void stop_stream();
	bool is_stream_running() const;
	const s_audio_driver_settings &get_settings() const;

	void get_stream_clock(f_audio_driver_stream_clock &clock_out, void *&context_out);

private:
	static int stream_callback_internal(
		const void *input,
		void *output,
		unsigned long frame_count,
		const PaStreamCallbackTimeInfo *time_info,
		PaStreamCallbackFlags status_flags,
		void *user_data);

	bool m_initialized;

	uint32 m_device_count;
	uint32 m_default_input_device_index;
	uint32 m_default_output_device_index;
	PaStream *m_stream;
	s_audio_driver_settings m_settings;
};

