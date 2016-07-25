#include "engine/task_functions/task_functions_basic.h"
#include "execution_graph/native_modules/native_modules_basic.h"
#include "engine/task_function_registry.h"
#include "engine/buffer.h"
#include "engine/buffer_operations/buffer_operations.h"
#include "engine/buffer_operations/buffer_operations_internal.h"

static const uint32 k_task_functions_basic_library_id = 0;

static const s_task_function_uid k_task_function_negation_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 0);
static const s_task_function_uid k_task_function_negation_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 1);

static const s_task_function_uid k_task_function_addition_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 2);
static const s_task_function_uid k_task_function_addition_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 3);

static const s_task_function_uid k_task_function_subtraction_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 10);
static const s_task_function_uid k_task_function_subtraction_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 11);
static const s_task_function_uid k_task_function_subtraction_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 12);

static const s_task_function_uid k_task_function_multiplication_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 20);
static const s_task_function_uid k_task_function_multiplication_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 21);

static const s_task_function_uid k_task_function_division_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 30);
static const s_task_function_uid k_task_function_division_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 31);
static const s_task_function_uid k_task_function_division_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 32);

static const s_task_function_uid k_task_function_modulo_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 40);
static const s_task_function_uid k_task_function_modulo_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 41);
static const s_task_function_uid k_task_function_modulo_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 42);

static const s_task_function_uid k_task_function_not_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 50);
static const s_task_function_uid k_task_function_not_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 51);

static const s_task_function_uid k_task_function_real_equal_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 60);

static const s_task_function_uid k_task_function_real_not_equal_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 70);

static const s_task_function_uid k_task_function_bool_equal_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 80);
static const s_task_function_uid k_task_function_bool_equal_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 81);
static const s_task_function_uid k_task_function_bool_equal_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 82);

static const s_task_function_uid k_task_function_bool_not_equal_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 90);
static const s_task_function_uid k_task_function_bool_not_equal_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 91);
static const s_task_function_uid k_task_function_bool_not_equal_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 92);

static const s_task_function_uid k_task_function_greater_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 100);

static const s_task_function_uid k_task_function_less_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 110);

static const s_task_function_uid k_task_function_greater_equal_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 120);

static const s_task_function_uid k_task_function_less_equal_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 130);

static const s_task_function_uid k_task_function_and_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 140);
static const s_task_function_uid k_task_function_and_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 141);
static const s_task_function_uid k_task_function_and_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 142);

static const s_task_function_uid k_task_function_or_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 150);
static const s_task_function_uid k_task_function_or_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 151);
static const s_task_function_uid k_task_function_or_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 152);

static const s_task_function_uid k_task_function_real_select_in_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 160);
static const s_task_function_uid k_task_function_real_select_in_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 161);
static const s_task_function_uid k_task_function_real_select_in_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 162);

static const s_task_function_uid k_task_function_bool_select_in_in_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 170);
static const s_task_function_uid k_task_function_bool_select_inout_in_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 171);
static const s_task_function_uid k_task_function_bool_select_in_inout_in_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 172);
static const s_task_function_uid k_task_function_bool_select_in_in_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 173);

static const s_task_function_uid k_task_function_real_array_dereference_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 180);
static const s_task_function_uid k_task_function_real_array_dereference_inout_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 181);

static const s_task_function_uid k_task_function_bool_array_dereference_in_out_uid = s_task_function_uid::build(k_task_functions_basic_library_id, 190);

#include "engine/task_functions/task_functions_basic_real.inl"
#include "engine/task_functions/task_functions_basic_bool.inl"
#include "engine/task_functions/task_functions_basic_select.inl"
#include "engine/task_functions/task_functions_basic_array.inl"

void register_task_functions_basic() {
	register_task_functions_basic_real();
	register_task_functions_basic_bool();
	register_task_functions_basic_select();
	register_task_functions_basic_array();

	// nocheckin select operations
}