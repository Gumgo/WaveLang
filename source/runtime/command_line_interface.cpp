#include "common/common.h"
#include "common/math/floating_point.h"
#include "common/threading/mutex.h"
#include "common/threading/semaphore.h"
#include "common/threading/thread.h"
#include "common/utility/memory_leak_detection.h"

#include "engine/events/event_data_types.h"
#include "engine/task_function_registration.h"
#include "engine/task_function_registry.h"
#include "engine/task_functions/scrape_task_functions.h"

#include "instrument/instrument.h"
#include "instrument/native_module_graph.h"
#include "instrument/native_module_registration.h"
#include "instrument/native_module_registry.h"
#include "instrument/native_modules/scrape_native_modules.h"

#include "runtime/runtime_config.h"
#include "runtime/runtime_context.h"

#include <algorithm>
#include <iostream>
#include <queue>
#include <string>
#include <vector>

static constexpr const char *k_runtime_config_filename = "wavelang_runtime_config.xml";

class c_command_line_interface {
public:
	c_command_line_interface(int argc, char **argv);
	~c_command_line_interface();

	int main_function();

private:
	struct s_command {
		std::string command;
		std::vector<std::string> arguments;
	};

	static s_command read_command();
	static size_t skip_whitespace(const std::string &str, size_t pos);
	static size_t skip_to_whitespace(const std::string &str, size_t pos);
	static size_t skip_to_quote(const std::string &str, size_t pos, std::string &result_out);
	static bool is_whitespace(char c);

	bool run_command(const s_command &command);

	void list_devices();
	void initialize_from_runtime_config();
	void process_command_load_synth(const s_command &command);

	static bool controller_hook_wrapper(void *context, const s_controller_event &controller_event);
	bool controller_hook(const s_controller_event &controller_event);

	static void controller_command_thread_entry_point(const s_thread_parameter_block *params);
	void controller_command_thread_main();

	std::vector<void *> m_task_function_library_contexts;

	c_runtime_config m_runtime_config;
	s_runtime_context m_runtime_context;
	c_mutex m_command_lock;

	c_thread m_controller_command_thread;
	c_mutex m_controller_command_lock;
	c_semaphore m_controller_command_semaphore;
	std::queue<s_command> m_controller_commands;

	// Command-line options:
	bool m_list_enabled;			// Whether to list all native modules and immediately exit
	std::string m_startup_synth;	// If non-empty, immediately loads a synth
};

int main(int argc, char **argv) {
	initialize_memory_leak_detection();
	initialize_floating_point_behavior();

	int result;
	{
		c_command_line_interface cli(argc, argv);
		result = cli.main_function();
	}
	return result;
}

c_command_line_interface::c_command_line_interface(int argc, char **argv) {
	m_list_enabled = false;

	// $TODO add real command line argument parsing
	if (argc == 2) {
		if (strcmp(argv[1], "--list") == 0) {
			m_list_enabled = true;
		} else {
			m_startup_synth = argv[1];
		}
	}
}

c_command_line_interface::~c_command_line_interface() {

}

int c_command_line_interface::main_function() {
	scrape_native_modules();
	scrape_task_functions();

	c_native_module_registry::initialize();
	c_task_function_registry::initialize();

	if (!register_native_modules()) {
		return 1;
	}

	if (!register_task_functions()) {
		return 1;
	}

	m_task_function_library_contexts.resize(c_task_function_registry::get_task_function_library_count(), nullptr);
	for (h_task_function_library library_handle : c_task_function_registry::iterate_task_function_libraries()) {
		const s_task_function_library &library = c_task_function_registry::get_task_function_library(library_handle);
		if (library.engine_initializer) {
			m_task_function_library_contexts[library_handle.get_data()] = library.engine_initializer();
		}
	}

	{
		s_audio_driver_result driver_result = m_runtime_context.audio_driver_interface.initialize();
		if (driver_result.result != e_audio_driver_result::k_success) {
			std::cout << driver_result.message << "\n";
			return 1;
		}
	}

	{
		s_controller_driver_result driver_result = m_runtime_context.controller_driver_interface.initialize();
		if (driver_result.result != e_controller_driver_result::k_success) {
			std::cout << driver_result.message << "\n";
			return 1;
		}
	}

	if (m_list_enabled) {
		list_devices();
	} else {
		e_runtime_config_result runtime_config_result = m_runtime_config.read_settings(
			&m_runtime_context.audio_driver_interface,
			&m_runtime_context.controller_driver_interface,
			k_runtime_config_filename);

		if (runtime_config_result == e_runtime_config_result::k_does_not_exist) {
			std::cout << "Creating default runtime config file\n";
			c_runtime_config::write_default_settings(k_runtime_config_filename);
		} else if (runtime_config_result == e_runtime_config_result::k_error) {
			std::cout << "Runtime config was found but had errors\n";
		} else {
			wl_assert(runtime_config_result == e_runtime_config_result::k_success);
		}

		initialize_event_data_types();
		initialize_from_runtime_config();

		// None active initially
		m_runtime_context.active_instrument = -1;

		if (!m_startup_synth.empty()) {
			s_command command;
			command.command = "load_synth";
			command.arguments.push_back(m_startup_synth);
			process_command_load_synth(command);
		}

		s_thread_definition controller_command_thread_definition;
		zero_type(&controller_command_thread_definition);
		controller_command_thread_definition.thread_name = "controller command thread";
		controller_command_thread_definition.thread_priority = e_thread_priority::k_normal;
		controller_command_thread_definition.processor = -1;
		controller_command_thread_definition.thread_entry_point = controller_command_thread_entry_point;
		*controller_command_thread_definition.parameter_block.get_memory_typed<c_command_line_interface *>() = this;
		m_controller_command_thread.start(controller_command_thread_definition);

		bool done = false;
		while (!done) {
			s_command command = read_command();
			done = run_command(command);
		}

		{
			c_scoped_lock lock(m_controller_command_lock);
			s_command exit_command;
			exit_command.command = "exit";
			m_controller_commands.push(exit_command);
		}

		m_controller_command_semaphore.notify();
		m_controller_command_thread.join();
	}

	m_runtime_context.executor.shutdown();
	m_runtime_context.controller_driver_interface.stop_stream();
	m_runtime_context.controller_driver_interface.shutdown();
	m_runtime_context.audio_driver_interface.stop_stream();
	m_runtime_context.audio_driver_interface.shutdown();

	for (h_task_function_library library_handle : c_task_function_registry::iterate_task_function_libraries()) {
		const s_task_function_library &library = c_task_function_registry::get_task_function_library(library_handle);
		if (library.engine_deinitializer) {
			library.engine_deinitializer(m_task_function_library_contexts[library_handle.get_data()]);
		}
	}

	c_task_function_registry::shutdown();
	c_native_module_registry::shutdown();

	return 0;
}

