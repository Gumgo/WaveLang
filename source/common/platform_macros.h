#pragma once

#include "common/macros.h"
#include "common/platform.h"

#include <cstdint>

// Since platform.h relies on macros.h, we can't put these in macros.h since they rely on platform defines

#if IS_TRUE(COMPILER_MSVC)

#define ASSUME(condition) __assume(condition)
#define ASSUME_ALIGNED(pointer, alignment) ASSUME(reinterpret_cast<uintptr_t>(pointer) % (alignment) == 0), (pointer)
// Note: __analysis_assume says not to use function calls within the condition and _Analysis_assume_ doesn't mention
// this, so use that version instead
#define ANALYSIS_ASSUME(condition) _Analysis_assume_(condition)
// Does MSVC support unreachable?
#define UNREACHABLE

#elif IS_TRUE(COMPILER_GCC)

// GCC workaround because GCC (apparently) doesn't have a general-purpose "assume"
#define ASSUME(condition) MACRO_BLOCK(if (!condition) { __builtin_unreachable(); })
#define ASSUME_ALIGNED(pointer, alignment) static_cast<decltype(pointer)>(__builtin_assume_aligned(pointer, alignment))
#define ANALYSIS_ASSUME(condition) NOOP
#define UNREACHABLE __builtin_unreachable()

#elif IS_TRUE(COMPILER_CLANG)

#define ASSUME(condition) __builtin_assume(condition)
#define ASSUME_ALIGNED(pointer, alignment) static_cast<decltype(pointer)>(__builtin_assume_aligned(pointer, alignment))
// Note: see https://clang-analyzer.llvm.org/faq.html#custom_assert for possible implementation
#define ANALYSIS_ASSUME(condition) NOOP
#define UNREACHABLE __builtin_unreachable()

#else // COMPILER

#error Unknown compiler

#endif // COMPILER

