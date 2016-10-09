#include "driver/audio_driver_interface.h"
#include "driver/controller_driver_interface.h"

#include "runtime/runtime_config.h"

#include <rapidxml.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

static const char *k_sample_format_strings[] = {
	"float32"
};

static_assert(NUMBEROF(k_sample_format_strings) == k_sample_format_count, "Sample format strings mismatch");

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node, const char *child_node_name, uint32 &out_value) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		try {
			uint32 value = std::stoul(node->value());
			out_value = value;
			return true;
		} catch (const std::out_of_range &) {
			std::cout << "Failed to parse '" << child_node_name << "'\n";
		}
	}

	return false;
}

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node, const char *child_node_name,
	uint32 range_min, uint32 range_max, uint32 &out_value) {
	if (try_to_get_value_from_child_node(parent_node, child_node_name, out_value)) {
		if (out_value < range_min || out_value > range_max) {
			std::cout << "'" << child_node_name << "'out of range\n";
			return false;
		}
	}

	return true;
}

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node, const char *child_node_name, bool &out_value) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();
		if (strcmp(value, "false") == 0) {
			out_value = false;
			return true;
		} else if (strcmp(value, "true") == 0) {
			out_value = true;
			return true;
		}

		std::cout << "Failed to parse '" << child_node_name << "'\n";
	}

	return false;
}

template<typename t_enum>
static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node, const char *child_node_name,
	c_wrapped_array_const<const char *> enum_strings, t_enum &out_value) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();
		for (size_t index = 0; index < enum_strings.get_count(); index++) {
			if (strcmp(value, enum_strings[index]) == 0) {
				out_value = static_cast<t_enum>(index);
				return true;
			}
		}

		std::cout << "Failed to parse '" << child_node_name << "'\n";
	}

	return false;
}

void c_runtime_config::initialize(
	const c_audio_driver_interface *audio_driver_interface,
	const c_controller_driver_interface *controller_driver_interface) {
	set_default_audio_device(audio_driver_interface);
	set_default_controller_device(controller_driver_interface);

	s_audio_device_info audio_device_info =
		audio_driver_interface->get_device_info(m_settings.audio_device_index);
	set_default_audio(&audio_device_info);

	if (m_settings.controller_enabled) {
		s_controller_device_info controller_device_info =
			controller_driver_interface->get_device_info(m_settings.controller_device_index);
		set_default_controller(&controller_device_info);
	}

	set_default_executor();
}

