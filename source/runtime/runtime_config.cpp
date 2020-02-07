#include "driver/audio_driver_interface.h"
#include "driver/controller_driver_interface.h"

#include "runtime/runtime_config.h"

#include <rapidxml.hpp>
#include <rapidxml_print.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

static const char *k_sample_format_xml_strings[] = {
	"float32"
};

static_assert(NUMBEROF(k_sample_format_xml_strings) == k_sample_format_count, "Sample format strings mismatch");

static const char *k_bool_xml_strings[] = {
	"false",
	"true"
};

static const char *k_default_xml_string = "default";
static const char *k_none_xml_string = "none";

static const uint32 k_default_audio_input_channels = 0;
static const uint32 k_default_audio_output_channels = 2;
static const e_sample_format k_default_audio_sample_format = k_sample_format_float32;
static const uint32 k_default_audio_frames_per_buffer = 512;

static const bool k_default_controller_enabled = false;
static const uint32 k_default_controller_event_queue_size = 1024;
static const uint32 k_default_controller_unknown_latency = 15;

static const uint32 k_default_executor_thread_count = 0;
static const uint32 k_default_executor_max_controller_parameters = 1024;
static const bool k_default_executor_console_enabled = true;
static const bool k_default_executor_profiling_enabled = false;
static const real32 k_default_executor_profiling_threshold = 0.0f;

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node, const char *child_node_name, uint32 default_value, uint32 &out_value) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();

		if (strcmp(value, k_default_xml_string) == 0) {
			out_value = default_value;
			return true;
		}

		try {
			out_value = std::stoul(value);
			return true;
		} catch (const std::out_of_range &) {
			std::cout << "Failed to parse '" << child_node_name << "'\n";
		}
	}

	return false;
}

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node,
	const char *child_node_name,
	uint32 range_min,
	uint32 range_max,
	uint32 default_value,
	uint32 &out_value) {
	if (try_to_get_value_from_child_node(parent_node, child_node_name, default_value, out_value)) {
		if (out_value < range_min || out_value > range_max) {
			std::cout << "'" << child_node_name << "'out of range\n";
			return false;
		}
	}

	return true;
}

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node, const char *child_node_name, bool default_value, bool &out_value) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();
		if (strcmp(value, k_default_xml_string) == 0) {
			out_value = default_value;
			return true;
		} else if (strcmp(value, k_bool_xml_strings[false]) == 0) {
			out_value = false;
			return true;
		} else if (strcmp(value, k_bool_xml_strings[true]) == 0) {
			out_value = true;
			return true;
		}

		std::cout << "Failed to parse '" << child_node_name << "'\n";
	}

	return false;
}

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node, const char *child_node_name, real32 default_value, real32 &out_value) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();

		if (strcmp(value, k_default_xml_string) == 0) {
			out_value = default_value;
			return true;
		}

		try {
			out_value = std::stof(value);
			return true;
		} catch (const std::out_of_range &) {
			std::cout << "Failed to parse '" << child_node_name << "'\n";
		}
	}

	return false;
}

static real32 try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node,
	const char *child_node_name,
	real32 range_min,
	real32 range_max,
	real32 default_value,
	real32 &out_value) {
	if (try_to_get_value_from_child_node(parent_node, child_node_name, default_value, out_value)) {
		if (out_value < range_min || out_value > range_max) {
			std::cout << "'" << child_node_name << "'out of range\n";
			return false;
		}
	}

	return true;
}

template<typename t_enum>
static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node,
	const char *child_node_name,
	c_wrapped_array_const<const char *> enum_strings,
	t_enum default_value,
	t_enum &out_value) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();

		if (strcmp(value, k_default_xml_string) == 0) {
			out_value = default_value;
			return true;
		}

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

