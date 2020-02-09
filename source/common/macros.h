#pragma once

// No-op statement which is optimized away
#define NOOP do {} while (0)

// Allows for multiple statements in a single macro
#define MACRO_BLOCK(x) do { x } while (0)

// Causes an error if x is used before it has been defined
#define IS_TRUE(opt, ...) (1 / defined opt##__VA_ARGS__ && opt)

#define NUMBEROF(x) (sizeof(x) / (sizeof(x[0])))

// $TODO make constexpr version
#define VALID_INDEX(i, c) ((i) >= 0 && (i) < (c))

#define ZERO_STRUCT(s) memset(s, 0, sizeof(*s))

#define STATIC_MIN(a, b) ((a) < (b) ? (a) : (b))
#define STATIC_MAX(a, b) ((a) > (b) ? (a) : (b))

#define SIZEOF_BITS(x) (sizeof(x) * 8)

#define TOKEN_CONCATENATE_HELPER(x, y) x ## y
#define TOKEN_CONCATENATE(x, y) TOKEN_CONCATENATE_HELPER(x, y)
