#include "common/threading/mutex.h"

c_mutex::c_mutex() {}

c_mutex::~c_mutex() {}

void c_mutex::lock() {
	m_mutex.lock();
}

bool c_mutex::try_lock() {
	return m_mutex.try_lock();
}

void c_mutex::unlock() {
	m_mutex.unlock();
}

c_scoped_lock::c_scoped_lock(c_mutex &mutex, bool acquire_lock) {
	if (acquire_lock) {
		m_unique_lock = std::unique_lock<std::mutex>(mutex.m_mutex);
	} else {
		m_unique_lock = std::unique_lock<std::mutex>(mutex.m_mutex, std::defer_lock);
	}
}

c_scoped_lock::~c_scoped_lock() {
	// m_unique_lock's destructor will do everything
}

void c_scoped_lock::lock() {
	m_unique_lock.lock();
}

bool c_scoped_lock::try_lock() {
	return m_unique_lock.try_lock();
}

void c_scoped_lock::unlock() {
	m_unique_lock.unlock();
}