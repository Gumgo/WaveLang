#pragma once

#include "common/common.h"

#include "console/console_commands.h"

class c_event_console {
public:
	c_event_console();
	~c_event_console();

	bool start();
	void stop();

	bool is_running() const;

	void print_to_console(const char *text, e_console_color color = e_console_color::k_white);
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

#if IS_TRUE(PLATFORM_WINDOWS)
	HANDLE m_pipe;
	HANDLE m_process;
#elif IS_TRUE(PLATFORM_LINUX)
	int m_pipe;
	pid_t m_process;
	FILE *m_write_stream;
#else // platform
//#error Unimplemented platform
#endif // platform
};

