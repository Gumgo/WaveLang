#include "common/asserts.h"
#include <cassert>

#ifdef _DEBUG

void handle_assert(const char *message) {
	assert(false);
}

#endif // _DEBUG