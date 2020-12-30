#include "engine/task_function_registration.h"

namespace stream_task_functions {

	static constexpr uint32 k_stream_library_id = 8;
	wl_task_function_library(k_stream_library_id, "stream", 0);

	// No runtime task functions, currently

	wl_end_active_library_task_function_registration();

}
