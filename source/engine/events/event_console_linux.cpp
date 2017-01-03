#include "engine/events/event_console.h"

#if IS_TRUE(PLATFORM_LINUX)

#include "common/utility/stopwatch.h"

#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/wait.h>

static const char *k_event_console_executable_name = "console";

void c_event_console::initialize_platform() {
	m_pipe = -1;
	m_process = -1;
	m_last_color = k_console_color_count;
	m_write_stream = nullptr;
}

void c_event_console::shutdown_platform() {
	if (m_pipe != -1) {
		stop();
	}
}

bool c_event_console::start_platform() {
	// Create a unique name for the pipe
	struct timespec clock_result;
	IF_ASSERTS_ENABLED(int clock_result_code = ) clock_gettime(CLOCK_REALTIME, &clock_result);
	wl_assert(clock_result_code == 0);
	int64 clock_time = clock_result.tv_sec * k_nanoseconds_per_second + clock_result.tv_nsec;
	std::string pipe_name = "event_console_pipe_" + std::to_string(clock_time);

	// Create the pipe
	{
		m_pipe = mkfifo(pipe_name.c_str(), 0666);

		if (m_pipe == -1) {
			return false;
		}
	}

	// Create a separate process for the console
	{
		pid_t process_id = fork();

		if (process_id == -1) {
			close(m_pipe);
			m_pipe = -1;
			return false;
		} else if (process_id == 0) {
			int exec_result = execl(k_event_console_executable_name,
				k_event_console_executable_name,	// Argument 0 is executable name
				pipe_name.c_str(),					// Argument 1 is pipe name
				nullptr);

			if (exec_result == -1) {
				// If we failed to load the console executable image, terminate immediately
				exit(1);
			}
		} else {
			m_process = process_id;
		}
	}

	// Connect the pipe
	{
		m_write_stream = fopen(pipe_name.c_str(), "w");

		if (!m_write_stream) {
			kill(m_process, SIGKILL);
			close(m_pipe);
			m_pipe = -1;
			m_process = -1;
			return false;
		}
	}

	return true;
}

void c_event_console::stop_platform() {
	wl_assert(m_pipe != -1);
	wl_assert(m_process != -1);
	wl_assert(m_write_stream);

	exit_command();

	// Wait for the process to close
	int wstatus;
	waitpid(m_process, &wstatus, 0);

	fclose(m_write_stream);
	close(m_pipe);

	m_pipe = -1;
	m_process = -1;
	m_write_stream = nullptr;
}

bool c_event_console::is_running_platform() const {
	return m_process != -1;
}

void c_event_console::write_platform(uint32 bytes, const void *buffer) {
	wl_assert(m_pipe != -1);
	wl_assert(m_write_stream);
	fwrite(buffer, bytes, 1, m_write_stream);
}

#endif // IS_TRUE(PLATFORM_LINUX)
