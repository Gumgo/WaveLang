#include "common/common.h"

#include "engine/events/event_data_types.h"
#include "engine/task_function_registry.h"
#include "engine/task_functions/task_function_registration.h"

#include "execution_graph/execution_graph.h"
#include "execution_graph/instrument.h"
#include "execution_graph/native_module_registry.h"
#include "execution_graph/native_modules/native_module_registration.h"

#include "runtime/runtime_config.h"
#include "runtime/runtime_context.h"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

static const char *k_runtime_config_filename = "wavelang_runtime_config.xml";

class c_command_line_interface {
public:
	c_command_line_interface(int argc, char **argv);
	~c_command_line_interface();

	int main_function(bool list_mode);

private:
	struct s_command {
		std::string command;
		std::vector<std::string> arguments;
	};

	static s_command process_command();
	static size_t skip_whitespace(const std::string &str, size_t pos);
	static size_t skip_to_whitespace(const std::string &str, size_t pos);
	static size_t skip_to_quote(const std::string &str, size_t pos, std::string &out_result);
	static bool is_whitespace(char c);

	void list_devices();
	void initialize_from_runtime_config();
	void process_command_load_synth(const s_command &command);

	c_runtime_config m_runtime_config;
	s_runtime_context m_runtime_context;
};

int main(int argc, char **argv) {
	int result;
	{
		c_command_line_interface cli(argc, argv);
		// $TODO add real command line argument parsing
		bool list_mode = (argc == 2 && strcmp(argv[1], "-list") == 0);
		result = cli.main_function(list_mode);
	}
	return result;
}

c_command_line_interface::c_command_line_interface(int argc, char **argv) {
}

c_command_line_interface::~c_command_line_interface() {

}

int c_command_line_interface::main_function(bool list_mode) {
	c_native_module_registry::initialize();
	c_task_function_registry::initialize();

	register_native_modules(false);
	register_task_functions();

	{
		s_audio_driver_result driver_result = m_runtime_context.audio_driver_interface.initialize();
		if (driver_result.result != k_audio_driver_result_success) {
			std::cout << driver_result.message << "\n";
			return 1;
		}
	}

	{
		s_controller_driver_result driver_result = m_runtime_context.controller_driver_interface.initialize();
		if (driver_result.result != k_controller_driver_result_success) {
			std::cout << driver_result.message << "\n";
			return 1;
		}
	}

	if (list_mode) {
		list_devices();
	} else {
		if (!m_runtime_config.read_settings(
			&m_runtime_context.audio_driver_interface,
			&m_runtime_context.controller_driver_interface,
			k_runtime_config_filename)) {
			c_runtime_config::write_default_settings(k_runtime_config_filename);
		}

		initialize_event_data_types();
		initialize_from_runtime_config();

		// None active initially
		m_runtime_context.active_instrument = -1;

		bool done = false;
		while (!done) {
			s_command command = process_command();

			if (command.command.empty()) {
				std::cout << "Invalid command\n";
			} else if (command.command == "exit") {
				done = true;
			} else if (command.command == "load_synth") {
				process_command_load_synth(command);
			} else {
				std::cout << "Invalid command\n";
			}
		}
	}

	m_runtime_context.executor.shutdown();
	m_runtime_context.controller_driver_interface.stop_stream();
	m_runtime_context.controller_driver_interface.shutdown();
	m_runtime_context.audio_driver_interface.stop_stream();
	m_runtime_context.audio_driver_interface.shutdown();

	c_task_function_registry::shutdown();
	c_native_module_registry::shutdown();

	return 0;
}

