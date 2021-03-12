#include "common/asserts.h"

#include <cassert>
#include <iostream>

#if IS_TRUE(ASSERTS_ENABLED)

static thread_local bool tl_did_current_thread_assert = false;

void handle_assert(const char *message, const char *file, int32 line) {
	tl_did_current_thread_assert = true;

	std::cerr << "Assertion failed (" << file << ":" << line << "): " << message << "\n";
	assert(false);

	tl_did_current_thread_assert = false;
}

bool did_current_thread_assert() {
	return tl_did_current_thread_assert;
}

#endif // IS_TRUE(ASSERTS_ENABLED)
