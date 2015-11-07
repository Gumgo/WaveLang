#include "engine/task_functions/task_function_registration.h"
#include "engine/task_function_registry.h"

#include "engine/task_functions/task_functions_basic.h"
#include "engine/task_functions/task_functions_utility.h"
#include "engine/task_functions/task_functions_sampler.h"
#include "engine/task_functions/task_functions_delay.h"
#include "engine/task_functions/task_functions_filter.h"
#include "engine/task_functions/task_functions_time.h"

void register_task_functions() {
	c_task_function_registry::begin_registration();

	register_task_functions_basic();
	register_task_functions_utility();
	register_task_functions_sampler();
	register_task_functions_delay();
	register_task_functions_filter();
	register_task_functions_time();

	// $TODO plugin task functions would be registered here

	c_task_function_registry::end_registration();
}