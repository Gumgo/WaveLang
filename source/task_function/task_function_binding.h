#pragma once

#include "common/common.h"
#include "common/utility/function_type_info.h"
#include "common/utility/template_string.h"
#include "common/utility/tuple.h"

#include "task_function/task_function.h"

// Functions and objects in this namespace are used to bind task functions and associate them with native modules
namespace task_function_binding {

	// Maps native type to 
	template<typename t_native_type>
	struct s_task_function_native_type_mapping {
		// The default implementation is invalid, we specialize for each supported type
		STATIC_ASSERT_MSG((!std::is_same_v<t_native_type, t_native_type>), "Unsupported task function type");
	};

#define TYPE_MAPPING(																						\
	argument_direction,																						\
	primitive_type,																							\
	is_array,																								\
	data_mutability,																						\
	native_type,																							\
	get_argument_function)																					\
	template<>																								\
	struct s_task_function_native_type_mapping<native_type> {												\
		using t_native_type = native_type;																	\
		static constexpr e_task_argument_direction k_argument_direction =									\
			e_task_argument_direction::argument_direction;													\
		static constexpr e_task_primitive_type k_primitive_type =											\
			e_task_primitive_type::primitive_type;															\
		static constexpr bool k_is_array = is_array;														\
		static constexpr e_task_data_mutability k_data_mutability =											\
			e_task_data_mutability::data_mutability;														\
																											\
		static t_native_type get_argument(const s_task_function_context &context, size_t argument_index) {	\
			return context.arguments[argument_index].get_argument_function();								\
		}																									\
	}

	// Table mapping native types to task types
	TYPE_MAPPING(k_in, k_real, false, k_constant, real32, get_real_constant_in);
	TYPE_MAPPING(k_in, k_real, false, k_variable, const c_real_buffer *, get_real_buffer_in);
	TYPE_MAPPING(k_out, k_real, false, k_variable, c_real_buffer *, get_real_buffer_out);
	TYPE_MAPPING(k_in, k_bool, false, k_constant, bool, get_real_constant_in);
	TYPE_MAPPING(k_in, k_bool, false, k_variable, const c_bool_buffer *, get_bool_buffer_in);
	TYPE_MAPPING(k_out, k_bool, false, k_variable, c_bool_buffer *, get_bool_buffer_out);
	TYPE_MAPPING(k_in, k_string, false, k_constant, const char *, get_string_constant_in);
	TYPE_MAPPING(k_in, k_real, true, k_constant, c_real_constant_array, get_real_constant_array_in);
	TYPE_MAPPING(k_in, k_real, true, k_variable, c_real_buffer_array_in, get_real_buffer_array_in);
	TYPE_MAPPING(k_in, k_bool, true, k_constant, c_bool_constant_array, get_bool_constant_array_in);
	TYPE_MAPPING(k_in, k_bool, true, k_variable, c_bool_buffer_array_in, get_bool_buffer_array_in);
	TYPE_MAPPING(k_in, k_string, true, k_constant, c_string_constant_array, get_string_constant_array_in);

