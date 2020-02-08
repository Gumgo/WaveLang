#pragma once

#include "common/common.h"

#include "engine/buffer_handle.h"

class c_buffer;
class c_executor;

class c_array_dereference_interface {
public:
	c_array_dereference_interface(c_executor *executor);
	const c_buffer *get_buffer(h_buffer buffer_handle) const;

private:
	c_executor *m_executor;
};

