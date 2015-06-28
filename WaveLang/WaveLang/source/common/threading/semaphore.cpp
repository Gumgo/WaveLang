#include "common/threading/semaphore.h"

c_semaphore::c_semaphore() {
}

c_semaphore::~c_semaphore() {
}

void c_semaphore::notify(uint32 count) {
	wl_assert(count > 0);

	if (count == 1) {
		c_scoped_lock lock(m_mutex);
		m_count++;
		m_condition_variable.notify_one();
	} else {
		c_scoped_lock lock(m_mutex);
		m_count += count;
		m_condition_variable.notify_all();
	}
}

void c_semaphore::wait() {
	c_scoped_lock lock(m_mutex);

	while (m_count == 0) {
		m_condition_variable.wait(lock);
	}

	m_count--;
}