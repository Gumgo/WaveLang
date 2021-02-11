#include "common/utility/reporting.h"

#include "runtime/driver/audio_driver_interface.h"
#include "runtime/driver/controller_driver_interface.h"
#include "runtime/rapidxml_ext.h"
#include "runtime/runtime_config.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <vector>

static constexpr const char *k_sample_format_xml_strings[] = {
	"float32"
};

STATIC_ASSERT(is_enum_fully_mapped<e_sample_format>(k_sample_format_xml_strings));

static constexpr const char *k_bool_xml_strings[] = {
	"false",
	"true"
};

static constexpr const char *k_default_xml_string = "default";
static constexpr const char *k_none_xml_string = "none";

static constexpr uint32 k_default_audio_input_channel_count = 0;
static constexpr uint32 k_default_audio_output_channel_count = 2;
static constexpr e_sample_format k_default_audio_sample_format = e_sample_format::k_float32;
static constexpr uint32 k_default_audio_frames_per_buffer = 512;

static constexpr bool k_default_controller_enabled = false;
static constexpr uint32 k_default_controller_event_queue_size = 1024;
static constexpr uint32 k_default_controller_unknown_latency = 15;

static constexpr uint32 k_default_executor_thread_count = 0;
static constexpr uint32 k_default_executor_max_controller_parameters = 1024;
static constexpr bool k_default_executor_console_enabled = true;
static constexpr bool k_default_executor_profiling_enabled = false;
static constexpr real32 k_default_executor_profiling_threshold = 0.0f;

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node,
	const char *child_node_name,
	const char *default_value,
	std::string &value_out) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();

		if (strcmp(value, k_default_xml_string) == 0) {
			value_out = default_value;
			return true;
		}

		value_out = value;
		return true;
	}

	return false;
}

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node,
	const char *child_node_name,
	uint32 default_value,
	uint32 &value_out) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();

		if (strcmp(value, k_default_xml_string) == 0) {
			value_out = default_value;
			return true;
		}

		try {
			value_out = std::stoul(value);
			return true;
		} catch (const std::out_of_range &) {
			report_error("Failed to parse '%s'", child_node_name);
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
	uint32 &value_out) {
	if (try_to_get_value_from_child_node(parent_node, child_node_name, default_value, value_out)) {
		if (value_out < range_min || value_out > range_max) {
			report_error("'%s' out of range", child_node_name);
			return false;
		}
	}

	return true;
}

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node,
	const char *child_node_name,
	bool default_value,
	bool &value_out) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();
		if (strcmp(value, k_default_xml_string) == 0) {
			value_out = default_value;
			return true;
		} else if (strcmp(value, k_bool_xml_strings[false]) == 0) {
			value_out = false;
			return true;
		} else if (strcmp(value, k_bool_xml_strings[true]) == 0) {
			value_out = true;
			return true;
		}

		report_error("Failed to parse '%s'", child_node_name);
	}

	return false;
}

static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node,
	const char *child_node_name,
	real32 default_value,
	real32 &value_out) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();

		if (strcmp(value, k_default_xml_string) == 0) {
			value_out = default_value;
			return true;
		}

		try {
			value_out = std::stof(value);
			return true;
		} catch (const std::out_of_range &) {
			report_error("Failed to parse '%s'", child_node_name);
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
	real32 &value_out) {
	if (try_to_get_value_from_child_node(parent_node, child_node_name, default_value, value_out)) {
		if (value_out < range_min || value_out > range_max) {
			report_error("'%s' out of range", child_node_name);
			return false;
		}
	}

	return true;
}

template<typename t_enum>
static bool try_to_get_value_from_child_node(
	const rapidxml::xml_node<> *parent_node,
	const char *child_node_name,
	c_wrapped_array<const char *const> enum_strings,
	t_enum default_value,
	t_enum &value_out) {
	const rapidxml::xml_node<> *node = parent_node->first_node(child_node_name);
	if (node) {
		const char *value = node->value();

		if (strcmp(value, k_default_xml_string) == 0) {
			value_out = default_value;
			return true;
		}

		for (size_t index = 0; index < enum_strings.get_count(); index++) {
			if (strcmp(value, enum_strings[index]) == 0) {
				value_out = static_cast<t_enum>(index);
				return true;
			}
		}

		report_error("Failed to parse '%s'", child_node_name);
	}

	return false;
}

