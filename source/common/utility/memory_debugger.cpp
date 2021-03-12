#include "common/asserts.h"
#include "common/utility/memory_debugger.h"

#if IS_TRUE(ASSERTS_ENABLED)
static thread_local bool tl_allocations_allowed = true;
#endif // IS_TRUE(ASSERTS_ENABLED)

#if IS_TRUE(PLATFORM_WINDOWS)
#if IS_TRUE(ASSERTS_ENABLED)
static int alloc_hook(
	int alloc_type,
	void *user_data,
	size_t size,
	int block_type,
	long request_number,
	const unsigned char *filename,
	int line_number) {
	// Assertions allocate memory for string output which we don't care about catching here
	if (!did_current_thread_assert()) {
		wl_assert(tl_allocations_allowed);
	}
	return TRUE;
}
#endif // IS_TRUE(ASSERTS_ENABLED)
#endif // IS_TRUE(PLATFORM_WINDOWS)

void initialize_memory_debugger() {
#if IS_TRUE(PLATFORM_WINDOWS)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#if IS_TRUE(ASSERTS_ENABLED)
	_CrtSetAllocHook(alloc_hook);
#endif // IS_TRUE(ASSERTS_ENABLED)
#else // IS_TRUE(PLATFORM_WINDOWS)
#error Not yet implemented
#endif // IS_TRUE(PLATFORM_WINDOWS)
}

#if IS_TRUE(ASSERTS_ENABLED)

void set_memory_allocations_allowed(bool allowed) {
	tl_allocations_allowed = allowed;
}

bool are_memory_allocations_allowed() {
	return tl_allocations_allowed;
}

#endif // IS_TRUE(ASSERTS_ENABLED)
