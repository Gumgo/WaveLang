#include "runtime/driver/audio_driver_interface.h"

#include <pa_asio.h>
#include <portaudio.h>

#if IS_TRUE(PLATFORM_LINUX)
// For the Raspberry Pi
#include <pa_linux_alsa.h>
#endif // IS_TRUE(PLATFORM_LINUX)

extern "C" {
	// For some reason, PortAudio doesn't expose these
	typedef void (*PaUtilLogCallback)(const char *log);
	void PaUtil_SetDebugPrintFunction(PaUtilLogCallback cb);
}

struct s_stream_parameters {
	PaStreamParameters input_params;
	PaAsioStreamInfo input_params_asio;
	s_static_array<int, k_max_audio_channels> input_channel_selectors;

	PaStreamParameters output_params;
	PaAsioStreamInfo output_params_asio;
	s_static_array<int, k_max_audio_channels> output_channel_selectors;
};

static void ignore_debug_output(const char *log) {}

static PaSampleFormat get_pa_sample_format(e_sample_format sample_format) {
	switch (sample_format) {
	case e_sample_format::k_float32:
		return paFloat32;

	default:
		wl_unreachable();
		return 0;
	}
}

static void setup_stream_parameters(
	const s_audio_driver_settings &settings,
	uint32 device_count,
	s_stream_parameters &stream_params_out) {
	wl_assert(valid_index(settings.input_device_index, device_count));
	wl_assert(valid_index(settings.output_device_index, device_count));
	wl_assert(settings.frames_per_buffer > 0);

	PaDeviceIndex input_device_index = static_cast<PaDeviceIndex>(settings.input_device_index);
	const PaDeviceInfo *input_device_info = Pa_GetDeviceInfo(input_device_index);
	wl_assert(input_device_info);

	PaDeviceIndex output_device_index = static_cast<PaDeviceIndex>(settings.output_device_index);
	const PaDeviceInfo *output_device_info = Pa_GetDeviceInfo(output_device_index);
	wl_assert(output_device_info);

	// $TODO should suggestedLatency be user-provided?

	zero_type(&stream_params_out);

	if (settings.input_channel_count == 0) {
		stream_params_out.input_params.device = paNoDevice;
	} else {
		stream_params_out.input_params.device = input_device_index;
		stream_params_out.input_params.channelCount = cast_integer_verify<int>(settings.input_channel_count);
		stream_params_out.input_params.sampleFormat = get_pa_sample_format(settings.sample_format);
		stream_params_out.input_params.suggestedLatency = input_device_info->defaultLowInputLatency;
	}

	stream_params_out.output_params.device = output_device_index;
	stream_params_out.output_params.channelCount = cast_integer_verify<int>(settings.output_channel_count);
	stream_params_out.output_params.sampleFormat = get_pa_sample_format(settings.sample_format);
	stream_params_out.output_params.suggestedLatency = output_device_info->defaultLowOutputLatency;

	if (Pa_GetHostApiInfo(input_device_info->hostApi)->type == paASIO) {
		stream_params_out.input_params.hostApiSpecificStreamInfo = &stream_params_out.input_params_asio;
		stream_params_out.input_params_asio.size = sizeof(PaAsioStreamInfo);
		stream_params_out.input_params_asio.hostApiType = paASIO;
		stream_params_out.input_params_asio.version = 1;
		stream_params_out.input_params_asio.flags = paAsioUseChannelSelectors;
		stream_params_out.input_params_asio.channelSelectors =
			stream_params_out.input_channel_selectors.get_elements();
		for (size_t channel = 0; channel < settings.input_channel_count; channel++) {
			stream_params_out.input_channel_selectors[channel] =
				cast_integer_verify<int32>(settings.input_channel_indices[channel]);
		}
	}

	if (Pa_GetHostApiInfo(output_device_info->hostApi)->type == paASIO) {
		stream_params_out.output_params.hostApiSpecificStreamInfo = &stream_params_out.output_params_asio;
		stream_params_out.output_params_asio.size = sizeof(PaAsioStreamInfo);
		stream_params_out.output_params_asio.hostApiType = paASIO;
		stream_params_out.output_params_asio.version = 1;
		stream_params_out.output_params_asio.flags = paAsioUseChannelSelectors;
		stream_params_out.output_params_asio.channelSelectors =
			stream_params_out.output_channel_selectors.get_elements();
		for (size_t channel = 0; channel < settings.output_channel_count; channel++) {
			stream_params_out.output_channel_selectors[channel] =
				cast_integer_verify<int32>(settings.output_channel_indices[channel]);
		}
	}
}

