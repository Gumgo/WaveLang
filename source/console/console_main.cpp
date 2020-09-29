#include "common/common.h"
#include "common/math/floating_point.h"

#include "console/console_commands.h"

#include <iostream>
#include <vector>

// Platform-specific functions:
static int initialize_platform(int argc, char **argv);
static int shutdown_platform();
static bool read_platform(uint32 bytes, void *buffer);
static void print_platform(const char *text);
static void set_text_color_platform(e_console_color color);
static void clear_platform();

// Returns true if we should exit
static bool process_next_command();

int main(int argc, char **argv) {
	initialize_floating_point_behavior();

	int init_result = initialize_platform(argc, argv);
	if (init_result != 0) {
		return init_result;
	}

	{
		bool done;
		do {
			done = process_next_command();
		} while (!done);
	}

	return shutdown_platform();
}

static bool process_next_command() {
	uint8 command;
	if (!read_platform(sizeof(command), &command)) {
		return true;
	}

	switch (static_cast<e_console_command>(command)) {
	case e_console_command::k_print:
	{
		uint32 length;
		if (!read_platform(sizeof(length), &length)) {
			return true;
		}

		if (length < 256) {
			s_static_array<char, 256> buffer;
			if (!read_platform(length, buffer.get_elements())) {
				return true;
			}

			buffer[length] = '\0';
			print_platform(buffer.get_elements());
		} else {
			// Dynamic buffer for long strings
			std::vector<char> buffer(length + 1);
			if (!read_platform(length, &buffer.front())) {
				return true;
			}

			buffer[length] = '\0';
			print_platform(&buffer.front());
		}

		break;
	}

	case e_console_command::k_set_text_color:
	{
		uint8 color;
		if (!read_platform(sizeof(color), &color)) {
			return true;
		}

		if (!valid_index(color, enum_count<e_console_color>())) {
			return true;
		}

		set_text_color_platform(static_cast<e_console_color>(color));
		break;
	}

	case e_console_command::k_clear:
		clear_platform();
		break;

	case e_console_command::k_exit:
		return true;

	default:
		// Unknown command
		return true;
	}

	return false;
}

#if IS_TRUE(PLATFORM_WINDOWS)

static HANDLE g_pipe = INVALID_HANDLE_VALUE;
static HANDLE g_console = INVALID_HANDLE_VALUE;

static int initialize_platform(int argc, char **argv) {
	if (argc != 2) {
		std::cout << "Invalid argument count";
		return 1;
	}

	g_console = GetStdHandle(STD_OUTPUT_HANDLE);

	const char *pipe_name = argv[1];
	g_pipe = INVALID_HANDLE_VALUE;

	while (g_pipe == INVALID_HANDLE_VALUE) {
		g_pipe = CreateFile(
			pipe_name,
			GENERIC_READ,
			0,
			nullptr,
			OPEN_EXISTING,
			0,
			nullptr);

		if (g_pipe == INVALID_HANDLE_VALUE) {
			// The only non-fail error is ERROR_PIPE_BUSY
			if (GetLastError() != ERROR_PIPE_BUSY) {
				std::cout << "Failed to open pipe";
				return 1;
			} else {
				// Try waiting for the pipe
				if (!WaitNamedPipe(pipe_name, NMPWAIT_USE_DEFAULT_WAIT)) {
					std::cout << "Failed to wait for pipe";
					return 1;
				}
			}
		}
	}

	return 0;
}

static int shutdown_platform() {
	wl_assert(g_pipe != INVALID_HANDLE_VALUE);
	CloseHandle(g_pipe);
	return 0;
}

static bool read_platform(uint32 bytes_to_read, void *buffer) {
	wl_assert(g_pipe);

	DWORD bytes_read;
	if (!ReadFile(g_pipe, buffer, bytes_to_read, &bytes_read, nullptr)
		|| bytes_read != bytes_to_read) {
		return false;
	}

	return true;
}

static void print_platform(const char *text) {
	DWORD chars_written;
	WriteConsole(g_console, text, static_cast<DWORD>(strlen(text)), &chars_written, nullptr);
}

static void set_text_color_platform(e_console_color color) {
	CONSOLE_SCREEN_BUFFER_INFO console_info;
	if (!GetConsoleScreenBufferInfo(g_console, &console_info)) {
		return;
	}

	// Replace the foreground color attribute
	console_info.wAttributes &= 0xfff0;
	console_info.wAttributes |= static_cast<uint16>(color);

	SetConsoleTextAttribute(g_console, console_info.wAttributes);
}

static void clear_platform() {
	CONSOLE_SCREEN_BUFFER_INFO console_info;
	if (!GetConsoleScreenBufferInfo(g_console, &console_info)) {
		return;
	}

	static constexpr COORD k_coords = { 0, 0 };
	DWORD length = console_info.dwSize.X * console_info.dwSize.Y;

	DWORD chars_written;
	FillConsoleOutputCharacter(
		g_console,
		' ',
		length,
		k_coords,
		&chars_written);

	DWORD attribs_written;
	FillConsoleOutputAttribute(
		g_console,
		console_info.wAttributes,
		length,
		k_coords,
		&attribs_written);

	SetConsoleCursorPosition(g_console, k_coords);
}

#else // platform
#error Unimplemented platform
#endif // platform