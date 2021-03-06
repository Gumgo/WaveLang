#include "common/math/floating_point.h"
#include "common/threading/thread.h"
#include "common/utility/memory_debugger.h"

#if IS_TRUE(PLATFORM_LINUX)
#include <pthread.h>
#endif // IS_TRUE(PLATFORM_LINUX)

#if !IS_TRUE(USE_THREAD_IMPLEMENTATION_WINDOWS)

c_thread::c_thread() {}

c_thread::c_thread(c_thread &&other) {
	wl_assert(!other.is_running());
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
	copy_type(&m_thread_parameter_block, &thread_definition.parameter_block);

	// Create the thread using the definition provided
	// Currently, no way to specify stack size, priority, or processor...
	m_thread = std::thread(c_thread::thread_entry_point, this);

#if IS_TRUE(PLATFORM_LINUX)
	/*static constexpr int k_thread_priority_map[] = {
		1,
		10,
		40,
		70,
		80
	};
	STATIC_ASSERT(is_enum_fully_mapped<e_thread_priority>(k_thread_priority_map));

	sched_param sch_params;
	sch_params.sched_priority = k_thread_priority_map[enum_index(thread_definition.thread_priority)];
	int policy = SCHED_FIFO;
	if (pthread_setschedparam(m_thread.native_handle(), policy, &sch_params)) {
		wl_haltf("Failed to set thread priority");
	}*/
#endif // IS_TRUE(PLATFORM_LINUX)

	// Cache the thread ID
	m_thread_id = m_thread.get_id();
}

void c_thread::join() {
	wl_assert(is_running());
	m_thread.join();
}

bool c_thread::is_running() const {
	return m_thread.joinable();
}

c_thread::t_thread_id c_thread::get_thread_id() const {
	wl_assert(is_running());
	return m_thread_id;
}

c_thread::t_thread_id c_thread::get_current_thread_id() {
	return std::this_thread::get_id();
}

void c_thread::thread_entry_point(const c_thread *this_ptr) {
	initialize_memory_debugger();
	initialize_floating_point_behavior();

	// Run the thread function
	wl_assert(this_ptr->m_thread_entry_point);
	this_ptr->m_thread_entry_point(&this_ptr->m_thread_parameter_block);
}

#endif // !IS_TRUE(USE_THREAD_IMPLEMENTATION_WINDOWS)
