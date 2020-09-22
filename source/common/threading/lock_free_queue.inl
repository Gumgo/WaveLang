template<typename t_element>
c_lock_free_queue<t_element>::c_lock_free_queue()
	: m_elements(nullptr, 0)
	, m_queue(nullptr, 0) {
}

template<typename t_element>
void c_lock_free_queue<t_element>::initialize(
	c_element_array element_memory,
	c_lock_free_handle_array queue_memory,
	c_lock_free_handle_array free_list_memory) {
	// Make sure sizes match up - we need at least 1 element for the dummy node which is unusable
	wl_vassert(element_memory.get_count() > 0, "Lock free queue requires 1 additional node");
	wl_assert(element_memory.get_count() == queue_memory.get_count());
	wl_assert(queue_memory.get_count() == free_list_memory.get_count());

	m_elements = element_memory;
	m_queue = queue_memory;
	m_free_list.initialize(free_list_memory);

	// We always keep one dummy node at the front of the queue
	// See http://www.cs.rochester.edu/u/scott/papers/1996_PODC_queues.pdf, and boost's lock-free queue
	s_lock_free_handle_data front_and_back;
	front_and_back.tag = 0;
	front_and_back.handle = m_free_list.allocate();
	m_front.handle = front_and_back.data;
	m_back.handle = front_and_back.data;

	s_lock_free_handle_data front_next;
	front_next.tag = 0;
	front_next.handle = k_lock_free_invalid_handle;
	m_queue[0].handle = front_next.data;
}

template<typename t_element>
bool c_lock_free_queue<t_element>::push(const t_element &element) {
	// Attempt to allocate an element from the pool
	uint32 handle = m_free_list.allocate();
	if (handle == k_lock_free_invalid_handle) {
		// Pool is empty
		return false;
	}

	// Store the element
	copy_type(&m_elements[handle], &element);

	s_lock_free_handle_data next;
	next.tag = 0;
	next.handle = k_lock_free_invalid_handle;
	m_queue[handle].handle = next.data;

	do {
		s_lock_free_handle_data old_back;
		s_lock_free_handle_data old_back_next;
		s_lock_free_handle_data old_back_verify;

		// Read the back handle and the next handle, then read the back handle again
		// If the back handle changed the second time, it means we should retry because something got pushed back
		// Since we always guarantee the existence of one node, old_back.handle will never be null
		old_back.data = m_back.handle;
		old_back_next.data = m_queue[old_back.handle].handle;
		old_back_verify.data = m_back.handle;

		if (old_back.data == old_back_verify.data) {
			if (old_back_next.handle == k_lock_free_invalid_handle) {
				// Back is currently pointing to the true back of the list
				s_lock_free_handle_data new_back_next;
				new_back_next.tag = old_back_next.tag + 1; // Increment operation count to avoid ABA problem
				new_back_next.handle = handle;

				s_aligned_lock_free_handle &next_handle = m_queue[old_back.handle];
				int64 expected = old_back_next.data;
				if (next_handle.handle.compare_exchange_strong(expected, new_back_next.data)) {
					// Exchange successful, try to point back handle to this node
					// It's okay if this fails, because it means back is pointing to a node that was enqueued after this
					// node was enqueued
					s_lock_free_handle_data new_back;
					new_back.tag = old_back.tag + 1;
					new_back.handle = handle;
					expected = old_back.data;
					m_back.handle.compare_exchange_strong(expected, new_back.data);
					return true;
				}
			} else {
				// A node has been tacked onto the back, but the back pointer hasn't been updated yet
				// Try to advance the back pointer and try again. If we don't do this, we would need to wait on another
				// thread to complete this operation, which would effectively be a lock.
				s_lock_free_handle_data new_back;
				new_back.tag = old_back.tag + 1;
				new_back.handle = old_back_next.handle;
				int64 expected = old_back.data;
				m_back.handle.compare_exchange_strong(expected, new_back.data);
			}
		} else {
			// Otherwise, try again
		}
	} while (true);
}

