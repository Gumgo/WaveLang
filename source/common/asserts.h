#pragma once

#include "common/macros.h"
#include "common/platform.h"
#include "common/platform_macros.h"
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
														\
	ANALYSIS_ASSUME(expression);						\
)

#define wl_assertf(expression, message) MACRO_BLOCK(	\
	if (!(expression)) {								\
		handle_assert(message, __FILE__, __LINE__);		\
	}													\
														\
	ANALYSIS_ASSUME(expression);						\
)

#define wl_halt() wl_assert("halt")

#define wl_haltf(message) wl_assertf(false, message)

#define wl_unreachable() wl_haltf("unreachable")

void handle_assert(const char *message, const char *file, int32 line);
bool did_current_thread_assert();

#else // IS_TRUE(ASSERTS_ENABLED)

#define IF_ASSERTS_ENABLED(x)
#define wl_assert(expression)			NOOP
#define wl_assertf(expression, message)	NOOP
#define wl_halt()						NOOP
#define wl_haltf(message)				NOOP
#define wl_unreachable()				NOOP

#endif // IS_TRUE(ASSERTS_ENABLED)

