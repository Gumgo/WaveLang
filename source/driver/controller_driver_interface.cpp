#include "driver/controller_driver_interface.h"

c_controller_driver_interface::c_controller_driver_interface() {
}

c_controller_driver_interface::~c_controller_driver_interface() {
}

s_controller_driver_result c_controller_driver_interface::initialize() {
	return m_controller_driver_midi.initialize();
}

void c_controller_driver_interface::shutdown() {
	m_controller_driver_midi.shutdown();
}

uint32 c_controller_driver_interface::get_device_count() const {
	return m_controller_driver_midi.get_device_count();
}

uint32 c_controller_driver_interface::get_default_device_index() const {
	return m_controller_driver_midi.get_default_device_index();
}

s_controller_device_info c_controller_driver_interface::get_device_info(uint32 device_index) const {
	return m_controller_driver_midi.get_device_info(device_index);
}

bool c_controller_driver_interface::are_settings_supported(const s_controller_driver_settings &settings) const {
	return m_controller_driver_midi.are_settings_supported(settings);
}

s_controller_driver_result c_controller_driver_interface::start_stream(const s_controller_driver_settings &settings) {
	return m_controller_driver_midi.start_stream(settings);
}

void c_controller_driver_interface::stop_stream() {
	m_controller_driver_midi.stop_stream();
}

bool c_controller_driver_interface::is_stream_running() const {
	return m_controller_driver_midi.is_stream_running();
}

const s_controller_driver_settings &c_controller_driver_interface::get_settings() const {
	return m_controller_driver_midi.get_settings();
}