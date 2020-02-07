#pragma once

#include "common/common.h"

class c_buffer;
class c_executor;

class c_array_dereference_interface {
public:
	c_array_dereference_interface(c_executor *executor);
	const c_buffer *get_buffer_by_index(uint32 buffer_index) const;

private:
	c_executor *m_executor;
};