static real64 stream_clock(void *context) {
	wl_assert(context);
	return Pa_GetStreamTime(static_cast<PaStream *>(context));
}

c_audio_driver_interface::c_audio_driver_interface() {
	m_initialized = false;
	m_device_count = 0;
	m_default_input_device_index = 0;
	m_default_output_device_index = 0;
	m_stream = nullptr;
}

c_audio_driver_interface::~c_audio_driver_interface() {
	shutdown();
}

s_audio_driver_result c_audio_driver_interface::initialize() {
	wl_assert(!m_initialized);

	s_audio_driver_result result;
	result.clear();

	PaUtil_SetDebugPrintFunction(ignore_debug_output);
	PaError error = Pa_Initialize();
	if (error != paNoError) {
		m_initialized = true;
		result.result = e_audio_driver_result::k_initialization_failed;
		result.message = Pa_GetErrorText(error);
		return result;
	}

	int32 device_count = Pa_GetDeviceCount();
	if (device_count < 0) {
		result.result = e_audio_driver_result::k_failed_to_query_devices;
		result.message = Pa_GetErrorText(device_count);
		Pa_Terminate();
		return result;
	}

	m_device_count = cast_integer_verify<uint32>(device_count);

	int32 default_input_device_index = Pa_GetDefaultInputDevice();
	if (default_input_device_index == paNoDevice) {
		default_input_device_index = 0;
	}

	int32 default_output_device_index = Pa_GetDefaultOutputDevice();
	if (default_output_device_index == paNoDevice) {
		default_output_device_index = 0;
	}

	m_default_input_device_index = cast_integer_verify<uint32>(default_input_device_index);
	m_default_output_device_index = cast_integer_verify<uint32>(default_output_device_index);

	m_initialized = true;
	return result;
}

void c_audio_driver_interface::shutdown() {
	wl_assert(!m_stream);

	if (!m_initialized) {
		return;
	}

	Pa_Terminate();
	m_initialized = false;
}

uint32 c_audio_driver_interface::get_device_count() const {
	wl_assert(m_initialized);
	return m_device_count;
}

uint32 c_audio_driver_interface::get_default_input_device_index() const {
	wl_assert(m_initialized);
	return m_default_input_device_index;
}

uint32 c_audio_driver_interface::get_default_output_device_index() const {
	wl_assert(m_initialized);
	return m_default_output_device_index;
}

s_audio_device_info c_audio_driver_interface::get_device_info(uint32 device_index) const {
	wl_assert(m_initialized);
	wl_assert(valid_index(device_index, m_device_count));

	s_audio_device_info result;
	const PaDeviceInfo *device_info = Pa_GetDeviceInfo(static_cast<PaDeviceIndex>(device_index));
	const PaHostApiInfo *host_api_info = Pa_GetHostApiInfo(device_info->hostApi);
	result.name = device_info->name;
	result.host_api_name = host_api_info->name;

	result.max_input_channels = cast_integer_verify<uint32>(device_info->maxInputChannels);
	result.default_low_input_latency = device_info->defaultLowInputLatency;
	result.default_high_input_latency = device_info->defaultHighInputLatency;

	result.max_output_channels = cast_integer_verify<uint32>(device_info->maxOutputChannels);
	result.default_low_output_latency = device_info->defaultLowOutputLatency;
	result.default_high_output_latency = device_info->defaultHighOutputLatency;

	result.default_sample_rate = device_info->defaultSampleRate;

	return result;
}

bool c_audio_driver_interface::are_settings_supported(const s_audio_driver_settings &settings) const {
	wl_assert(m_initialized);

	s_stream_parameters stream_params;
	setup_stream_parameters(settings, m_device_count, stream_params);

	PaError result = Pa_IsFormatSupported(
		stream_params.input_params.device == paNoDevice ? nullptr : &stream_params.input_params,
		&stream_params.output_params,
		settings.sample_rate);
	return (result == paFormatIsSupported);
}

