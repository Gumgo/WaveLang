#include "common/asserts.h"

#include <cassert>
#include <iostream>

#if IS_TRUE(ASSERTS_ENABLED)

void handle_assert(const char *message, const char *file, int32 line) {
	std::cerr << "Assertion failed (" << file << ":" << line << "): " << message << "\n";
	assert(false);
}

#endif // IS_TRUE(ASSERTS_ENABLED)
