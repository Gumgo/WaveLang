#ifndef WAVELANG_MACROS_H__
#define WAVELANG_MACROS_H__

// Annoyingly, despite NOT being implemented, some header files prevent us from defining alignof and alignas! So we must
// instead use all-caps versions.
#define ALIGNOF __alignof
#define ALIGNAS(x) __declspec(align(x))

// No-op statement which is optimized away
#define NOOP do {} while (0)

// Allows for multiple statements in a single macro
#define MACRO_BLOCK(x) do { x } while (0)

// Causes an error if x is used before it has been defined
#define PREDEFINED(opt,tmp) (1 / defined opt##tmp && opt)

#define NUMBEROF(x) (sizeof(x) / (sizeof(x[0])))

#define VALID_INDEX(i, c) ((i) >= 0 && (i) < (c))

#define ZERO_STRUCT(s) memset(s, 0, sizeof(*s))

#endif // WAVELANG_MACROS_H__
