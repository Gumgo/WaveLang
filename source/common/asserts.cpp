#include "common/asserts.h"

#include <cassert>
#include <iostream>

#ifdef _DEBUG

void handle_assert(const char *message, const char *file, int32 line) {
	std::cerr << "Assertion failed (" << file << ":" << line << "): " << message << "\n";
	assert(false);
}

#endif // _DEBUG
