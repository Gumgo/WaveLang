#pragma once

#include "common/macros.h"
#include "common/types.h"

#ifdef _DEBUG
#define ASSERTS_ENABLED 1
#else // _DEBUG
#define ASSERTS_ENABLED 0
#endif // _DEBUG

#if IS_TRUE(ASSERTS_ENABLED)

#define IF_ASSERTS_ENABLED(x) x

#define wl_assert(expression) MACRO_BLOCK(				\
	if (!(expression)) {								\
		handle_assert(#expression, __FILE__, __LINE__);	\
	}													\
)

#define wl_vassert(expression, message) MACRO_BLOCK(	\
	if (!(expression)) {								\
		handle_assert(message, __FILE__, __LINE__);		\
	}													\
)

#define wl_halt() handle_assert("halt")

#define wl_vhalt(message) handle_assert(message, __FILE__, __LINE__)

#define wl_unreachable() handle_assert("unreachable", __FILE__, __LINE__)

void handle_assert(const char *message, const char *file, int32 line);

#else // IS_TRUE(ASSERTS_ENABLED)

// $TODO change vassert to assertf and vhalt to haltf
#define IF_ASSERTS_ENABLED(x)
#define wl_assert(expression)			NOOP
#define wl_vassert(expression, message)	NOOP
#define wl_halt()						NOOP
#define wl_vhalt(message)				NOOP
#define wl_unreachable()				NOOP

#endif // IS_TRUE(ASSERTS_ENABLED)

