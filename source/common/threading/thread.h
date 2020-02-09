#pragma once

#include "common/common.h"

#define PREFER_THREAD_IMPLEMENTATION_FALLBACK 0

#define USE_THREAD_IMPLEMENTATION_WINDOWS 0

#if !IS_TRUE(PREFER_THREAD_IMPLEMENTATION_FALLBACK)
	#if IS_TRUE(PLATFORM_WINDOWS)
		#undef USE_THREAD_IMPLEMENTATION_WINDOWS
		#define USE_THREAD_IMPLEMENTATION_WINDOWS 1
	#endif // implementation
#endif // !IS_TRUE(PREFER_THREAD_IMPLEMENTATION_FALLBACK)

#if IS_TRUE(USE_THREAD_IMPLEMENTATION_WINDOWS)
// No additional includes
#else // fallback
#include <thread>
#endif // fallback

enum class e_thread_priority {
	k_lowest,
	k_low,
	k_normal,
	k_high,
	k_highest,

	k_count
};

// Parameters for thread function
// Since we don't know what might go in here, might as well align
struct alignas(16) s_thread_parameter_block {
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
class c_thread {
public:
#if IS_TRUE(USE_THREAD_IMPLEMENTATION_WINDOWS)
	typedef uint32 t_thread_id;
#else // fallback
	typedef std::thread::id t_thread_id;
#endif // fallback

	// Initializes to not running state
	c_thread();

	UNCOPYABLE(c_thread);

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
	t_thread_id get_thread_id() const;

	// Returns the ID of the current thread
	static t_thread_id get_current_thread_id();

private:
	// Thread function and parameter block
	f_thread_entry_point m_thread_entry_point;
	s_thread_parameter_block m_thread_parameter_block;

	// Cached thread ID
	t_thread_id m_thread_id;

	// Implementation-specific thread handle
#if IS_TRUE(USE_THREAD_IMPLEMENTATION_WINDOWS)
	HANDLE m_thread_handle;
	static DWORD WINAPI thread_entry_point(LPVOID param);
#else // fallback
	std::thread m_thread;
	static void thread_entry_point(const c_thread *this_ptr);
#endif // fallback
};

