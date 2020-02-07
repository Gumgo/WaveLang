#ifndef WAVELANG_COMMON_THREADING_SEMAPHORE_H__
#define WAVELANG_COMMON_THREADING_SEMAPHORE_H__

#include "common/common.h"
#include "common/threading/condition_variable.h"
#include "common/threading/mutex.h"

class c_semaphore {
public:
	c_semaphore();
	UNCOPYABLE(c_semaphore);
	~c_semaphore();

	void notify(uint32 count = 1);
	void wait();

private:
	c_mutex m_mutex;
	c_condition_variable m_condition_variable;
	uint32 m_count;
};

#endif // WAVELANG_COMMON_THREADING_SEMAPHORE_H__
