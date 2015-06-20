#include "driver/driver_interface.h"

#include <portaudio.h>
#pragma comment(lib, "portaudio_x86.lib")

static PaSampleFormat get_pa_sample_format(e_sample_format sample_format) {
	switch (sample_format) {
	case k_sample_format_float32:
		return paFloat32;

	default:
		wl_halt();
		return 0;
	}
}

static void setup_stream_parameters(const s_driver_settings &settings, uint32 device_count,
	PaStreamParameters &out_input_params, PaStreamParameters &out_output_params) {
	wl_assert(VALID_INDEX(settings.device_index, device_count));

	PaDeviceIndex device_index = static_cast<PaDeviceIndex>(settings.device_index);
	const PaDeviceInfo *device_info = Pa_GetDeviceInfo(device_index);
	wl_assert(device_info != nullptr);

	// $TODO fill these out eventually if we want to support line-in processing
	ZERO_STRUCT(&out_input_params);
	out_input_params.device = paNoDevice;

	ZERO_STRUCT(&out_output_params);
	out_output_params.device = device_index;
	out_output_params.channelCount = static_cast<int>(settings.output_channels);
	out_output_params.sampleFormat = get_pa_sample_format(settings.sample_format);
	out_output_params.suggestedLatency = device_info->defaultLowInputLatency;
	// $TODO should suggestedLatency be user-provided?
}

c_driver_interface::c_driver_interface() {
	m_initialized = false;
	m_device_count = 0;
	m_default_device_index = 0;
	m_stream = nullptr;
}

c_driver_interface::~c_driver_interface() {
	shutdown();
}

s_driver_result c_driver_interface::initialize() {
	wl_assert(!m_initialized);

	s_driver_result result;
	result.clear();

	PaError error = Pa_Initialize();
	if (error != paNoError) {
		m_initialized = true;
		result.result = k_driver_result_initialization_failed;
		result.message = Pa_GetErrorText(error);
		return result;
	}

	int32 device_count = Pa_GetDeviceCount();
	if (device_count < 0) {
		result.result = k_driver_result_failed_to_query_devices;
		result.message = Pa_GetErrorText(device_count);
		Pa_Terminate();
		return result;
	}

	m_device_count = static_cast<uint32>(device_count);
	m_default_device_index = static_cast<uint32>(Pa_GetDefaultOutputDevice());

	m_initialized = true;
	return result;
}

void c_driver_interface::shutdown() {
	wl_assert(m_stream == nullptr);

	if (!m_initialized) {
		return;
	}

	Pa_Terminate();
	m_initialized = false;
}

uint32 c_driver_interface::get_device_count() const {
	wl_assert(m_initialized);
	return m_device_count;
}

uint32 c_driver_interface::get_default_device_index() const {
	wl_assert(m_initialized);
	return m_default_device_index;
}

s_device_info c_driver_interface::get_device_info(uint32 device_index) const {
	wl_assert(m_initialized);
	wl_assert(VALID_INDEX(device_index, m_device_count));

	s_device_info result;
	const PaDeviceInfo *device_info = Pa_GetDeviceInfo(static_cast<PaDeviceIndex>(device_index));
	result.name = device_info->name;
	result.max_output_channels = static_cast<uint32>(device_info->maxOutputChannels);
	result.default_low_output_latency = device_info->defaultLowOutputLatency;
	result.default_high_output_latency = device_info->defaultHighOutputLatency;
	result.default_sample_rate = device_info->defaultSampleRate;

	return result;
}

bool c_driver_interface::are_settings_supported(const s_driver_settings &settings) const {
	wl_assert(m_initialized);

	PaStreamParameters input_params;
	PaStreamParameters output_params;
	setup_stream_parameters(settings, m_device_count, input_params, output_params);

	// $TODO pass in input_params if we ever want to support line-in processing
	PaError result = Pa_IsFormatSupported(nullptr, &output_params, settings.sample_rate);
	return (result == paFormatIsSupported);
}

s_driver_result c_driver_interface::start_stream(const s_driver_settings &settings) {
	wl_assert(m_initialized);
	wl_assert(m_stream == nullptr);

	s_driver_result result;
	result.clear();

	if (!are_settings_supported(settings)) {
		result.result = k_driver_result_settings_not_supported;
		result.message = "Settings not supported";
		return result;
	}

	PaStreamParameters input_params;
	PaStreamParameters output_params;
	setup_stream_parameters(settings, m_device_count, input_params, output_params);

	PaError error = Pa_OpenStream(
		&m_stream,
		nullptr, // $TODO pass in input_params if we want to support line-in processing
		&output_params,
		settings.sample_rate,
		settings.frames_per_buffer,
		paNoFlag,
		stream_callback_internal,
		this);
	if (error != paNoError) {
		wl_assert(m_stream == nullptr);
		result.result = k_driver_result_failed_to_open_stream;
		result.message = Pa_GetErrorText(error);
		return result;
	}

	error = Pa_StartStream(m_stream);
	if (error != paNoError) {
		result.result = k_driver_result_failed_to_start_stream;
		result.message = Pa_GetErrorText(error);

		// Clean up the stream
		if (Pa_CloseStream(m_stream) != paNoError) {
			wl_halt();
		}

		m_stream = nullptr;
		return result;
	}

	m_settings = settings;

	wl_assert(m_stream != nullptr);
	return result;
}

void c_driver_interface::stop_stream() {
	if (m_stream == nullptr) {
		return;
	}

	if (Pa_StopStream(m_stream) != paNoError) {
		wl_halt();
	}

	if (Pa_CloseStream(m_stream) != paNoError) {
		wl_halt();
	}

	m_stream = nullptr;
}

int c_driver_interface::stream_callback_internal(
	const void *input, void *output, unsigned long frame_count,
	const PaStreamCallbackTimeInfo *time_info, PaStreamCallbackFlags status_flags, void *user_data) {
	c_driver_interface *this_ptr = static_cast<c_driver_interface *>(user_data);

	// $TODO temporary
	memset(output, 0, sizeof(float) * frame_count * this_ptr->m_settings.frames_per_buffer);

	return 0;
}