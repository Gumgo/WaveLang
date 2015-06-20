#ifndef WAVELANG_ASSERTS_H__
#define WAVELANG_ASSERTS_H__

#ifdef _DEBUG

#define ASSERTS_ENABLED 1
#define IF_ASSERTS_ENABLED(x) x
void wl_assert(bool expression);
void wl_halt();

#else // _DEBUG

#define ASSERTS_ENABLED 0

#define ASSERTS_NOOP			do {} while (0)
#define IF_ASSERTS_ENABLED(x)	ASSERTS_NOOP
#define wl_assert(expression)	ASSERTS_NOOP
#define wl_halt()				ASSERTS_NOOP

#endif // _DEBUG

#endif // WAVELANG_ASSERTS_H__