static void append_comment(rapidxml::xml_document<> *document, const char *comment) {
	document->append_node(
		document->allocate_node(rapidxml::node_comment, nullptr, document->allocate_string(comment)));
}

static void append_comment(rapidxml::xml_node<> *node, const char *comment) {
	node->append_node(
		node->document()->allocate_node(rapidxml::node_comment, nullptr, node->document()->allocate_string(comment)));
}

static void append_setting(
	rapidxml::xml_node<> *node,
	const char *name,
	const char *description,
	const char *default_value) {
	append_comment(node, description);
	node->append_node(node->document()->allocate_node(rapidxml::node_element, name, k_default_xml_string));
}

static void append_setting(
	rapidxml::xml_node<> *node,
	const char *name,
	const std::string &description,
	const char *default_value) {
	append_setting(node, name, description.c_str(), default_value);
}

static void try_to_parse_channel_indices(
	const char *name,
	const char *indices_string,
	c_wrapped_array<uint32> indices_out) {
	std::vector<uint32> parsed_channel_indices;
	size_t start_index = 0;
	size_t end_index = 0;
	while (true) {
		char c = indices_string[end_index];
		if (c == ',' || c == '\0') {
			// Try to parse
			try {
				uint32 parsed_channel_index = std::stoul(std::string(indices_string, start_index, end_index));
				parsed_channel_indices.push_back(parsed_channel_index);
			} catch (const std::out_of_range &) {
				report_error("Failed to parse '%s'", name);
				return;
			}

			if (c == '\0') {
				break;
			} else {
				end_index++;
				start_index = end_index;
			}
		} else {
			end_index++;
		}
	}

	if (parsed_channel_indices.size() != indices_out.get_count()) {
		report_error("Incorrect number of channel indices specified for '%s'", name);
		return;
	}

	for (size_t index = 0; index < indices_out.get_count(); index++) {
		indices_out[index] = parsed_channel_indices[index];
	}
}