bool c_runtime_config::write_default_settings(const char *fname) {
	rapidxml::xml_document<> document;

	document.append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		"Settings for wavelang runtime - specify \"default\" for any setting to use the default value"));

	rapidxml::xml_node<> *audio_node = document.allocate_node(rapidxml::node_element, "audio");
	document.append_node(audio_node);
	audio_node->append_node(document.allocate_node(rapidxml::node_comment, nullptr, "Contains audio settings"));

	audio_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		"Index of the audio device to use - run with --list to see a list of options"));
	audio_node->append_node(document.allocate_node(rapidxml::node_element, "device_index", k_default_xml_string));

	audio_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		"Sample rate of the audio stream - default is the audio device's preferred sample rate"));
	audio_node->append_node(document.allocate_node(rapidxml::node_element, "sample_rate", k_default_xml_string));

	audio_node->append_node(document.allocate_node(rapidxml::node_comment, nullptr, document.allocate_string(
		("Number of input channels - default is " + std::to_string(k_default_audio_input_channels)).c_str())));
	audio_node->append_node(document.allocate_node(rapidxml::node_element, "input_channels", k_default_xml_string));

	audio_node->append_node(document.allocate_node(rapidxml::node_comment, nullptr, document.allocate_string(
		("Number of output channels - default is " + std::to_string(k_default_audio_output_channels)).c_str())));
	audio_node->append_node(document.allocate_node(rapidxml::node_element, "output_channels", k_default_xml_string));

	audio_node->append_node(document.allocate_node(rapidxml::node_comment, nullptr, document.allocate_string(
		("Sample format - default is " +
			std::string(k_sample_format_xml_strings[k_default_audio_sample_format])).c_str())));
	audio_node->append_node(document.allocate_node(rapidxml::node_element, "sample_format", k_default_xml_string));

	audio_node->append_node(document.allocate_node(rapidxml::node_comment, nullptr, document.allocate_string(
		("Audio chunk processing size in frames - default is " +
			std::to_string(k_default_audio_frames_per_buffer)).c_str())));
	audio_node->append_node(document.allocate_node(rapidxml::node_element, "frames_per_buffer", k_default_xml_string));

	rapidxml::xml_node<> *controller_node = document.allocate_node(rapidxml::node_element, "controller");
	document.append_node(controller_node);
	controller_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		"Contains controller settings"));

	controller_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		"Index of the controller device to use - run with --list to see a list of options, "
		"or use \"none\" to disable controller"));
	controller_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		document.allocate_string(
			("To enable multiple controllers, specify <device_index_0> through <device_index_" +
				std::to_string(k_max_controller_devices) + ">").c_str())));
	controller_node->append_node(document.allocate_node(
		rapidxml::node_element,
		"device_index",
		k_default_xml_string));

	controller_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		document.allocate_string(
			("Capacity of the controller event queue - default is " +
				std::to_string(k_default_controller_event_queue_size) +
				", this should only need to be increased when sending a huge amount of controller events").c_str())));
	controller_node->append_node(document.allocate_node(
		rapidxml::node_element,
		"event_queue_size",
		k_default_xml_string));

	controller_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		document.allocate_string(
			("Amount of unknown latency in milliseconds to compensate for things like schedule jitter - default is " +
				std::to_string(k_default_controller_unknown_latency)).c_str())));
	controller_node->append_node(document.allocate_node(
		rapidxml::node_element,
		"unknown_latency",
		k_default_xml_string));

	rapidxml::xml_node<> *executor_node = document.allocate_node(rapidxml::node_element, "executor");
	document.append_node(executor_node);
	executor_node->append_node(document.allocate_node(rapidxml::node_comment, nullptr, "Contains executor settings"));

	executor_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		document.allocate_string(
			("Number of worker threads to use for processing - default is " +
				std::to_string(k_default_executor_thread_count) +
				", which causes audio processing to run on the audio callback thread").c_str())));
	executor_node->append_node(document.allocate_node(rapidxml::node_element, "thread_count", k_default_xml_string));

	executor_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		document.allocate_string(
			("Maximum number of unique controller parameters which will be tracked - default is " +
				std::to_string(k_default_executor_max_controller_parameters)).c_str())));
	executor_node->append_node(document.allocate_node(
		rapidxml::node_element,
		"max_controller_parameters",
		k_default_xml_string));

	executor_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		document.allocate_string(
			("Whether console output for the instrument is enabled - default is " +
				std::string(k_bool_xml_strings[k_default_executor_console_enabled])).c_str())));
	executor_node->append_node(document.allocate_node(rapidxml::node_element, "console_enabled", k_default_xml_string));

	executor_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		document.allocate_string(
			("Whether profiling should occur when running the instrument - default is " +
				std::string(k_bool_xml_strings[k_default_executor_profiling_enabled])).c_str())));
	executor_node->append_node(document.allocate_node(
		rapidxml::node_element,
		"profiling_enabled",
		k_default_xml_string));

	executor_node->append_node(document.allocate_node(
		rapidxml::node_comment,
		nullptr,
		document.allocate_string(
			("Threshold for when profiling occurs as a ratio of chunk time - default is " +
				std::to_string(k_default_executor_profiling_threshold)).c_str())));
	executor_node->append_node(document.allocate_node(
		rapidxml::node_element,
		"profiling_threshold",
		k_default_xml_string));

	std::ofstream file(fname);
	file << document;
	return !file.fail();
}