c_command_line_interface::s_command c_command_line_interface::read_command() {
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

size_t c_command_line_interface::skip_to_quote(const std::string &str, size_t pos, std::string &result_out) {
	wl_assert(str[pos] == '"');

	bool prev_was_escape_character = false;
	for (size_t index = pos + 1; index < str.length(); index++) {
		if (prev_was_escape_character) {
			if (str[index] == '"' || str[index] == '\\') {
				// Escaped character
				result_out.push_back(str[index]);
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
				result_out.push_back(str[index]);
			}
		}
	}

	// No terminating quote found
	return std::string::npos;
}

bool c_command_line_interface::is_whitespace(char c) {
	return (c == ' ' || c == '\t' || c == '\n');
}

bool c_command_line_interface::run_command(const s_command &command) {
	bool done = false;
	if (command.command.empty()) {
		std::cout << "Invalid command\n";
	} else if (command.command == "exit") {
		done = true;
	} else if (command.command == "load_synth") {
		process_command_load_synth(command);
	} else {
		std::cout << "Invalid command\n";
	}
	return done;
}

void c_command_line_interface::list_devices() {
	std::cout << "Audio devices:\n";
	for (uint32 index = 0; index < m_runtime_context.audio_driver_interface.get_device_count(); index++) {
		s_audio_device_info device_info = m_runtime_context.audio_driver_interface.get_device_info(index);
		std::cout << index << ": " << device_info.name << "\n";
		std::cout << "\t" << device_info.host_api_name << ", "
			<< device_info.max_input_channels << " inputs, "
			<< device_info.max_output_channels << " outputs\n";
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
		settings.input_device_index = config_settings.audio_input_device_index;
		settings.input_channel_count = config_settings.audio_input_channel_count;
		copy_type(
			settings.input_channel_indices.get_elements(),
			config_settings.audio_input_channel_indices.get_elements(),
			k_max_audio_channels);

		settings.output_device_index = config_settings.audio_output_device_index;
		settings.output_channel_count = config_settings.audio_output_channel_count;
		copy_type(
			settings.output_channel_indices.get_elements(),
			config_settings.audio_output_channel_indices.get_elements(),
			k_max_audio_channels);

		settings.sample_rate = config_settings.audio_sample_rate;
		settings.sample_format = config_settings.audio_sample_format;
		settings.frames_per_buffer = config_settings.audio_frames_per_buffer;

		settings.stream_callback = m_runtime_context.stream_callback;
		settings.stream_callback_user_data = &m_runtime_context;

		s_audio_driver_result result = m_runtime_context.audio_driver_interface.start_stream(settings);
		if (result.result != e_audio_driver_result::k_success) {
			std::cout << "Failed to start audio stream: " << result.message << "\n";
		}
	}

	// Initialize the controller
	if (config_settings.controller_device_count > 0) {
		s_controller_driver_settings settings;
		settings.device_count = config_settings.controller_device_count;
		for (size_t device = 0; device < config_settings.controller_device_count; device++) {
			settings.device_indices[device] = config_settings.controller_device_indices[device];
		}
		settings.controller_event_queue_size = config_settings.controller_event_queue_size;
		settings.unknown_latency = static_cast<real64>(config_settings.controller_unknown_latency) * 0.001;

		if (m_runtime_context.audio_driver_interface.is_stream_running()) {
			m_runtime_context.audio_driver_interface.get_stream_clock(settings.clock, settings.clock_context);
		}

		settings.event_hook = controller_hook_wrapper;
		settings.event_hook_context = this;

		s_controller_driver_result result = m_runtime_context.controller_driver_interface.start_stream(settings);
		if (result.result != e_controller_driver_result::k_success) {
			std::cout << "Failed to start controller stream: " << result.message << "\n";
		}
	}
}

void c_command_line_interface::process_command_load_synth(const s_command &command) {
	c_scoped_lock lock(m_command_lock);

	if (command.arguments.size() == 1) {
		if (!m_runtime_context.audio_driver_interface.is_stream_running()) {
			std::cout << "Audio stream is not running\n";
			return;
		}

		// Try to load the instrument
		c_instrument instrument;

		// First argument is the path to load
		e_instrument_result load_result = instrument.load(command.arguments[0].c_str());
		if (load_result != e_instrument_result::k_success) {
			std::cout << "Failed to load '" << command.arguments[0] <<
				"' (result code " << enum_index(load_result) << ")\n";
			return;
		}

		// Select the native module graph from the instrument
		uint32 instrument_variant_index;
		{
			s_instrument_variant_requirements requirements;
			requirements.sample_rate = static_cast<uint32>(
				m_runtime_context.audio_driver_interface.get_settings().sample_rate);

			e_instrument_variant_for_requirements_result instrument_variant_result =
				instrument.get_instrument_variant_for_requirements(requirements, instrument_variant_index);

			if (instrument_variant_result == e_instrument_variant_for_requirements_result::k_no_match) {
				std::cout << "Failed to find instrument variant matching the runtime requirements\n";
				return;
			} else if (instrument_variant_result == e_instrument_variant_for_requirements_result::k_ambiguous_matches) {
				std::cout << "Found multiple instrument variants matching the runtime requirements - "
					"refine stream parameters or instrument globals\n";
				return;
			} else {
				wl_assert(instrument_variant_result == e_instrument_variant_for_requirements_result::k_success);
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
			settings.input_channel_count = runtime_config_settings.audio_input_channel_count;
			settings.output_channel_count = runtime_config_settings.audio_output_channel_count;
			settings.controller_event_queue_size = runtime_config_settings.controller_event_queue_size;
			settings.max_controller_parameters = runtime_config_settings.executor_max_controller_parameters;
			settings.process_controller_events = s_runtime_context::process_controller_events_callback;
			settings.process_controller_events_context = &m_runtime_context;
			settings.event_console_enabled = runtime_config_settings.executor_console_enabled;
			settings.profiling_enabled = runtime_config_settings.executor_profiling_enabled;
			settings.profiling_threshold = runtime_config_settings.executor_profiling_threshold;
			m_runtime_context.executor.initialize(
				settings,
				c_wrapped_array<void *>(m_task_function_library_contexts));
		}

		m_runtime_context.active_instrument = loading_instrument;
		return;
	}

	std::cout << "Invalid command\n";
}

bool c_command_line_interface::controller_hook_wrapper(void *context, const s_controller_event &controller_event) {
	return static_cast<c_command_line_interface *>(context)->controller_hook(controller_event);
}

bool c_command_line_interface::controller_hook(const s_controller_event &controller_event) {
#if IS_TRUE(TARGET_RASPBERRY_PI)
	// Hack: use events on MIDI parameter 15 to load a different synths!
	static constexpr uint32 k_synth_parameter = 15;
	if (controller_event.event_type == e_controller_event_type::k_parameter_change) {
		const s_controller_event_data_parameter_change *parameter_change_controller_event =
			controller_event.get_data<s_controller_event_data_parameter_change>();
		if (parameter_change_controller_event->parameter_id == k_synth_parameter) {
			int32 synth_index = static_cast<int32>(std::round(parameter_change_controller_event->value * 127.0f));
			s_command command;
			command.command = "load_synth";
			command.arguments.push_back(std::to_string(synth_index) + ".wls");
			{
				c_scoped_lock lock(m_controller_command_lock);
				m_controller_commands.push(command);
			}
			m_controller_command_semaphore.notify();
			return true;
		}
	}
#endif // IS_TRUE(TARGET_RASPBERRY_PI)

	return false;
}

void c_command_line_interface::controller_command_thread_entry_point(const s_thread_parameter_block *params) {
	(*params->get_memory_typed<c_command_line_interface *>())->controller_command_thread_main();
}

void c_command_line_interface::controller_command_thread_main() {
	bool done = false;
	while (!done) {
		m_controller_command_semaphore.wait();

		s_command command;
		{
			c_scoped_lock lock(m_controller_command_lock);
			if (!m_controller_commands.empty()) {
				command = m_controller_commands.front();
				m_controller_commands.pop();
			}
		}

		if (!command.command.empty()) {
			done = run_command(command);
		}
	}
}