bool c_runtime_config::write_default_settings(const char *fname) {
	rapidxml::xml_document<> document;

	append_comment(
		&document,
		"Settings for wavelang runtime - specify \"default\" for any setting to use the default value");

	rapidxml::xml_node<> *audio_node = document.allocate_node(rapidxml::node_element, "audio");
	document.append_node(audio_node);
	append_comment(audio_node, "Contains audio settings");

	append_setting(
		audio_node,
		"input_device_index",
		"Index of the input audio device to use - run with --list to see a list of options",
		k_default_xml_string);

	append_setting(
		audio_node,
		"input_channel_count",
		str_format("Number of input channels - default is %u", k_default_audio_input_channel_count),
		k_default_xml_string);

	append_setting(
		audio_node,
		"input_channel_indices",
		"Comma-separated list of input channel indices - default is 0, ..., input_channel_count-1",
		k_default_xml_string);

	append_setting(
		audio_node,
		"output_device_index",
		"Index of the output audio device to use - run with --list to see a list of options",
		k_default_xml_string);

	append_setting(
		audio_node,
		"output_channel_count",
		str_format("Number of output channels - default is %u", k_default_audio_output_channel_count),
		k_default_xml_string);

	append_setting(
		audio_node,
		"output_channel_indices",
		"Comma-separated list of output channel indices - default is 0, ..., output_channel_count-1",
		k_default_xml_string);

	append_setting(
		audio_node,
		"sample_rate",
		"Sample rate of the audio stream - default is the audio device's preferred sample rate",
		k_default_xml_string);

	append_setting(
		audio_node,
		"sample_format",
		str_format(
			"Sample format - default is %s",
			k_sample_format_xml_strings[enum_index(k_default_audio_sample_format)]),
		k_default_xml_string);

	append_setting(
		audio_node,
		"frames_per_buffer",
		str_format("Audio chunk processing size in frames - default is %u", k_default_audio_frames_per_buffer),
		k_default_xml_string);

	rapidxml::xml_node<> *controller_node = document.allocate_node(rapidxml::node_element, "controller");
	document.append_node(controller_node);
	append_comment(controller_node, "Contains controller settings");

	append_comment(
		controller_node,
		"Index of the controller device to use - run with --list to see a list of options, "
		"or use \"none\" to disable controller");

	append_setting(
		controller_node,
		"device_index",
		str_format(
			"To enable multiple controllers, specify <device_index_0> through <device_index_%u>",
			k_max_controller_devices),
		k_default_xml_string);

	append_setting(
		controller_node,
		"event_queue_size",
		str_format(
			"Capacity of the controller event queue - default is %u, "
			"this should only need to be increased when sending a huge amount of controller events",
			k_default_controller_event_queue_size),
		k_default_xml_string);

	append_setting(
		controller_node,
		"unknown_latency",
		str_format(
			"Amount of unknown latency in milliseconds to compensate for things like schedule jitter - default is %u",
			k_default_controller_unknown_latency),
		k_default_xml_string);

	rapidxml::xml_node<> *executor_node = document.allocate_node(rapidxml::node_element, "executor");
	document.append_node(executor_node);
	append_comment(executor_node, "Contains executor settings");

	append_setting(
		executor_node,
		"thread_count",
		str_format(
			"Number of worker threads to use for processing - default is %u, "
			", which causes audio processing to run on the audio callback thread",
			k_default_executor_thread_count),
		k_default_xml_string);

	append_setting(
		executor_node,
		"max_controller_parameters",
		str_format(
			"Maximum number of unique controller parameters which will be tracked - default is %u",
			k_default_executor_max_controller_parameters),
		k_default_xml_string);

	append_setting(
		executor_node,
		"console_enabled",
		str_format(
			"Whether console output for the instrument is enabled - default is %s",
			k_bool_xml_strings[k_default_executor_console_enabled]),
		k_default_xml_string);

	append_setting(
		executor_node,
		"profiling_enabled",
		str_format(
			"Whether profiling should occur when running the instrument - default is %s",
			k_bool_xml_strings[k_default_executor_profiling_enabled]),
		k_default_xml_string);

	append_setting(
		executor_node,
		"profiling_threshold",
		str_format(
			"Threshold for when profiling occurs as a ratio of chunk time - default is %f",
			k_default_executor_profiling_threshold),
		k_default_xml_string);

	std::ofstream file(fname);
	file << document;
	return !file.fail();
}

