#ifndef WAVELANG_RUNTIME_CONTEXT_H__
#define WAVELANG_RUNTIME_CONTEXT_H__

#include "common/common.h"
#include "driver/driver_interface.h"

struct s_runtime_context {
	c_driver_interface driver_interface;
};

#endif // WAVELANG_RUNTIME_CONTEXT_H__