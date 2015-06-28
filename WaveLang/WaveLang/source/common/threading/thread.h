#ifndef WAVELANG_THREAD_H__
#define WAVELANG_THREAD_H__

#include "common/common.h"

enum e_thread_priority {
	k_thread_priority_lowest,
	k_thread_priority_low,
	k_thread_priority_normal,
	k_thread_priority_high,
	k_thread_priority_highest,

	k_thread_priority_count
};

// Parameters for thread function
// Since we don't know what might go in here, might as well align
ALIGNAS(16) struct s_thread_parameter_block {
	// 32 bytes of memory for custom parameters
	uint8 memory[32];

	template<typename t_memory>
	t_memory *get_memory_typed() {
		static_assert(sizeof(t_memory) <= sizeof(memory), "Parameter block too small");
		return reinterpret_cast<t_memory *>(memory);
	}

	template<typename t_memory>
	const t_memory *get_memory_typed() const {
		static_assert(sizeof(t_memory) <= sizeof(memory), "Parameter block too small");
		return reinterpret_cast<const t_memory *>(memory);
	}
};

// Entry point thread function
typedef void (*f_thread_entry_point)(const s_thread_parameter_block *param_block);

// Properties of a thread
struct s_thread_definition {
	const char *thread_name;					// String identifier for the thread
	size_t stack_size;							// Size of the stack - 0 for default
	e_thread_priority thread_priority;			// The thread's priority
	int32 processor;							// Which processor the thread should run on, or -1 for any

	f_thread_entry_point thread_entry_point;	// Function to execute
	s_thread_parameter_block parameter_block;	// Parameters passed to the thread's function
};

// Class encapsulating a thread
class c_thread : private c_uncopyable {
public:
	// Initializes to not running state
	c_thread();

	// Move constructor
	c_thread(c_thread &&other);

	// The thread must be joined before being destroyed - this is asserted
	~c_thread();

	// Starts thread execution using the given thread definition
	void start(const s_thread_definition &thread_definition);

	// Blocks until the thread exits
	void join();

	// Returns true if the thread is running. Not thread safe, but will always return the correct result for the thread
	// represented by this object, or for the thread responsible for joining with this thread.
	bool is_running() const;

	// Returns the ID of this thread
	uint32 get_thread_id() const;

	// Returns the ID of the current thread
	static uint32 get_current_thread_id();

private:
	// Thread function and parameter block
	f_thread_entry_point m_thread_entry_point;
	s_thread_parameter_block m_thread_parameter_block;

	// Cached thread ID
	uint32 m_thread_id;

	// Platform-specific thread handle
#if PREDEFINED(PLATFORM_WINDOWS)
	HANDLE m_thread_handle;
	static DWORD WINAPI thread_entry_point(LPVOID param);
#else // platform
#error Unknown platform
#endif // platform
};

#endif // WAVELANG_THREAD_H__