void c_runtime_config::initialize(
	const c_audio_driver_interface *audio_driver_interface,
	const c_controller_driver_interface *controller_driver_interface) {
	set_default_audio_device(audio_driver_interface);
	set_default_controller_device(controller_driver_interface);

	s_audio_device_info audio_input_device_info =
		audio_driver_interface->get_device_info(m_settings.audio_input_device_index);
	s_audio_device_info audio_output_device_info =
		audio_driver_interface->get_device_info(m_settings.audio_output_device_index);
	set_default_audio(&audio_input_device_info, &audio_output_device_info);

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
				report_error("Failed to open '%s'", fname);
				initialize(audio_driver_interface, controller_driver_interface);
				return e_runtime_config_result::k_does_not_exist;
			}

			std::streampos file_size = file.tellg();
			file.seekg(0);

			file_contents.resize(static_cast<size_t>(file_size) + 1);
			file.read(file_contents.data(), file_size);
			file_contents.back() = '\0';

			if (file.fail()) {
				report_error("Failed to read '%s'", fname);
				initialize(audio_driver_interface, controller_driver_interface);
				return e_runtime_config_result::k_error;
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
			const rapidxml::xml_node<> *audio_input_device_index_node = audio_node->first_node("input_device_index");
			if (audio_input_device_index_node) {
				uint32 index;
				if (try_to_get_value_from_child_node(
					audio_node,
					"input_device_index",
					m_settings.audio_input_device_index, index)
					&& index < audio_driver_interface->get_device_count()) {
					m_settings.audio_input_device_index = index;
				}
			}

			const rapidxml::xml_node<> *audio_output_device_index_node = audio_node->first_node("output_device_index");
			if (audio_output_device_index_node) {
				uint32 index;
				if (try_to_get_value_from_child_node(
					audio_node,
					"output_device_index",
					m_settings.audio_output_device_index, index)
					&& index < audio_driver_interface->get_device_count()) {
					m_settings.audio_output_device_index = index;
				}
			}
		}

		if (controller_node) {
			uint32 default_device_index = (m_settings.controller_device_count == 0)
				? 0
				: m_settings.controller_device_indices[0];

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
							controller_node, node_name.c_str(), default_device_index, index)
							&& index < controller_driver_interface->get_device_count()) {
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
		s_audio_device_info audio_input_device_info =
			audio_driver_interface->get_device_info(m_settings.audio_input_device_index);
		s_audio_device_info audio_output_device_info =
			audio_driver_interface->get_device_info(m_settings.audio_output_device_index);
		set_default_audio(&audio_input_device_info, &audio_output_device_info);

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
				"input_channel_count",
				0u,
				k_max_audio_channels,
				m_settings.audio_input_channel_count,
				m_settings.audio_input_channel_count);

			std::string input_channel_indices;
			if (try_to_get_value_from_child_node(
				audio_node,
				"input_channel_indices",
				"",
				input_channel_indices)
				&& !input_channel_indices.empty()) {
				try_to_parse_channel_indices(
					"input_channel_indices",
					input_channel_indices.c_str(),
					c_wrapped_array<uint32>(
						m_settings.audio_input_channel_indices.get_elements(),
						m_settings.audio_input_channel_count));
			}

			try_to_get_value_from_child_node(
				audio_node,
				"output_channel_count",
				1u,
				k_max_audio_channels,
				m_settings.audio_output_channel_count,
				m_settings.audio_output_channel_count);

			std::string output_channel_indices;
			if (try_to_get_value_from_child_node(
				audio_node,
				"output_channel_indices",
				"",
				output_channel_indices)
				&& !output_channel_indices.empty()) {
				try_to_parse_channel_indices(
					"output_channel_indices",
					output_channel_indices.c_str(),
					c_wrapped_array<uint32>(
						m_settings.audio_output_channel_indices.get_elements(),
						m_settings.audio_output_channel_count));
			}

			try_to_get_value_from_child_node(
				audio_node,
				"sample_rate",
				1u,
				1000000u,
				m_settings.audio_sample_rate,
				m_settings.audio_sample_rate);
			try_to_get_value_from_child_node(
				audio_node,
				"sample_format",
				c_wrapped_array<const char *const>::construct(k_sample_format_xml_strings),
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
		report_error("'%s' is not valid XML", fname);
		initialize(audio_driver_interface, controller_driver_interface);
		return e_runtime_config_result::k_error;
	}

	return e_runtime_config_result::k_success;
}

const c_runtime_config::s_settings &c_runtime_config::get_settings() const {
	return m_settings;
}

void c_runtime_config::set_default_audio_device(const c_audio_driver_interface *audio_driver_interface) {
	m_settings.audio_input_device_index = audio_driver_interface->get_default_input_device_index();
	m_settings.audio_output_device_index = audio_driver_interface->get_default_output_device_index();
}

void c_runtime_config::set_default_audio(
	const s_audio_device_info *audio_input_device_info,
	const s_audio_device_info *audio_output_device_info) {
	m_settings.audio_input_channel_count =
		std::min(k_default_audio_input_channel_count, audio_input_device_info->max_input_channels);
	for (uint32 index = 0; index < m_settings.audio_input_channel_indices.get_count(); index++) {
		m_settings.audio_input_channel_indices[index] = index;
	}

	m_settings.audio_output_channel_count =
		std::min(k_default_audio_output_channel_count, audio_output_device_info->max_output_channels);
	for (uint32 index = 0; index < m_settings.audio_output_channel_indices.get_count(); index++) {
		m_settings.audio_output_channel_indices[index] = index;
	}

	// Grab sample rate default from the output device
	m_settings.audio_sample_rate = static_cast<uint32>(audio_output_device_info->default_sample_rate);
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
	m_settings.executor_console_enabled = k_default_executor_console_enabled;
	m_settings.executor_profiling_enabled = k_default_executor_profiling_enabled;
	m_settings.executor_profiling_threshold = k_default_executor_profiling_threshold;
}