c_command_line_interface::s_command c_command_line_interface::process_command() {
	s_command command;

	std::string command_string;
	std::getline(std::cin, command_string);

	size_t command_start = skip_whitespace(command_string, 0);
	size_t command_end = skip_to_whitespace(command_string, command_start);
	command.command = command_string.substr(command_start, command_end - command_start);

	size_t arg_start = skip_whitespace(command_string, command_end);
	while (arg_start < command_string.length()) {
		size_t arg_end;
		if (command_string[arg_start] == '"') {
			command.arguments.push_back(std::string());
			arg_end = skip_to_quote(command_string, arg_start, command.arguments.back());
			if (arg_end == std::string::npos) {
				// Invalid quoted argument - return error result
				command.command.clear();
				command.arguments.clear();
				break;
			}
		} else {
			arg_end = skip_to_whitespace(command_string, arg_start);
			command.arguments.push_back(command_string.substr(arg_start, arg_end - arg_start));
		}

		arg_start = skip_whitespace(command_string, arg_end);
	}

	return command;
}

size_t c_command_line_interface::skip_whitespace(const std::string &str, size_t pos) {
	for (size_t index = pos; index < str.length(); index++) {
		if (!is_whitespace(str[index])) {
			return index;
		}
	}

	return str.length();
}

size_t c_command_line_interface::skip_to_whitespace(const std::string &str, size_t pos) {
	for (size_t index = pos; index < str.length(); index++) {
		if (is_whitespace(str[index])) {
			return index;
		}
	}

	return str.length();
}

size_t c_command_line_interface::skip_to_quote(const std::string &str, size_t pos, std::string &out_result) {
	wl_assert(str[pos] == '"');

	bool prev_was_escape_character = false;
	for (size_t index = pos + 1; index < str.length(); index++) {
		if (prev_was_escape_character) {
			if (str[index] == '"' || str[index] == '\\') {
				// Escaped character
				out_result.push_back(str[index]);
			} else {
				// Error - unknown escape sequence
				break;
			}

			// Reset escape character processing
			prev_was_escape_character = false;
		} else {
			if (str[index] == '"') {
				// Terminate the string
				return index + 1;
			} else if (str[index] == '\\') {
				// Next character gets escaped
				prev_was_escape_character = true;
			} else {
				// Append to string
				out_result.push_back(str[index]);
			}
		}
	}

	// No terminating quote found
	return std::string::npos;
}

bool c_command_line_interface::is_whitespace(char c) {
	return (c == ' ' || c == '\t' || c == '\n');
}

void c_command_line_interface::list_devices() {
	std::cout << "Audio devices:\n";
	for (uint32 index = 0; index < m_runtime_context.audio_driver_interface.get_device_count(); index++) {
		s_audio_device_info device_info = m_runtime_context.audio_driver_interface.get_device_info(index);
		std::cout << index << ": " << device_info.name << "\n";
	}

	std::cout << "Controller devices:\n";
	for (uint32 index = 0; index < m_runtime_context.controller_driver_interface.get_device_count(); index++) {
		s_controller_device_info device_info = m_runtime_context.controller_driver_interface.get_device_info(index);
		std::cout << index << ": " << device_info.name << "\n";
	}
}

void c_command_line_interface::initialize_from_runtime_config() {
	const c_runtime_config::s_settings &config_settings = m_runtime_config.get_settings();

	// Initialize audio stream
	{
		s_audio_driver_settings settings;
		settings.set_default();
		settings.device_index = config_settings.audio_device_index;
		settings.sample_rate = config_settings.audio_sample_rate;
		//settings.input_channels = config_settings.audio_input_channels; // $TODO $INPUT
		settings.output_channels = config_settings.audio_output_channels;
		settings.sample_format = config_settings.audio_sample_format;
		settings.frames_per_buffer = config_settings.audio_frames_per_buffer;

		settings.stream_callback = m_runtime_context.stream_callback;
		settings.stream_callback_user_data = &m_runtime_context;

		s_audio_driver_result result = m_runtime_context.audio_driver_interface.start_stream(settings);
		if (result.result != k_audio_driver_result_success) {
			std::cout << result.message << "\n";
		}
	}

	// Initialize the controller
	if (config_settings.controller_enabled) {
		s_controller_driver_settings settings;
		settings.set_default();
		settings.device_index = config_settings.controller_device_index;
		settings.controller_event_queue_size = config_settings.controller_event_queue_size;
		settings.unknown_latency = static_cast<real64>(config_settings.controller_unknown_latency) * 0.001;

		if (m_runtime_context.audio_driver_interface.is_stream_running()) {
			m_runtime_context.audio_driver_interface.get_stream_clock(settings.clock, settings.clock_context);
		}

		s_controller_driver_result result = m_runtime_context.controller_driver_interface.start_stream(settings);
		if (result.result != k_controller_driver_result_success) {
			std::cout << result.message << "\n";
		}
	}
}

