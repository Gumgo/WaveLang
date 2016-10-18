#ifndef WAVELANG_ENGINE_ARRAY_DEREFERENCE_INTERFACE_ARRAY_DEREFERENCE_INTERFACE_H__
#define WAVELANG_ENGINE_ARRAY_DEREFERENCE_INTERFACE_ARRAY_DEREFERENCE_INTERFACE_H__

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

#endif // WAVELANG_ENGINE_ARRAY_DEREFERENCE_INTERFACE_ARRAY_DEREFERENCE_INTERFACE_H__
