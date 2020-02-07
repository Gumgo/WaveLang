#pragma once

#include "common/common.h"

#define PREFER_ATOMICS_IMPLEMENTATION_FALLBACK 0

#define USE_ATOMICS_IMPLEMENTATION_WINDOWS 0

#if !IS_TRUE(PREFER_ATOMICS_IMPLEMENTATION_FALLBACK)
	#if IS_TRUE(PLATFORM_WINDOWS)
		#undef USE_ATOMICS_IMPLEMENTATION_WINDOWS
		#define USE_ATOMICS_IMPLEMENTATION_WINDOWS 1
	#endif // implementation
#endif // !IS_TRUE(PREFER_ATOMICS_IMPLEMENTATION_FALLBACK)

#if IS_TRUE(USE_ATOMICS_IMPLEMENTATION_WINDOWS)
// No additional includes
#else // fallback
#include <atomic>
#endif // fallback

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
	inline int32 bitwise_and(int32 x);

	// ORs the value with x
	inline int32 bitwise_or(int32 x);

	// XORs the value with x
	inline int32 bitwise_xor(int32 x);

	// Performs the specified operation atomically
	// T should implement int32 operator()(int32 value) const
	// The operation must be a pure function of value, i.e. should always produce the same result when called with the
	// same value
	// Returns previous value
	template<typename t_operation> int32 execute_atomic(const t_operation &operation);

private:
#if IS_TRUE(USE_ATOMICS_IMPLEMENTATION_WINDOWS)
	volatile int32 m_value;
#else // fallback
	std::atomic<int32> m_value;
#endif // fallback
};

static_assert(sizeof(c_atomic_int32) == sizeof(int32), "c_atomic_int32 not 32 bits?");

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
	inline int64 bitwise_and(int64 x);

	// ORs the value with x
	inline int64 bitwise_or(int64 x);

	// XORs the value with x
	inline int64 bitwise_xor(int64 x);

	// Performs the specified operation atomically
	// T should implement int64 operator()(int64 value) const
	// The operation must be a pure function of value, i.e. should always produce the same result when called with the
	// same value
	// Returns previous value
	template<typename t_operation> int64 execute_atomic(const t_operation &operation);

private:
#if IS_TRUE(USE_ATOMICS_IMPLEMENTATION_WINDOWS)
	volatile int64 m_value;
#else // fallback
	std::atomic<int64> m_value;
#endif // fallback
};

static_assert(sizeof(c_atomic_int32) == sizeof(int32), "c_atomic_int64 not 64 bits?");

inline void read_write_barrier();
inline void read_barrier();
inline void write_barrier();

#if IS_TRUE(USE_ATOMICS_IMPLEMENTATION_WINDOWS)
#include "common/threading/atomics_windows.inl"
#else // fallback
#include "common/threading/atomics_fallback.inl"
#endif // fallback