template<typename t_element>
bool c_lock_free_queue<t_element>::pop(t_element &element_out) {
	do {
		s_lock_free_handle_data front;
		s_lock_free_handle_data back;
		s_lock_free_handle_data front_next;
		s_lock_free_handle_data front_verify;

		// Read the front, back, and front next handles, and then the front handle again to check for consistency
		front.data = m_front.handle;
		back.data = m_back.handle;
		front_next.data = m_queue[front.handle].handle;
		front_verify.data = m_front.handle;

		if (front.data == front_verify.data) {
			// Check if queue is empty or back is falling behind
			// Back could fall behind because we update it after we tack on a node
			if (front.handle == back.handle) {
				if (front_next.handle == k_lock_free_invalid_handle) {
					// Queue is actually empty
					return false;
				} else {
					// Back is falling behind, try to advance it and then loop again. If we didn't do this, we would
					// have to rely on another thread to advance, which would effectively be a lock.
					s_lock_free_handle_data new_back;
					new_back.tag = back.tag + 1;
					new_back.handle = front_next.handle;
					m_back.handle.compare_exchange_strong(back.data, new_back.data);
				}
			} else {
				// Copy the value into our output; do this BEFORE we compare_exchange
				// The reason is, we don't actually free this node, we free the node pointed to by front
				// Therefore, another pop operation could come and free this one before we get a chance to copy
				// If the compare_exchange fails, we may have to copy multiple times
				copy_type(&element_out, &m_elements[front_next.handle]);

				// Try to move front to next
				s_lock_free_handle_data new_front;
				new_front.tag = front.tag + 1;
				new_front.handle = front_next.handle;
				int64 expected = front.data;
				if (m_front.handle.compare_exchange_strong(expected, new_front.data)) {
					m_free_list.free(front.handle);
					return true;
				}
			}
		}
	} while (true);
}

template<typename t_element>
bool c_lock_free_queue<t_element>::pop() {
	do {
		s_lock_free_handle_data front;
		s_lock_free_handle_data back;
		s_lock_free_handle_data front_next;
		s_lock_free_handle_data front_verify;

		// Read the front, back, and front next handles, and then the front handle again to check for consistency
		front.data = m_front.handle;
		back.data = m_back.handle;
		front_next.data = m_queue[front.handle].handle;
		front_verify.data = m_front.handle;

		if (front.data == front_verify.data) {
			// Check if queue is empty or back is falling behind
			// Back could fall behind because we update it after we tack on a node
			if (front.handle == back.handle) {
				if (front_next.handle == k_lock_free_invalid_handle) {
					// Queue is actually empty
					return false;
				} else {
					// Back is falling behind, try to advance it and then loop again. If we didn't do this, we would
					// have to rely on another thread to advance, which would effectively be a lock.
					s_lock_free_handle_data new_back;
					new_back.tag = back.tag + 1;
					new_back.handle = front_next.handle;
					int64 expected = back.data;
					m_back.handle.compare_exchange_strong(expected, new_back.data);
				}
			} else {
				// Try to move front to next
				s_lock_free_handle_data new_front;
				new_front.tag = front.tag + 1;
				new_front.handle = front_next.handle;
				int64 expected = front.data;
				if (m_front.handle.compare_exchange_strong(expected, new_front.data)) {
					m_free_list.free(front.handle);
					return true;
				}
			}
		}
	} while (true);
}

template<typename t_element>
bool c_lock_free_queue<t_element>::peek(t_element &element_out) {
	do {
		s_lock_free_handle_data front;
		s_lock_free_handle_data back;
		s_lock_free_handle_data front_next;
		s_lock_free_handle_data front_verify;

		// Read the front, back, and front next handles, and then the front handle again to check for consistency
		front.data = m_front.handle;
		back.data = m_back.handle;
		front_next.data = m_queue[front.handle].handle;
		front_verify.data = m_front.handle;

		if (front.data == front_verify.data) {
			// Check if queue is empty or back is falling behind
			// Back could fall behind because we update it after we tack on a node
			if (front.handle == back.handle) {
				if (front_next.handle == k_lock_free_invalid_handle) {
					// Queue is actually empty
					return false;
				} else {
					// Back is falling behind, try to advance it and then loop again. If we didn't do this, we would
					// have to rely on another thread to advance, which would effectively be a lock.
					s_lock_free_handle_data new_back;
					new_back.tag = back.tag + 1;
					new_back.handle = front_next.handle;
					int64 expected = back.data;
					m_back.handle.compare_exchange_strong(expected, new_back.data);
				}
			} else {
				// Copy the value into our output; we may have to copy multiple times
				copy_type(&element_out, &m_elements[front_next.handle]);

				// If the front handle did not change, then our memcpy was safe
				if (m_front.handle == front.data) {
					return true;
				}
			}
		}
	} while (true);
}