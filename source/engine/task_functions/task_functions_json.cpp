#include "engine/task_function_registration.h"

namespace json_task_functions {

	void scrape_task_functions() {
		static constexpr uint32 k_json_library_id = 9;
		wl_task_function_library(k_json_library_id, "json", 0);

		// No runtime task functions, currently

		wl_end_active_library_task_function_registration();
	}

}
