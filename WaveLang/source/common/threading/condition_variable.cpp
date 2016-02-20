#include "common/threading/condition_variable.h"
#include "common/threading/mutex.h"

c_condition_variable::c_condition_variable() {
}

c_condition_variable::~c_condition_variable() {
}

void c_condition_variable::wait(c_scoped_lock &scoped_lock) {
	m_condition_variable.wait(scoped_lock.m_unique_lock);
}

void c_condition_variable::wait(c_scoped_lock &scoped_lock, uint32 timeout_ms) {
	m_condition_variable.wait_for(scoped_lock.m_unique_lock, std::chrono::milliseconds(timeout_ms));
}

void c_condition_variable::notify_one() {
	m_condition_variable.notify_one();
}

void c_condition_variable::notify_all() {
	m_condition_variable.notify_all();
}