s_audio_driver_result c_audio_driver_interface::start_stream(const s_audio_driver_settings &settings) {
	wl_assert(m_initialized);
	wl_assert(!m_stream);

	s_audio_driver_result result;
	result.clear();

	if (!are_settings_supported(settings)) {
		result.result = e_audio_driver_result::k_settings_not_supported;
		result.message = "Settings not supported";
		return result;
	}

	m_settings = settings;

	s_stream_parameters stream_params;
	setup_stream_parameters(settings, m_device_count, stream_params);

	PaError error = Pa_OpenStream(
		&m_stream,
		stream_params.input_params.device == paNoDevice ? nullptr : &stream_params.input_params,
		&stream_params.output_params,
		settings.sample_rate,
		settings.frames_per_buffer,
		paNoFlag,
		stream_callback_internal,
		this);
	if (error != paNoError) {
		wl_assert(!m_stream);
		result.result = e_audio_driver_result::k_failed_to_open_stream;
		result.message = Pa_GetErrorText(error);
		return result;
	}

#if IS_TRUE(PLATFORM_LINUX)
	PaAlsa_EnableRealtimeScheduling(m_stream, true);
#endif // IS_TRUE(PLATFORM_LINUX)

	error = Pa_StartStream(m_stream);
	if (error != paNoError) {
		result.result = e_audio_driver_result::k_failed_to_start_stream;
		result.message = Pa_GetErrorText(error);

		// Clean up the stream
		if (Pa_CloseStream(m_stream) != paNoError) {
			wl_haltf("Failed to close stream");
		}

		m_stream = nullptr;
		return result;
	}

	wl_assert(m_stream);
	return result;
}

void c_audio_driver_interface::stop_stream() {
	if (!m_stream) {
		return;
	}

	if (Pa_StopStream(m_stream) != paNoError) {
		wl_haltf("Failed to stop stream");
	}

	if (Pa_CloseStream(m_stream) != paNoError) {
		wl_haltf("Failed to close stream");
	}

	m_stream = nullptr;
}

bool c_audio_driver_interface::is_stream_running() const {
	return m_stream != nullptr;
}

const s_audio_driver_settings &c_audio_driver_interface::get_settings() const {
	wl_assert(is_stream_running());
	return m_settings;
}

void c_audio_driver_interface::get_stream_clock(f_audio_driver_stream_clock &clock_out, void *&context_out) {
	wl_assert(is_stream_running());
	clock_out = stream_clock;
	context_out = m_stream;
}

int c_audio_driver_interface::stream_callback_internal(
	const void *input,
	void *output,
	unsigned long frame_count,
	const PaStreamCallbackTimeInfo *time_info,
	PaStreamCallbackFlags status_flags,
	void *user_data) {
	c_audio_driver_interface *this_ptr = static_cast<c_audio_driver_interface *>(user_data);

	wl_assert(frame_count == this_ptr->m_settings.frames_per_buffer);

	const PaStreamInfo *stream_info = Pa_GetStreamInfo(this_ptr->m_stream);

	s_audio_driver_stream_callback_context context;
	context.driver_settings = &this_ptr->m_settings;
	context.buffer_time_sec = time_info->outputBufferDacTime - stream_info->outputLatency;

	size_t input_buffer_size =
		context.driver_settings->output_channel_count *
		frame_count *
		get_sample_format_size(context.driver_settings->sample_format);
	context.input_buffer = c_wrapped_array<const uint8>(static_cast<const uint8 *>(input), input_buffer_size);

	size_t output_buffer_size =
		context.driver_settings->output_channel_count *
		frame_count *
		get_sample_format_size(context.driver_settings->sample_format);
	context.output_buffer = c_wrapped_array<uint8>(static_cast<uint8 *>(output), output_buffer_size);

	context.user_data = this_ptr->m_settings.stream_callback_user_data;
	// $TODO fill in the rest

	this_ptr->m_settings.stream_callback(context);

	return 0;
}
