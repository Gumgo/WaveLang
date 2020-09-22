#pragma once

// No-op statement which is optimized away
#define NOOP do {} while (0)

// Allows for multiple statements in a single macro
#define MACRO_BLOCK(x) do { x } while (0)

// Causes an error if x is used before it has been defined
#define IS_TRUE(opt, ...) (1 / defined opt##__VA_ARGS__ && opt)

#define SIZEOF_BITS(x) (sizeof(x) * 8)

#define TOKEN_CONCATENATE_HELPER(x, y) x ## y
#define TOKEN_CONCATENATE(x, y) TOKEN_CONCATENATE_HELPER(x, y)