void c_runtime_config::initialize(
	const c_audio_driver_interface *audio_driver_interface,
	const c_controller_driver_interface *controller_driver_interface) {
	set_default_audio_device(audio_driver_interface);
	set_default_controller_device(controller_driver_interface);

	s_audio_device_info audio_device_info =
		audio_driver_interface->get_device_info(m_settings.audio_device_index);
	set_default_audio(&audio_device_info);

	if (m_settings.controller_device_count > 0) {
		s_controller_device_info controller_device_info =
			controller_driver_interface->get_device_info(m_settings.controller_device_indices[0]);
		set_default_controller(&controller_device_info);
	}

	set_default_executor();
}

e_runtime_config_result c_runtime_config::read_settings(
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
				return k_runtime_config_result_does_not_exist;
			}

			std::streampos file_size = file.tellg();
			file.seekg(0);

			file_contents.resize(static_cast<size_t>(file_size) + 1);
			file.read(&file_contents.front(), file_size);
			file_contents.back() = '\0';

			if (file.fail()) {
				std::cout << "Failed to read '" << fname << "'\n";
				initialize(audio_driver_interface, controller_driver_interface);
				return k_runtime_config_result_error;
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
				uint32 index;
				if (try_to_get_value_from_child_node(
					audio_node, "device_index", m_settings.audio_device_index, index) &&
					index < audio_driver_interface->get_device_count()) {
					m_settings.audio_device_index = index;
				}
			}
		}

		if (controller_node) {
			uint32 default_device_index = (m_settings.controller_device_count == 0) ?
				0 :
				m_settings.controller_device_indices[0];

			for (uint32 device = 0; device < k_max_controller_devices; device++) {
				std::string node_name = "device_index_" + std::to_string(device);
				const rapidxml::xml_node<> *controller_device_index_node =
					controller_node->first_node(node_name.c_str());

				if (device == 0 && !controller_device_index_node) {
					// Device 0 can also be specified with just "device_index"
					node_name = "device_index";
					controller_device_index_node = controller_node->first_node(node_name.c_str());
				}

				if (controller_device_index_node) {
					const char *controller_device_index_string = controller_device_index_node->value();

					if (strcmp(controller_device_index_string, k_none_xml_string) == 0) {
						// Do nothing - this device isn't enabled
					} else {
						uint32 index;
						if (try_to_get_value_from_child_node(
							controller_node , node_name.c_str(), default_device_index, index) &&
							index < controller_driver_interface->get_device_count()) {
							// Determine if this device is already in use
							bool in_use = false;
							for (size_t used_device = 0;
								 !in_use && used_device < m_settings.controller_device_count;
								 used_device++) {
								in_use = (m_settings.controller_device_indices[used_device] == index);
							}

							if (!in_use) {
								m_settings.controller_device_indices[m_settings.controller_device_count] = index;
								m_settings.controller_device_count++;
							}
						}
					}
				}
			}
		}

		// Based on device index, apply default settings
		s_audio_device_info audio_device_info =
			audio_driver_interface->get_device_info(m_settings.audio_device_index);
		set_default_audio(&audio_device_info);

		if (m_settings.controller_device_count > 0) {
			s_controller_device_info controller_device_info =
				controller_driver_interface->get_device_info(m_settings.controller_device_indices[0]);
			set_default_controller(&controller_device_info);
		}

		set_default_executor();

		// Last, override the defaults. Since we've already set everything to default, just pass the existing values in
		// as the default values.
		if (audio_node) {
			try_to_get_value_from_child_node(
				audio_node,
				"sample_rate",
				1u,
				1000000u,
				m_settings.audio_sample_rate,
				m_settings.audio_sample_rate);
			try_to_get_value_from_child_node(
				audio_node,
				"input_channels",
				0u,
				64u,
				m_settings.audio_input_channels,
				m_settings.audio_input_channels);
			try_to_get_value_from_child_node(
				audio_node,
				"output_channels",
				1u,
				64u,
				m_settings.audio_output_channels,
				m_settings.audio_output_channels);
			try_to_get_value_from_child_node(
				audio_node,
				"sample_format",
				c_wrapped_array_const<const char *>::construct(k_sample_format_xml_strings),
				m_settings.audio_sample_format,
				m_settings.audio_sample_format);
			try_to_get_value_from_child_node(
				audio_node,
				"frames_per_buffer",
				1u,
				1000000u,
				m_settings.audio_frames_per_buffer,
				m_settings.audio_frames_per_buffer);
		}

		if (controller_node && m_settings.controller_device_count > 0) {
			try_to_get_value_from_child_node(
				controller_node,
				"event_queue_size",
				1u,
				1000000u,
				m_settings.controller_event_queue_size,
				m_settings.controller_event_queue_size);
			try_to_get_value_from_child_node(
				controller_node,
				"unknown_latency",
				0u,
				1000u,
				m_settings.controller_unknown_latency,
				m_settings.controller_unknown_latency);
		}

		if (executor_node) {
			try_to_get_value_from_child_node(
				executor_node,
				"thread_count",
				0u,
				16u,
				m_settings.executor_thread_count,
				m_settings.executor_thread_count);
			try_to_get_value_from_child_node(
				executor_node,
				"max_controller_parameters",
				1u,
				65535u,
				m_settings.executor_max_controller_parameters,
				m_settings.executor_max_controller_parameters);
			try_to_get_value_from_child_node(
				executor_node,
				"console_enabled",
				m_settings.executor_console_enabled,
				m_settings.executor_console_enabled);
			try_to_get_value_from_child_node(
				executor_node,
				"profiling_enabled",
				m_settings.executor_profiling_enabled,
				m_settings.executor_profiling_enabled);
			try_to_get_value_from_child_node(
				executor_node,
				"profiling_threshold",
				0.0f, 1.0f,
				m_settings.executor_profiling_threshold,
				m_settings.executor_profiling_threshold);
		}
	} catch (const rapidxml::parse_error &) {
		std::cout << "'" << fname << "' is not valid XML\n";
		initialize(audio_driver_interface, controller_driver_interface);
		return k_runtime_config_result_error;
	}

	return k_runtime_config_result_success;
}

