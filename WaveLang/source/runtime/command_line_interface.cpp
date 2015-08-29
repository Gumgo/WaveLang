#include "common/common.h"
#include "runtime/runtime_context.h"
#include "execution_graph/execution_graph.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

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

	static s_command process_command();
	static size_t skip_whitespace(const std::string &str, size_t pos);
	static size_t skip_to_whitespace(const std::string &str, size_t pos);
	static size_t skip_to_quote(const std::string &str, size_t pos, std::string &out_result);
	static bool is_whitespace(char c);

	void process_command_init_stream(const s_command &command);
	void process_command_load_synth(const s_command &command);

	s_runtime_context runtime_context;
};

int main(int argc, char **argv) {
	int result;
	{
		c_command_line_interface cli(argc, argv);
		result = cli.main_function();
	}
	return result;
}

c_command_line_interface::c_command_line_interface(int argc, char **argv) {
}

c_command_line_interface::~c_command_line_interface() {

}

int c_command_line_interface::main_function() {
	{
		s_driver_result driver_result = runtime_context.driver_interface.initialize();
		if (driver_result.result != k_driver_result_success) {
			std::cout << driver_result.message << "\n";
			return 1;
		}
	}

	// None active initially
	runtime_context.active_task_graph = -1;

	bool done = false;
	while (!done) {
		s_command command = process_command();

		if (command.command.empty()) {
			std::cout << "Invalid command\n";
		} else if (command.command == "exit") {
			done = true;
		} else if (command.command == "init_stream") {
			process_command_init_stream(command);
		} else if (command.command == "load_synth") {
			process_command_load_synth(command);
		} else {
			std::cout << "Invalid command\n";
		}
	}

	runtime_context.driver_interface.stop_stream();
	runtime_context.driver_interface.shutdown();

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

void c_command_line_interface::process_command_init_stream(const s_command &command) {
	s_driver_settings settings;
	bool valid_settings = false;
	bool read_settings = true;

	if (command.arguments.size() == 1) {
		if (command.arguments[0] == "list") {
			for (size_t index = 0; index < runtime_context.driver_interface.get_device_count(); index++) {
				s_device_info device_info = runtime_context.driver_interface.get_device_info(index);
				std::cout << index << ": " << device_info.name << "\n";
			}
			return;
		} else if (command.arguments[0] == "default") {
			if (!VALID_INDEX(runtime_context.driver_interface.get_default_device_index(),
				runtime_context.driver_interface.get_device_count())) {
				std::cout << "No default device\n";
				return;
			}

			s_device_info device_info = runtime_context.driver_interface.get_device_info(
				runtime_context.driver_interface.get_default_device_index());

			settings.device_index = runtime_context.driver_interface.get_default_device_index();
			settings.sample_rate = device_info.default_sample_rate;
			settings.output_channels = std::min(2u, device_info.max_output_channels);
			settings.sample_format = k_sample_format_float32;
			settings.frames_per_buffer = 512; // $TODO how do I choose this?
			valid_settings = true;
			read_settings = false;
		}
	}

	if (read_settings) {
		if (command.arguments.size() == 5) {
			try {
				settings.device_index = std::stoul(command.arguments[0]);
				if (!VALID_INDEX(settings.device_index, runtime_context.driver_interface.get_device_count())) {
					throw std::invalid_argument("Invalid device index");
				}

				settings.sample_rate = std::stod(command.arguments[1]);
				settings.output_channels = std::stoul(command.arguments[2]);

				// This static assert will fire if we add or remove a format
				static_assert(k_sample_format_count == 1, "Update sample format parse logic");
				if (command.arguments[3] == "float32") {
					settings.sample_format = k_sample_format_float32;
				} else {
					throw std::invalid_argument("Invalid format");
				}

				settings.frames_per_buffer = std::stoul(command.arguments[4]);

				// If we've gotten to this point, we will attempt to open the stream
				valid_settings = true;
			} catch (const std::invalid_argument &) {
			} catch (const std::out_of_range &) {
			}
		}
	}

	if (valid_settings) {
		// Stop the old stream
		runtime_context.driver_interface.stop_stream();

		settings.stream_callback = runtime_context.stream_callback;
		settings.stream_callback_user_data = &runtime_context;
		s_driver_result result = runtime_context.driver_interface.start_stream(settings);
		if (result.result != k_driver_result_success) {
			std::cout << result.message << "\n";
		}

		return;
	}

	std::cout << "Invalid command\n";
}

void c_command_line_interface::process_command_load_synth(const s_command &command) {
	if (command.arguments.size() == 1) {
		// Try to load the execution graph
		c_execution_graph execution_graph;

		// First argument is the path to load
		e_execution_graph_result load_result = execution_graph.load(command.arguments[0].c_str());
		if (load_result != k_execution_graph_result_success) {
			std::cout << "Failed to load '" << command.arguments[0] << "' (result code " << load_result << ")\n";
			return;
		}

		// Load into the inactive task graph
		int32 loading_task_graph;
		if (runtime_context.active_task_graph == -1) {
			loading_task_graph = 0;
		} else {
			loading_task_graph = (runtime_context.active_task_graph == 0) ? 1 : 0;
		}

		c_task_graph &task_graph = runtime_context.task_graphs[loading_task_graph];
		if (!task_graph.build(execution_graph)) {
			std::cout << "Failed to build task graph\n";
			return;
		}

		if (runtime_context.driver_interface.is_stream_running()) {
			// Set up the executor with the new graph
			runtime_context.executor.shutdown();

			s_executor_settings settings;
			settings.task_graph = &task_graph;
			settings.sample_rate = static_cast<uint32>(runtime_context.driver_interface.get_settings().sample_rate);
			settings.max_buffer_size = runtime_context.driver_interface.get_settings().frames_per_buffer;
			settings.output_channels = runtime_context.driver_interface.get_settings().output_channels;
			runtime_context.executor.initialize(settings);
		}

		runtime_context.active_task_graph = loading_task_graph;
		return;
	}

	std::cout << "Invalid command\n";
}
