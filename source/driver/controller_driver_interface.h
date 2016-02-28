#ifndef WAVELANG_CONTROLLER_DRIVER_INTERFACE_H__
#define WAVELANG_CONTROLLER_DRIVER_INTERFACE_H__

#include "common/common.h"
#include "driver/controller_driver.h"
#include "driver/controller_driver_midi.h"

class c_controller_driver_interface {
public:
	c_controller_driver_interface();
	~c_controller_driver_interface();

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
	c_controller_driver_midi m_controller_driver_midi;
	// c_controller_driver_osc m_controller_driver_osc; // $TODO
};

#endif // WAVELANG_CONTROLLER_DRIVER_INTERFACE_H__