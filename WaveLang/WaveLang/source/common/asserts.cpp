#include "common/asserts.h"
#include <cassert>

#ifdef _DEBUG

void wl_assert(bool expression) {
	assert(expression);
}

void wl_halt() {
	assert(false);
}

#endif // _DEBUG