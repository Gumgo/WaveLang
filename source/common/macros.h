#ifndef WAVELANG_MACROS_H__
#define WAVELANG_MACROS_H__

// No-op statement which is optimized away
#define NOOP do {} while (0)

// Allows for multiple statements in a single macro
#define MACRO_BLOCK(x) do { x } while (0)

// Causes an error if x is used before it has been defined
#define PREDEFINED(opt, ...) (1 / defined opt##__VA_ARGS__ && opt)

#define NUMBEROF(x) (sizeof(x) / (sizeof(x[0])))

#define VALID_INDEX(i, c) ((i) >= 0 && (i) < (c))

#define ZERO_STRUCT(s) memset(s, 0, sizeof(*s))

#define STATIC_MIN(a, b) ((a) < (b) ? (a) : (b))
#define STATIC_MAX(a, b) ((a) > (b) ? (a) : (b))

#define SIZEOF_BITS(x) (sizeof(x) * 8)

#endif // WAVELANG_MACROS_H__