bool c_runtime_config::read_settings(
	const c_audio_driver_interface *audio_driver_interface,
	const c_controller_driver_interface *controller_driver_interface,
	const char *fname) {
	try {
		std::vector<char> file_contents;

		{
			std::ifstream file(fname, std::ios::binary | std::ios::ate);
			if (!file.is_open()) {
				std::cout << "Failed to open '" << fname << "'\n";
				initialize(audio_driver_interface, controller_driver_interface);
				return false;
			}

			std::streampos file_size = file.tellg();
			file.seekg(0);

			file_contents.resize(static_cast<size_t>(file_size) + 1);
			file.read(&file_contents.front(), file_size);
			file_contents.back() = '\0';

			if (file.fail()) {
				std::cout << "Failed to read '" << fname << "'\n";
				initialize(audio_driver_interface, controller_driver_interface);
				return false;
			}
		}

		rapidxml::xml_document<> document;
		document.parse<0>(&file_contents.front());

		// First, set default device index, in case they are not specified
		set_default_audio_device(audio_driver_interface);
		set_default_controller_device(controller_driver_interface);

		const rapidxml::xml_node<> *audio_node = document.first_node("audio");
		const rapidxml::xml_node<> *controller_node = document.first_node("controller");
		const rapidxml::xml_node<> *executor_node = document.first_node("executor");

		// Read device index for audio and controller

		if (audio_node) {
			const rapidxml::xml_node<> *audio_device_index_node = audio_node->first_node("device_index");
			if (audio_device_index_node) {
				const char *audio_device_index_string = audio_device_index_node->value();

				if (strcmp(audio_device_index_string, "default") == 0) {
					// Do nothing
				} else {
					uint32 index;
					if (try_to_get_value_from_child_node(audio_node, "device_index", index) &&
						index < audio_driver_interface->get_device_count()) {
						m_settings.audio_device_index = index;
					}
				}
			}
		}

		if (controller_node) {
			const rapidxml::xml_node<> *controller_device_index_node = controller_node->first_node("device_index");
			if (controller_device_index_node) {
				const char *controller_device_index_string = controller_device_index_node->value();

				if (strcmp(controller_device_index_string, "default") == 0) {
					// Do nothing
				} else if (strcmp(controller_device_index_string, "none") == 0) {
					m_settings.controller_enabled = false;
				} else {
					uint32 index;
					if (try_to_get_value_from_child_node(controller_node , "device_index", index) &&
						index < controller_driver_interface->get_device_count()) {
						m_settings.controller_enabled = true;
						m_settings.controller_device_index = index;
					}
				}
			}
		}

		// Based on device index, apply default settings
		s_audio_device_info audio_device_info =
			audio_driver_interface->get_device_info(m_settings.audio_device_index);
		set_default_audio(&audio_device_info);

		if (m_settings.controller_enabled) {
			s_controller_device_info controller_device_info =
				controller_driver_interface->get_device_info(m_settings.controller_device_index);
			set_default_controller(&controller_device_info);
		}

		set_default_executor();

		// Last, override the defaults
		if (audio_node) {
			try_to_get_value_from_child_node(audio_node, "sample_rate", 1u, 1000000u, m_settings.audio_sample_rate);
			try_to_get_value_from_child_node(audio_node, "input_channels", 0u, 64u, m_settings.audio_input_channels);
			try_to_get_value_from_child_node(audio_node, "output_channels", 1u, 64u, m_settings.audio_output_channels);
			try_to_get_value_from_child_node(
				audio_node, "sample_format", c_wrapped_array_const<const char *>::construct(k_sample_format_strings),
				m_settings.audio_sample_format);
			try_to_get_value_from_child_node(
				audio_node, "frames_per_buffer", 1u, 1000000u, m_settings.audio_frames_per_buffer);
		}

		if (controller_node && m_settings.controller_enabled) {
			try_to_get_value_from_child_node(
				controller_node, "event_queue_size", 1u, 1000000u, m_settings.controller_event_queue_size);
		}

		if (executor_node) {
			try_to_get_value_from_child_node(executor_node, "thread_count", 1u, 16u, m_settings.executor_thread_count);
			try_to_get_value_from_child_node(executor_node, "console_enabled", m_settings.executor_console_enabled);
			try_to_get_value_from_child_node(executor_node, "profiling_enabled", m_settings.executor_profiling_enabled);
		}
	} catch (const rapidxml::parse_error &) {
		std::cout << "'" << fname << "' is not valid XML\n";
		initialize(audio_driver_interface, controller_driver_interface);
		return false;
	}

	return true;
}

const c_runtime_config::s_settings &c_runtime_config::get_settings() const {
	return m_settings;
}

void c_runtime_config::set_default_audio_device(const c_audio_driver_interface *audio_driver_interface) {
	m_settings.audio_device_index = audio_driver_interface->get_default_device_index();
}

void c_runtime_config::set_default_audio(const s_audio_device_info *audio_device_info) {
	m_settings.audio_sample_rate = static_cast<uint32>(audio_device_info->default_sample_rate);
	m_settings.audio_input_channels = 0;
	m_settings.audio_output_channels = std::min(2u, audio_device_info->max_output_channels);
	m_settings.audio_sample_format = k_sample_format_float32;
	m_settings.audio_frames_per_buffer = 512; // $TODO how do I choose this?
}

void c_runtime_config::set_default_controller_device(const c_controller_driver_interface *controller_driver_interface) {
	m_settings.controller_enabled = (controller_driver_interface->get_device_count() > 0);
	if (m_settings.controller_enabled) {
		m_settings.controller_device_index = controller_driver_interface->get_default_device_index();
	} else {
		m_settings.controller_event_queue_size = 1;
	}
}

void c_runtime_config::set_default_controller(const s_controller_device_info *controller_device_info) {
	m_settings.controller_event_queue_size = 1024;
}

void c_runtime_config::set_default_executor() {
	m_settings.executor_thread_count = 1;
	m_settings.executor_console_enabled = true;
	m_settings.executor_profiling_enabled = true;
}