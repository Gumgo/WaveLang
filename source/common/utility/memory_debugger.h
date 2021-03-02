#pragma once

#include "common/common.h"

void initialize_memory_debugger();

// Sets whether memory allocations are allowed on a per-thread basis
#if IS_TRUE(ASSERTS_ENABLED)

void set_memory_allocations_allowed(bool allowed);
bool are_memory_allocations_allowed();

class c_set_memory_allocations_allowed_for_scope {
public:
	c_set_memory_allocations_allowed_for_scope(bool allowed) {
		m_previous_memory_allocations_allowed = are_memory_allocations_allowed();
		set_memory_allocations_allowed(allowed);
	}

	UNCOPYABLE(c_set_memory_allocations_allowed_for_scope);

	~c_set_memory_allocations_allowed_for_scope() {
		set_memory_allocations_allowed(m_previous_memory_allocations_allowed);
	}

private:
	bool m_previous_memory_allocations_allowed;
};

#define SET_MEMORY_ALLOCATIONS_ALLOWED_FOR_SCOPE(allowed)														\
	c_set_memory_allocations_allowed_for_scope TOKEN_CONCATENATE(memory_allocations_allowed, __LINE__)(allowed)

#else // IS_TRUE(ASSERTS_ENABLED)

inline void set_memory_allocations_allowed(bool allowed) {};

#define SET_MEMORY_ALLOCATIONS_ALLOWED_FOR_SCOPE(allowed) NOOP

#endif // IS_TRUE(ASSERTS_ENABLED)
