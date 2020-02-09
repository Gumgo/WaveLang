#include "common/threading/thread.h"

#if IS_TRUE(USE_THREAD_IMPLEMENTATION_WINDOWS)

c_thread::c_thread() {
	m_thread_handle = nullptr;
}

c_thread::c_thread(c_thread &&other) {
	wl_assert(!other.is_running());
	m_thread_handle = nullptr;
}

c_thread::~c_thread() {
	// We better have joined on this thread before destroying
	wl_assert(!is_running());
}

void c_thread::start(const s_thread_definition &thread_definition) {
	wl_assert(!is_running());
	wl_assert(valid_enum_index(thread_definition.thread_priority));
	wl_assert(thread_definition.thread_entry_point);

	// Store thread function and parameter block for the thread to use
	m_thread_entry_point = thread_definition.thread_entry_point;
	memcpy(&m_thread_parameter_block, &thread_definition.parameter_block, sizeof(m_thread_parameter_block));

	// Create the thread using the definition provided
	DWORD thread_id;
	m_thread_handle = CreateThread(
		nullptr,						// Don't need security attributes
		thread_definition.stack_size,	// Thread stack size, 0 chooses default
		&c_thread::thread_entry_point,	// Thread routine
		this,							// Parameter is a pointer to this
		CREATE_SUSPENDED,				// Start suspended so we can set up some additional properties
		&thread_id);					// Cache thread ID

	wl_assert(m_thread_handle);

	// Cache the thread ID
	m_thread_id = static_cast<uint32>(thread_id);

	// Setup thread properties
	static const int k_thread_priority_map[] = {
		THREAD_PRIORITY_LOWEST,
		THREAD_PRIORITY_BELOW_NORMAL,
		THREAD_PRIORITY_NORMAL,
		THREAD_PRIORITY_ABOVE_NORMAL,
		THREAD_PRIORITY_HIGHEST
	};
	static_assert(NUMBEROF(k_thread_priority_map) == enum_count<e_thread_priority>(),
		"Thread priority mapping mismatch");

	if (!SetThreadPriority(m_thread_handle, k_thread_priority_map[enum_index(thread_definition.thread_priority)])) {
		wl_vhalt("Failed to set thread priority");
	}

	if (thread_definition.processor != -1 &&
		SetThreadIdealProcessor(m_thread_handle, static_cast<DWORD>(thread_definition.processor)) < 0) {
		wl_vhalt("Failed to set ideal processor");
	}

	// Start the thread - should return 1 because the thread previously had a suspend count of 1
	if (ResumeThread(m_thread_handle) != 1) {
		wl_vhalt("Failed to start thread");
	}
}

void c_thread::join() {
	wl_assert(is_running());
	if (WaitForSingleObject(m_thread_handle, INFINITE) != WAIT_OBJECT_0) {
		wl_vhalt("Failed to join thread");
	}
	CloseHandle(m_thread_handle);
	m_thread_handle = nullptr;
}

bool c_thread::is_running() const {
	return m_thread_handle != nullptr;
}

c_thread::t_thread_id c_thread::get_thread_id() const {
	wl_assert(is_running());
	return m_thread_id;
}

c_thread::t_thread_id c_thread::get_current_thread_id() {
	return static_cast<uint32>(GetCurrentThreadId());
}

DWORD WINAPI c_thread::thread_entry_point(LPVOID param) {
	c_thread *this_thread = static_cast<c_thread *>(param);

	// Run the thread function
	wl_assert(this_thread->m_thread_entry_point);
	this_thread->m_thread_entry_point(&this_thread->m_thread_parameter_block);

	return 0;
}

#endif // IS_TRUE(USE_THREAD_IMPLEMENTATION_WINDOWS)