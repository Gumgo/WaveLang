#ifndef WAVELANG_EVENT_CONSOLE_H__
#define WAVELANG_EVENT_CONSOLE_H__

#include "common/common.h"
#include "console/console_commands.h"

class c_event_console {
public:
	c_event_console();
	~c_event_console();

	bool start();
	void stop();

	bool is_running() const;

	void print_to_console(const char *text, e_console_color color = k_console_color_white);
	void clear();

private:
	void initialize_platform();
	void shutdown_platform();
	bool start_platform();
	void stop_platform();
	bool is_running_platform() const;
	void write_platform(uint32 bytes, const void *buffer);

	void exit_command();

	e_console_color m_last_color;

#if PREDEFINED(PLATFORM_WINDOWS)
	HANDLE m_pipe;
	HANDLE m_process;
#else // platform
#error Unimplemented platform
#endif // platform
};

#endif // WAVELANG_EVENT_CONSOLE_H__