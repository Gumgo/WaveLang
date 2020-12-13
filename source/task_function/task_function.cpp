#include "task_function/task_function.h"

const s_task_function_uid s_task_function_uid::k_invalid = s_task_function_uid::build(0xffffffff, 0xffffffff);

bool validate_task_function(const s_task_function &task_function) {
	for (size_t argument_index = 0; argument_index < task_function.argument_count; argument_index++) {
		const s_task_function_argument &argument = task_function.arguments[argument_index];

		if (!argument.type.is_legal()) {
			return false;
		}

		if (argument.argument_direction == e_task_argument_direction::k_out
			&& argument.type.get_data_mutability() != e_task_data_mutability::k_constant) {
			return false;
		}
	}

	return true;
}
