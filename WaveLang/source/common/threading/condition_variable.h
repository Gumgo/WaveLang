#ifndef WAVELANG_CONDITION_VARIABLE_H__
#define WAVELANG_CONDITION_VARIABLE_H__

#include "common/common.h"
#include <condition_variable>

class c_scoped_lock;

class c_condition_variable : private c_uncopyable {
public:
	c_condition_variable();
	~c_condition_variable();

	void wait(c_scoped_lock &scoped_lock);
	void notify_one();
	void notify_all();

private:
	std::condition_variable m_condition_variable;
};

#endif // WAVELANG_MUTEX_H__