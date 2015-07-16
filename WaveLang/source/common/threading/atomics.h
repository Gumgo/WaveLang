#ifndef WAVELANG_ATOMICS_H__
#define WAVELANG_ATOMICS_H__

#include "common/common.h"

class c_atomic_int32 {
public:
	// Not thread safe - simply sets the initial value. Construction of an object inherently is not atomic, so there is
	// no reason to make the initialization atomic.
	inline void initialize(int32 value);

	// Simply returns the value directly - not thread safe
	inline int32 get_unsafe() const;

	// Returns the value
	inline int32 get();

	// Exchanges the value with x
	inline int32 exchange(int32 x);

	// If the current value equals c, exchanges the value with x
	inline int32 compare_exchange(int32 c, int32 x);

	// Adds x to the value
	inline int32 add(int32 x);

	// Increments the value
	inline int32 increment();

	// Decrements the value
	inline int32 decrement();

	// ANDs the value with x
	inline int32 and(int32 x);

	// ORs the value with x
	inline int32 or(int32 x);

	// XORs the value with x
	inline int32 xor(int32 x);

	// Performs the specified operation atomically
	// T should implement int32 operator()(int32 value) const
	// The operation must be a pure function of value, i.e. should always produce the same result when called with the
	// same value
	// Returns previous value
	template<typename t_operation> int32 execute_atomic(const t_operation &operation);

private:
	volatile int32 m_value;
};

class c_atomic_int64 {
public:
	// Not thread safe - simply sets the initial value. Construction of an object inherently is not atomic, so there is
	// no reason to make the initialization atomic.
	inline void initialize(int64 value);

	// Simply returns the value directly - not thread safe
	inline int64 get_unsafe() const;

	// Returns the value
	inline int64 get();

	// Exchanges the value with x
	inline int64 exchange(int64 x);

	// If the current value equals c, exchanges the value with x
	inline int64 compare_exchange(int64 c, int64 x);

	// Adds x to the value
	inline int64 add(int64 x);

	// Increments the value
	inline int64 increment();

	// Decrements the value
	inline int64 decrement();

	// ANDs the value with x
	inline int64 and(int64 x);

	// ORs the value with x
	inline int64 or(int64 x);

	// XORs the value with x
	inline int64 xor(int64 x);

	// Performs the specified operation atomically
	// T should implement int64 operator()(int64 value) const
	// The operation must be a pure function of value, i.e. should always produce the same result when called with the
	// same value
	// Returns previous value
	template<typename t_operation> int64 execute_atomic(const t_operation &operation);

private:
	volatile int64 m_value;
};

#include "common/threading/atomics.inl"

#endif // WAVELANG_ATOMICS_H__