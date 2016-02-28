#ifndef WAVELANG_CONTROLLER_DRIVER_MIDI_H__
#define WAVELANG_CONTROLLER_DRIVER_MIDI_H__

#include "common/common.h"
#include "driver/controller_driver.h"
#include <vector>

class RtMidiIn;

class c_controller_driver_midi {
public:
	c_controller_driver_midi();
	~c_controller_driver_midi();

	s_controller_driver_result initialize();
	void shutdown();

	uint32 get_device_count() const;
	uint32 get_default_device_index() const;
	s_controller_device_info get_device_info(uint32 device_index) const;

	bool are_settings_supported(const s_controller_driver_settings &settings) const;

	s_controller_driver_result start_stream(const s_controller_driver_settings &settings);
	void stop_stream();
	bool is_stream_running() const;
	const s_controller_driver_settings &get_settings() const;

private:
	static void message_callback_wrapper(real64 time_stamp, std::vector<uint8> *message, void *user_data);
	void message_callback(real64 time_stamp, c_wrapped_array_const<uint8> message);

	bool m_initialized;
	RtMidiIn *m_midi_in;
	std::vector<std::string> m_port_names;
	bool m_stream_running;
	s_controller_driver_settings m_settings;
};

#endif // WAVELANG_CONTROLLER_DRIVER_MIDI_H__