void c_command_line_interface::process_command_load_synth(const s_command &command) {
	if (command.arguments.size() == 1) {
		if (!m_runtime_context.audio_driver_interface.is_stream_running()) {
			std::cout << "Audio stream is not running\n";
			return;
		}

		// Try to load the instrument
		c_instrument instrument;

		// First argument is the path to load
		e_instrument_result load_result = instrument.load(command.arguments[0].c_str());
		if (load_result != k_instrument_result_success) {
			std::cout << "Failed to load '" << command.arguments[0] << "' (result code " << load_result << ")\n";
			return;
		}

		// Select the execution graph from the instrument
		uint32 instrument_variant_index;
		{
			s_instrument_variant_requirements requirements;
			requirements.sample_rate = static_cast<uint32>(
				m_runtime_context.audio_driver_interface.get_settings().sample_rate);

			e_instrument_variant_for_requirements_result instrument_variant_result =
				instrument.get_instrument_variant_for_requirements(requirements, instrument_variant_index);

			if (instrument_variant_result == k_instrument_variant_for_requirements_result_no_match) {
				std::cout << "Failed to find instrument variant matching the runtime requirements\n";
				return;
			} else if (instrument_variant_index == k_instrument_variant_for_requirements_result_ambiguous_matches) {
				std::cout << "Found multiple instrument variants matching the runtime requirements - "
					"refine stream parameters or instrument globals\n";
				return;
			} else {
				wl_assert(instrument_variant_result == k_instrument_variant_for_requirements_result_success);
			}
		}

		// Load into the inactive instrument
		int32 loading_instrument;
		if (m_runtime_context.active_instrument == -1) {
			loading_instrument = 0;
		} else {
			loading_instrument = (m_runtime_context.active_instrument == 0) ? 1 : 0;
		}

		c_runtime_instrument &runtime_instrument = m_runtime_context.runtime_instruments[loading_instrument];
		if (!runtime_instrument.build(instrument.get_instrument_variant(instrument_variant_index))) {
			std::cout << "Failed to build runtime instrument\n";
			return;
		}

		if (m_runtime_context.audio_driver_interface.is_stream_running()) {
			// Set up the executor with the new graph
			m_runtime_context.executor.shutdown();

			const c_runtime_config::s_settings &runtime_config_settings = m_runtime_config.get_settings();
			s_executor_settings settings;
			settings.runtime_instrument = &runtime_instrument;
			settings.thread_count = runtime_config_settings.executor_thread_count;
			settings.sample_rate = runtime_config_settings.audio_sample_rate;
			settings.max_buffer_size = runtime_config_settings.audio_frames_per_buffer;
			settings.output_channels = runtime_config_settings.audio_output_channels;
			settings.controller_event_queue_size = runtime_config_settings.controller_event_queue_size;
			settings.max_controller_parameters = runtime_config_settings.executor_max_controller_parameters;
			settings.process_controller_events = s_runtime_context::process_controller_events_callback;
			settings.process_controller_events_context = &m_runtime_context;
			settings.event_console_enabled = runtime_config_settings.executor_console_enabled;
			settings.profiling_enabled = runtime_config_settings.executor_profiling_enabled;
			m_runtime_context.executor.initialize(settings);
		}

		m_runtime_context.active_instrument = loading_instrument;
		return;
	}

	std::cout << "Invalid command\n";
}
