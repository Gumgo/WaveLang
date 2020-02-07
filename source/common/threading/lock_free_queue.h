#pragma once

#include "common/common.h"
#include "common/threading/lock_free.h"
#include "common/threading/lock_free_pool.h"

template<typename t_element>
class c_lock_free_queue {
public:
	typedef c_wrapped_array<t_element> c_element_array;

	// Non-thread-safe functions:

	c_lock_free_queue();

	// Initializes the queue with element backing memory and a free list. The free list must contain one extra element
	// due implementation details.
	void initialize(
		c_element_array element_memory,
		c_lock_free_handle_array queue_memory,
		c_lock_free_handle_array free_list_memory);

	// Thread-safe functions:

	// Push onto queue, returns false if queue backing memory is full
	bool push(const t_element &element);

	// Pop from queue, returns false if queue is empty
	bool pop(t_element &out_element);

	// Pop from queue, returns false if queue is empty (does not copy the element)
	bool pop();

	// Peek at the top element, returns false if queue is empty
	bool peek(t_element &out_element);

private:
	c_element_array m_elements;

	s_aligned_lock_free_handle m_front;	// Points to next element to be popped
	s_aligned_lock_free_handle m_back;	// Points to most recently pushed element
	c_lock_free_handle_array m_queue;	// Points to the element to be popped after this one

	c_lock_free_pool m_free_list;
};

#include "common/threading/lock_free_queue.inl"

