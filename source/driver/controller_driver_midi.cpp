#include "driver/controller_driver_midi.h"

#include <RtMidi.h>
#pragma comment(lib, "rtmidid.lib")
#ifdef _DEBUG
#else // _DEBUG
#pragma comment(lib, "rtmidi.lib")
#endif // _DEBUG

c_controller_driver_midi::c_controller_driver_midi() {
	m_initialized = false;
	m_midi_in = nullptr;
	m_stream_running = false;
}

c_controller_driver_midi::~c_controller_driver_midi() {
	shutdown();
}

s_controller_driver_result c_controller_driver_midi::initialize() {
	wl_assert(!m_initialized);

	s_controller_driver_result result;
	result.clear();

	m_midi_in = new RtMidiIn;
	m_midi_in->setCallback(message_callback_wrapper, this);
	uint32 port_count = m_midi_in->getPortCount();
	for (uint32 index = 0; index < port_count; index++) {
		m_port_names.push_back(m_midi_in->getPortName(index));
	}

	m_initialized = true;
	return result;
}

void c_controller_driver_midi::shutdown() {
	wl_assert(!m_stream_running);

	if (!m_initialized) {
		return;
	}

	delete m_midi_in;
	m_midi_in = nullptr;
}

uint32 c_controller_driver_midi::get_device_count() const {
	wl_assert(m_initialized);
	return cast_integer_verify<uint32>(m_port_names.size());
}

uint32 c_controller_driver_midi::get_default_device_index() const {
	return 0;
}

s_controller_device_info c_controller_driver_midi::get_device_info(uint32 device_index) const {
	wl_assert(m_initialized);
	wl_assert(VALID_INDEX(device_index, get_device_count()));

	s_controller_device_info result;
	result.name = m_port_names[device_index].c_str();

	return result;
}

bool c_controller_driver_midi::are_settings_supported(const s_controller_driver_settings &settings) const {
	wl_assert(m_initialized);
	return true;
}

s_controller_driver_result c_controller_driver_midi::start_stream(const s_controller_driver_settings &settings) {
	wl_assert(m_initialized);
	wl_assert(!m_stream_running);

	s_controller_driver_result result;
	result.clear();

	try {
		m_midi_in->openPort(settings.device_index);
	} catch (const RtMidiError &error) {
		result.result = k_controller_driver_result_failed_to_open_stream;
		result.message = error.getMessage();
		return result;
	}

	m_settings = settings;
	m_stream_running = true;
	return result;
}

void c_controller_driver_midi::stop_stream() {
	if (!m_stream_running) {
		return;
	}

	m_midi_in->closePort();

	m_stream_running = false;
}

bool c_controller_driver_midi::is_stream_running() const {
	return m_stream_running;
}

const s_controller_driver_settings &c_controller_driver_midi::get_settings() const {
	wl_assert(is_stream_running());
	return m_settings;
}

void c_controller_driver_midi::message_callback_wrapper(
	real64 time_stamp, std::vector<uint8> *message, void *user_data) {
	c_controller_driver_midi *this_ptr = static_cast<c_controller_driver_midi *>(user_data);
	c_wrapped_array_const<uint8> message_array(
		message->empty() ? nullptr : &message->front(),
		message->size());
	this_ptr->message_callback(time_stamp, message_array);
}

void c_controller_driver_midi::message_callback(real64 time_stamp, c_wrapped_array_const<uint8> message) {
	// $TODO temporary! not thread safe
	std::cout << time_stamp << " " << message.get_count() << "\n";
}