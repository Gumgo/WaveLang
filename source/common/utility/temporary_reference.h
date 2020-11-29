#pragma once

#include "common/common.h"

// This class is used to pass around data in a temporary context. When it goes out of scope, the data should have been
// passed along or cleared.
template<typename t_type>
class c_temporary_reference {
public:
	c_temporary_reference() = default;
	~c_temporary_reference() {
		wl_assert(!m_pointer);
	}

	UNCOPYABLE(c_temporary_reference);

	template<typename t_other_type, CONDITION_DECLARATION(std::is_convertible_v<t_other_type *, t_type *>)>
	explicit c_temporary_reference(t_other_type *pointer)
		: m_pointer(pointer) {}

	c_temporary_reference(c_temporary_reference &&other) {
		std::swap(m_pointer, other.m_pointer);
	}

	c_temporary_reference &operator=(c_temporary_reference &&other) {
		std::swap(m_pointer, other.m_pointer);
	}

	// This is used to initialize the object when its current value is null
	template<typename t_other_type, CONDITION_DECLARATION(std::is_convertible_v<t_other_type *, t_type *>)>
	void initialize(t_other_type *pointer) {
		wl_assert(!m_pointer);
		m_pointer = pointer;
	}

	t_type *get() {
		return m_pointer;
	}

	const t_type *get() const {
		return m_pointer;
	}

	t_type &operator*() {
		return *m_pointer;
	}

	const t_type &operator*() const {
		return *m_pointer;
	}

	t_type *operator->() {
		return m_pointer;
	}

	const t_type *operator->() const {
		return m_pointer;
	}

	t_type *release() {
		t_type *pointer = m_pointer;
		m_pointer = nullptr;
		return pointer;
	}

private:
	t_type *m_pointer = nullptr;
};
