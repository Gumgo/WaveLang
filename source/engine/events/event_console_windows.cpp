#include "engine/events/event_console.h"

#if PREDEFINED(PLATFORM_WINDOWS)

static const char *k_event_console_executable_name = "WaveLangConsole.exe";

void c_event_console::initialize_platform() {
	m_pipe = INVALID_HANDLE_VALUE;
	m_process = INVALID_HANDLE_VALUE;
	m_last_color = k_console_color_count;
}

void c_event_console::shutdown_platform() {
	if (m_pipe != INVALID_HANDLE_VALUE) {
		stop();
	}
}

bool c_event_console::start_platform() {
	// Create a unique name for the pipe
	std::string pipe_name = "\\\\.\\pipe\\event_console_pipe_" + std::to_string(GetTickCount());

	// Create the pipe
	{
		m_pipe = CreateNamedPipe(
			pipe_name.c_str(),
			PIPE_ACCESS_OUTBOUND,
			PIPE_TYPE_MESSAGE | PIPE_READMODE_BYTE | PIPE_WAIT,
			1,
			4096,
			0,
			0,
			nullptr);

		if (m_pipe == INVALID_HANDLE_VALUE) {
			return false;
		}
	}

	// Create a separate process for the console
	{
		STARTUPINFO startup_info;
		PROCESS_INFORMATION process_info;
		GetStartupInfo(&startup_info);

		// Send the pipe name as the first argument
		c_static_string<256> command_line;
		command_line.clear();
		command_line.append_verify(k_event_console_executable_name);
		command_line.append_verify(" ");
		command_line.append_verify(pipe_name.c_str());

		BOOL process_result = CreateProcess(
			nullptr,
			command_line.get_string_unsafe(),
			nullptr,
			nullptr,
			FALSE,
			CREATE_NEW_CONSOLE,
			nullptr,
			nullptr,
			&startup_info,
			&process_info);

		if (!process_result) {
			CloseHandle(m_pipe);
			m_pipe = INVALID_HANDLE_VALUE;
			return false;
		}

		m_process = process_info.hProcess;
	}

	// Connect the pipe
	{
		BOOL pipe_result = ConnectNamedPipe(m_pipe, nullptr);

		if (!pipe_result) {
			TerminateProcess(m_process, 1);
			CloseHandle(m_pipe);
			m_pipe = INVALID_HANDLE_VALUE;
			m_process = INVALID_HANDLE_VALUE;
			return false;
		}
	}

	return true;
}

void c_event_console::stop_platform() {
	wl_assert(m_pipe != INVALID_HANDLE_VALUE);
	wl_assert(m_process != INVALID_HANDLE_VALUE);

	exit_command();

	// Wait for the process to close
	{
		IF_ASSERTS_ENABLED(DWORD wait_result =) WaitForSingleObject(m_process, INFINITE);
		wl_assert(wait_result != WAIT_TIMEOUT);
	}

	DisconnectNamedPipe(m_pipe);
	CloseHandle(m_pipe);

	m_pipe = INVALID_HANDLE_VALUE;
	m_process = INVALID_HANDLE_VALUE;
}

bool c_event_console::is_running_platform() const {
	return m_process != INVALID_HANDLE_VALUE;
}

void c_event_console::write_platform(uint32 bytes, const void *buffer) {
	wl_assert(m_pipe != INVALID_HANDLE_VALUE);
	DWORD bytes_written;
	WriteFile(m_pipe, buffer, static_cast<DWORD>(bytes), &bytes_written, nullptr);
}

#endif // PREDEFINED(PLATFORM_WINDOWS)