#undef TYPE_MAPPING

	template<typename t_native_type_param, uint32 k_upsample_factor, typename t_name, bool k_is_unshared>
		struct s_task_function_bound_argument {
		using t_native_type = t_native_type_param;
		using t_type_mapping = s_task_function_native_type_mapping<t_native_type>;
		static constexpr e_task_argument_direction k_argument_direction = t_type_mapping::k_argument_direction;
		static constexpr e_task_primitive_type k_primitive_type = t_type_mapping::k_primitive_type;
		static constexpr bool k_is_array = t_type_mapping::k_is_array;
		static constexpr uint32 k_upsample_factor = k_upsample_factor;
		static constexpr e_task_data_mutability k_data_mutability = t_type_mapping::k_data_mutability;
		static constexpr const char *k_name = t_name::k_value;
		static constexpr bool k_is_unshared = k_is_unshared;

		t_native_type value;

		t_native_type operator*() const {
			return value;
		}

		std::remove_pointer_t<std::remove_reference_t<t_native_type>> *operator->() {
			if constexpr (std::is_pointer_v<t_native_type>) {
				return value;
			} else {
				return &value;
			}
		}

		const std::remove_pointer_t<std::remove_reference_t<t_native_type>> *operator->() const {
			if constexpr (std::is_pointer_v<t_native_type>) {
				return value;
			} else {
				return &value;
			}
		}

		operator t_native_type() const {
			return value;
		}
	};

	template<size_t k_argument_index>
	struct s_argument_index {
		static constexpr size_t k_index = k_argument_index;
	};

	template<size_t... k_indices>
	constexpr auto build_argument_indices(std::index_sequence<k_indices...>) {
		return std::make_tuple(s_argument_index<k_indices>()...);
	}

	template<typename t_function, typename t_expected_return_type>
	struct s_task_function_type_info {
		using t_function_type_info = s_function_type_info<t_function>;
		using t_return_type = typename t_function_type_info::t_return_type;
		using t_argument_types = typename t_function_type_info::t_argument_types;

		STATIC_ASSERT_MSG(
			(std::is_same_v<t_return_type, t_expected_return_type>),
			"Incorrect task function return type");

		static constexpr bool k_first_argument_is_context =
			std::tuple_size_v<t_argument_types> > 0
			&& std::is_same_v<std::tuple_element_t<0, t_argument_types>, const s_task_function_context &>;
		using t_script_argument_types =
			typename s_tuple_remove_leading_element<k_first_argument_is_context, t_argument_types>::t_type;
		static constexpr size_t k_script_argument_count = std::tuple_size_v<t_script_argument_types>;
	};

	using s_argument_index_map = s_static_array<size_t, k_max_task_function_arguments>;
	using s_task_function_argument_names = s_static_array<const char *, k_max_task_function_arguments>;

	// k_argument_index_map: not all task function calls necessarily use all arguments (e.g. the memory query). This
	// structure maps the function call arguments to the arguments defined in s_task_function
	template<auto k_function, typename t_return_type, const s_argument_index_map *k_argument_index_map = nullptr>
	t_return_type task_function_call_wrapper(const s_task_function_context &context) {
		using t_function_pointer = std::remove_reference_t<decltype(k_function)> *;
		using t_info = s_task_function_type_info<t_function_pointer, t_return_type>;

		// Call the correct s_task_function_context function for each argument and store the results as a tuple
		static constexpr auto k_argument_indices =
			build_argument_indices(std::make_index_sequence<t_info::k_script_argument_count>());
		auto script_argument_values = tuple_apply(
			k_argument_indices,
			[&](auto argument_index) {
				// argument_index is of type s_argument_index, so grab the index
				static constexpr size_t k_argument_index = decltype(argument_index)::k_index;

				// t_argument is of type s_task_function_bound_argument. Grab the type mapping.
				using t_argument = std::tuple_element_t<k_argument_index, typename t_info::t_script_argument_types>;
				using t_type_mapping = typename t_argument::t_type_mapping;
				size_t mapped_argument_index;
				if constexpr (k_argument_index_map) {
					mapped_argument_index = (*k_argument_index_map)[k_argument_index];
				} else {
					mapped_argument_index = k_argument_index;
				}
				return t_argument{ t_type_mapping::get_argument(context, mapped_argument_index) };
			});

		if constexpr (t_info::k_first_argument_is_context) {
			return std::apply(k_function, std::tuple_cat(std::make_tuple(context), script_argument_values));
		} else {
			return std::apply(k_function, script_argument_values);
		}
	}

	template<typename t_function, typename t_return_type>
	void populate_task_function_arguments(
		s_task_function &task_function,
		s_task_function_argument_names &argument_names) {
		using t_function_pointer = std::remove_reference_t<t_function> *;
		using t_info = s_task_function_type_info<t_function_pointer, t_return_type>;

		static constexpr auto k_argument_indices =
			build_argument_indices(std::make_index_sequence<t_info::k_script_argument_count>());
		task_function.argument_count = t_info::k_script_argument_count;
		tuple_for_each(
			k_argument_indices,
			[&](auto argument_index) {
				// argument_index is of type s_argument_index, so grab the index
				static constexpr size_t k_argument_index = decltype(argument_index)::k_index;

				// t_argument is of type s_task_function_bound_argument. Grab the type mapping.
				using t_argument = std::tuple_element_t<k_argument_index, typename t_info::t_script_argument_types>;

				s_task_function_argument &argument = task_function.arguments[k_argument_index];
				argument.argument_direction = t_argument::k_argument_direction;
				argument.type = c_task_qualified_data_type(
					c_task_data_type(
						t_argument::k_primitive_type,
						t_argument::k_is_array,
						t_argument::k_upsample_factor),
					t_argument::k_data_mutability);
				argument.is_unshared = t_argument::k_is_unshared;
				argument_names[k_argument_index] = t_argument::k_name;
			});
	}

}

#define TASK_FUNCTION_BOUND_ARGUMENT(native_type, upsample_factor, name, is_unshared)	\
	task_function_binding::s_task_function_bound_argument<								\
		native_type,																	\
		upsample_factor,																\
		TEMPLATE_STRING(#name),															\
		is_unshared>

#define wl_task_argument(native_type, name)							\
	TASK_FUNCTION_BOUND_ARGUMENT(native_type, 1, name, false) name

#define wl_task_argument_unshared(native_type, name)				\
	TASK_FUNCTION_BOUND_ARGUMENT(native_type, 1, name, true) name

#define wl_task_argument_upsampled(native_type, upsample_factor, name)				\
	TASK_FUNCTION_BOUND_ARGUMENT(native_type, upsample_factor, name, false) name

#define wl_task_argument_upsampled_unshared(native_type, upsample_factor, name)	\
	TASK_FUNCTION_BOUND_ARGUMENT(native_type, upsample_factor, name, true) name
