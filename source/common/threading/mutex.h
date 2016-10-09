#ifndef WAVELANG_COMMON_THREADING_MUTEX_H__
#define WAVELANG_COMMON_THREADING_MUTEX_H__

#include "common/common.h"

#include <mutex>

class c_mutex : private c_uncopyable {
public:
	c_mutex();
	~c_mutex();

	void lock();
	bool try_lock();
	void unlock();

private:
	friend class c_scoped_lock;
	std::mutex m_mutex;
};

class c_scoped_lock {
public:
	c_scoped_lock(c_mutex &mutex, bool acquire_lock = true);
	~c_scoped_lock();

	void lock();
	bool try_lock();
	void unlock();

private:
	friend class c_condition_variable;
	std::unique_lock<std::mutex> m_unique_lock;
};

#endif // WAVELANG_COMMON_THREADING_MUTEX_H__
