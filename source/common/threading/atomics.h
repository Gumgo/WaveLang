#pragma once

#include "common/common.h"

// Performs the specified operation atomically
// t_atomic should be of type std::atomic<v>
// t_operation should implement v operator()(v value) const, where v is the underlying atomic type
// The operation must be a pure function of value, i.e. should always produce the same result when called with the
// same value. The previous value is returned.
template<typename t_atomic, typename t_operation, typename t_value = t_atomic::value_type> t_value execute_atomic(
	t_atomic &atomic,
	const t_operation &operation) {
	t_value original, result;
	do {
		original = atomic;
		result = operation(original);
	} while (!atomic.compare_exchange_strong(original, result));
	return original;
}
