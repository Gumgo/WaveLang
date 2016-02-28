#include "engine/events/event_console.h"

c_event_console::c_event_console() {
	initialize_platform();
}

c_event_console::~c_event_console() {
	shutdown_platform();
}

bool c_event_console::start() {
	m_last_color = k_console_color_count;
	return start_platform();
}

void c_event_console::stop() {
	stop_platform();
}

bool c_event_console::is_running() const {
	return is_running_platform();
}

void c_event_console::print_to_console(const char *text, e_console_color color) {
	wl_assert(VALID_INDEX(color, k_console_color_count));

	if (color != m_last_color) {
		uint8 command = k_console_command_set_text_color;
		write_platform(sizeof(command), &command);

		uint8 color_byte = static_cast<uint8>(color);
		write_platform(sizeof(color_byte), &color_byte);
		m_last_color = color;
	}

	{
		uint8 command = k_console_command_print;
		write_platform(sizeof(command), &command);

		uint32 length = cast_integer_verify<uint32>(strlen(text));
		write_platform(sizeof(length), &length);
		write_platform(length, text);
	}
}

void c_event_console::clear() {
	uint8 command = k_console_command_clear;
	write_platform(sizeof(command), &command);
}

void c_event_console::exit_command() {
	uint8 command = k_console_command_exit;
	write_platform(sizeof(command), &command);
}