const c_runtime_config::s_settings &c_runtime_config::get_settings() const {
	return m_settings;
}

void c_runtime_config::set_default_audio_device(const c_audio_driver_interface *audio_driver_interface) {
	m_settings.audio_device_index = audio_driver_interface->get_default_device_index();
}

void c_runtime_config::set_default_audio(const s_audio_device_info *audio_device_info) {
	m_settings.audio_sample_rate = static_cast<uint32>(audio_device_info->default_sample_rate);
	m_settings.audio_input_channels =
		k_default_audio_input_channels;
		//std::min(k_default_audio_input_channels, audio_device_info->max_input_channels); // $TODO $INPUT
	m_settings.audio_output_channels =
		std::min(k_default_audio_output_channels, audio_device_info->max_output_channels);
	m_settings.audio_sample_format = k_default_audio_sample_format;
	m_settings.audio_frames_per_buffer = k_default_audio_frames_per_buffer;
}

void c_runtime_config::set_default_controller_device(const c_controller_driver_interface *controller_driver_interface) {
	m_settings.controller_device_count = (controller_driver_interface->get_device_count() == 0) ? 0 : 1;
	if (m_settings.controller_device_count > 0) {
		m_settings.controller_device_indices[0] = controller_driver_interface->get_default_device_index();
		m_settings.controller_unknown_latency = k_default_controller_unknown_latency;
	} else {
		m_settings.controller_event_queue_size = 1;
	}
}

void c_runtime_config::set_default_controller(const s_controller_device_info *controller_device_info) {
	m_settings.controller_event_queue_size = k_default_controller_event_queue_size;
}

void c_runtime_config::set_default_executor() {
	m_settings.executor_thread_count = k_default_executor_thread_count;
	m_settings.executor_max_controller_parameters = k_default_executor_max_controller_parameters;
	m_settings.executor_console_enabled = k_default_controller_enabled;
	m_settings.executor_profiling_enabled = k_default_executor_profiling_enabled;
	m_settings.executor_profiling_threshold = k_default_executor_profiling_threshold;
}
