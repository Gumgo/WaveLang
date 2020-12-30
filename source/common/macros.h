#pragma once

// No-op statement which is optimized away
#define NOOP do {} while (0)

// Allows for multiple statements in a single macro
#define MACRO_BLOCK(x) do { x } while (0)

// Causes an error if x is used before it has been defined
#define IS_TRUE(opt, ...) (1 / defined opt##__VA_ARGS__ && opt)

#define STATIC_ASSERT(x) static_assert(x, #x)
#define STATIC_ASSERT_MSG(x, message) static_assert(x, message)
#define STATIC_UNREACHABLE() static_assert(false, "Unreachable")

// This function is not marked constexpr so it will fail if evaluated inside of a constexpr function
inline bool constexpr_error(bool x) { return x; }
#define CONSTEXPR_ERROR() constexpr_error(false)

#define SIZEOF_BITS(x) (sizeof(x) * 8)

#define TOKEN_CONCATENATE_HELPER(x, y) x ## y
#define TOKEN_CONCATENATE(x, y) TOKEN_CONCATENATE_HELPER(x, y)

#define CONDITION_DECLARATION(...)				\
	bool k_condition = __VA_ARGS__,				\
	typename = std::enable_if_t<k_condition>
