#include "engine/array_dereference_interface/array_dereference_interface.h"
#include "engine/executor/executor.h"

c_array_dereference_interface::c_array_dereference_interface(c_executor *executor)
	: m_executor(executor) {
	wl_assert(executor);
}

const c_buffer *c_array_dereference_interface::get_buffer(h_buffer buffer_handle) const {
	return m_executor->get_buffer(buffer_handle);
}
