#pragma once

#include "common/common.h"

#include <condition_variable>

class c_scoped_lock;

class c_condition_variable {
public:
	c_condition_variable();
	UNCOPYABLE(c_condition_variable);
	~c_condition_variable();

	void wait(c_scoped_lock &scoped_lock);
	void wait(c_scoped_lock &scoped_lock, uint32 timeout_ms);
	void notify_one();
	void notify_all();

private:
	std::condition_variable m_condition_variable;
};

