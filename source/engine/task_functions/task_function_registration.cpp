#include "task_function_registration_inputs.h"

#include "engine/task_function_registry.h"
#include "engine/task_functions/task_function_registration.h"

#include "execution_graph/native_modules/native_module_registration.h"

#include "task_function/task_function.h"

// Generated native module registration
#define TASK_FUNCTION_REGISTRATION_ENABLED
#include "generated/registration_generated.inl"
#undef TASK_FUNCTION_REGISTRATION_ENABLED

void register_task_functions() {
	c_task_function_registry::begin_registration();

	//bool result = // $TODO $PLUGIN handle failure
	register_task_functions_generated();

	// $TODO plugin task functions would be registered here

	c_task_function_registry::end_registration();
}