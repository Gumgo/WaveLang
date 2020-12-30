#include "driver/audio_driver_interface.h"

#include <portaudio.h>

#if IS_TRUE(PLATFORM_LINUX)
// For the Raspberry Pi
#include <pa_linux_alsa.h>
#endif // IS_TRUE(PLATFORM_LINUX)

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
	PaStreamParameters &input_params_out,
	PaStreamParameters &output_params_out) {
	wl_assert(valid_index(settings.device_index, device_count));
	wl_assert(settings.frames_per_buffer > 0);

	PaDeviceIndex device_index = static_cast<PaDeviceIndex>(settings.device_index);
	const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
	wl_assert(device_info);

	// $TODO $INPUT fill these out eventually if we want to support line-in processing
	zero_type(&input_params_out);
	input_params_out.device = paNoDevice;

	zero_type(&output_params_out);
	output_params_out.device = device_index;
	output_params_out.channelCount = cast_integer_verify<int>(settings.output_channels);
	output_params_out.sampleFormat = get_pa_sample_format(settings.sample_format);
	output_params_out.suggestedLatency = device_info->defaultLowOutputLatency;
	// $TODO should suggestedLatency be user-provided?
}

static real64 stream_clock(void *context) {
	wl_assert(context);
	return Pa_GetStreamTime(static_cast<PaStream *>(context));
}

c_audio_driver_interface::c_audio_driver_interface() {
	m_initialized = false;
	m_device_count = 0;
	m_default_device_index = 0;
	m_stream = nullptr;
}

c_audio_driver_interface::~c_audio_driver_interface() {
	shutdown();
}

s_audio_driver_result c_audio_driver_interface::initialize() {
	wl_assert(!m_initialized);

	s_audio_driver_result result;
	result.clear();

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

	int32 default_device_index = Pa_GetDefaultOutputDevice();
	if (default_device_index == paNoDevice) {
		default_device_index = 0;
	}

	m_default_device_index = cast_integer_verify<uint32>(default_device_index);

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

uint32 c_audio_driver_interface::get_default_device_index() const {
	wl_assert(m_initialized);
	return m_default_device_index;
}

s_audio_device_info c_audio_driver_interface::get_device_info(uint32 device_index) const {
	wl_assert(m_initialized);
	wl_assert(valid_index(device_index, m_device_count));

	s_audio_device_info result;
	const PaDeviceInfo *device_info = Pa_GetDeviceInfo(static_cast<PaDeviceIndex>(device_index));
	result.name = device_info->name;
	result.max_output_channels = cast_integer_verify<uint32>(device_info->maxOutputChannels);
	result.default_low_output_latency = device_info->defaultLowOutputLatency;
	result.default_high_output_latency = device_info->defaultHighOutputLatency;
	result.default_sample_rate = device_info->defaultSampleRate;

	return result;
}

bool c_audio_driver_interface::are_settings_supported(const s_audio_driver_settings &settings) const {
	wl_assert(m_initialized);

	PaStreamParameters input_params;
	PaStreamParameters output_params;
	setup_stream_parameters(settings, m_device_count, input_params, output_params);

	// $TODO $INPUT pass in input_params if we ever want to support line-in processing
	PaError result = Pa_IsFormatSupported(nullptr, &output_params, settings.sample_rate);
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

	PaStreamParameters input_params;
	PaStreamParameters output_params;
	setup_stream_parameters(settings, m_device_count, input_params, output_params);

	PaError error = Pa_OpenStream(
		&m_stream,
		nullptr, // $TODO $INPUT pass in input_params if we want to support line-in processing
		&output_params,
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

	m_settings = settings;

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

	size_t output_buffer_size =
		context.driver_settings->output_channels *
		frame_count *
		get_sample_format_size(context.driver_settings->sample_format);
	context.output_buffer = c_wrapped_array<uint8>(static_cast<uint8 *>(output), output_buffer_size);
	context.user_data = this_ptr->m_settings.stream_callback_user_data;
	// $TODO fill in the rest

	this_ptr->m_settings.stream_callback(context);

	return 0;
}
