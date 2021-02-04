#pragma once

#include "common/common.h"
#include "common/utility/function_type_info.h"
#include "common/utility/template_string.h"
#include "common/utility/tuple.h"

#include "native_module/native_module.h"

// Functions and objects in this namespace are used to bind native and script types
namespace native_module_binding {

	// Maps script types to native types
	template<
		e_native_module_argument_direction k_argument_direction,
		e_native_module_primitive_type k_primitive_type,
		bool k_is_array,
		e_native_module_data_access k_data_access>
	struct s_native_module_native_type_mapping {
		// The default implementation is invalid, we specialize for each supported type
		STATIC_ASSERT_MSG(k_argument_direction != k_argument_direction, "Unsupported native module type");
	};

	struct s_parse_script_type_string_result {
		bool is_return = false;
		e_native_module_argument_direction argument_direction = e_native_module_argument_direction::k_invalid;
		e_native_module_primitive_type primitive_type = e_native_module_primitive_type::k_invalid;
		bool is_array = false;
		uint32 upsample_factor = 0;
		e_native_module_data_mutability data_mutability = e_native_module_data_mutability::k_invalid;
		e_native_module_data_access data_access = e_native_module_data_access::k_invalid;
	};

	// Parses a string that describes a script type. These strings take the form:
	// [return] argument_direction [ref] [data_mutability] primitive_type [upsample_factor] [[]]
	// Examples include:
	// in real
	// out const string[]
	// in ref const? bool
	// in real@2x
	// return out real[]
	template<size_t k_length>
	constexpr s_parse_script_type_string_result parse_script_type_string(const char (&script_string)[k_length]) {
		struct s_context {
			const char *script_string;
			size_t token_start = 0;
			size_t token_end = 0;

			constexpr s_context(const char *str)
				: script_string(str) {
			}

			constexpr bool read_token() {
				// Simple token parser - combine alphanumerics
				token_start = token_end;

				while (true) {
					char c = script_string[token_start];
					if (c == ' ' || c == '\t') {
						token_start++;
					} else {
						break;
					}
				}

				token_end = token_start;
				bool is_alphanumeric = false;
				while (true) {
					char c = script_string[token_end];
					if (c == ' ' || c == '\t' || c == '\0') {
						break;
					} else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
						// Keep looping until we find a non-alphanumeric character
						is_alphanumeric = true;
						token_end++;
					} else {
						// Symbol - return it on its own or break if we were parsing an alphanumeric
						if (!is_alphanumeric) {
							token_end++;
						}

						break;
					}
				}

				return true;
			}

			constexpr bool compare_token(const char *test_value) const {
				size_t length = token_end - token_start;
				for (size_t index = 0; index < length; index++) {
					if (test_value[index] != script_string[token_start + index]) {
						return false;
					}
				}

				return test_value[length] == '\0';
			}
		};

		s_parse_script_type_string_result result{};
		s_context context(script_string);

		context.read_token();
		if (context.compare_token("return")) {
			result.is_return = true;
			context.read_token();
		}

		if (context.compare_token("in")) {
			if (result.is_return) {
				CONSTEXPR_ERROR();
			}

			result.argument_direction = e_native_module_argument_direction::k_in;
		} else if (context.compare_token("out")) {
			result.argument_direction = e_native_module_argument_direction::k_out;
		} else {
			CONSTEXPR_ERROR();
		}

		context.read_token();
		if (context.compare_token("ref")) {
			result.data_access = e_native_module_data_access::k_reference;
			context.read_token();
		} else {
			result.data_access = e_native_module_data_access::k_value;
		}

		if (context.compare_token("const")) {
			context.read_token();
			if (context.compare_token("?")) {
				result.data_mutability = e_native_module_data_mutability::k_dependent_constant;
				context.read_token();
			} else {
				result.data_mutability = e_native_module_data_mutability::k_constant;
			}
		} else {
			result.data_mutability = e_native_module_data_mutability::k_variable;
		}

		if (context.compare_token("real")) {
			result.primitive_type = e_native_module_primitive_type::k_real;
		} else if (context.compare_token("bool")) {
			result.primitive_type = e_native_module_primitive_type::k_bool;
		} else if (context.compare_token("string")) {
			result.primitive_type = e_native_module_primitive_type::k_string;
		} else {
			CONSTEXPR_ERROR();
		}

		context.read_token();
		if (context.compare_token("@")) {
			if (result.data_mutability == e_native_module_data_mutability::k_constant) {
				CONSTEXPR_ERROR();
			}

			context.read_token();
			if (context.token_start == context.token_end || context.script_string[context.token_end - 1] != 'x') {
				CONSTEXPR_ERROR();
			}

			// Parse integer to read upsample factor
			uint32 upsample_factor = 0;
			for (size_t index = context.token_start; index < context.token_end - 1; index++) {
				char c = context.script_string[index];
				if (c == '0' && index == context.token_start) {
					CONSTEXPR_ERROR();
				} else if (c >= '0' && c <= '9') {
					uint32 value = c - '0';
					uint32 new_upsample_factor = upsample_factor * 10 + value;
					if (new_upsample_factor < upsample_factor) {
						CONSTEXPR_ERROR();
					}
				} else {
					CONSTEXPR_ERROR();
				}
			}

			result.upsample_factor = upsample_factor;
			context.read_token();
		} else {
			result.upsample_factor = 1;
		}

		if (context.compare_token("[")) {
			context.read_token();
			if (!context.compare_token("]")) {
				CONSTEXPR_ERROR();
			}

			result.is_array = true;
			context.read_token();
		}

		if (!context.compare_token("")) {
			CONSTEXPR_ERROR();
		}

		return result;
	}

#define TYPE_MAPPING(																						\
	argument_direction,																						\
	primitive_type,																							\
	is_array,																								\
	data_access,																							\
	native_type,																							\
	get_argument_function)																					\
	template<>																								\
	struct s_native_module_native_type_mapping<																\
		e_native_module_argument_direction::argument_direction,												\
		e_native_module_primitive_type::primitive_type,														\
		is_array,																							\
		e_native_module_data_access::data_access> {															\
		using t_native_type = native_type;																	\
																											\
		static t_native_type get_argument(const s_native_module_context &context, size_t argument_index) {	\
			return context.arguments[argument_index].get_argument_function();								\
		}																									\
	}

	// Table mapping script types to native types
	TYPE_MAPPING(k_in, k_real, false, k_value, real32, get_real_in);
	TYPE_MAPPING(k_in, k_real, false, k_reference, c_native_module_real_reference, get_real_reference_in);
	TYPE_MAPPING(k_out, k_real, false, k_value, real32 &, get_real_out);
	TYPE_MAPPING(k_out, k_real, false, k_reference, c_native_module_real_reference &, get_real_reference_out);
	TYPE_MAPPING(k_in, k_bool, false, k_value, bool, get_bool_in);
	TYPE_MAPPING(k_in, k_bool, false, k_reference, c_native_module_bool_reference, get_bool_reference_in);
	TYPE_MAPPING(k_out, k_bool, false, k_value, bool &, get_bool_out);
	TYPE_MAPPING(k_out, k_bool, false, k_reference, c_native_module_bool_reference &, get_bool_reference_out);
	TYPE_MAPPING(k_in, k_string, false, k_value, const c_native_module_string &, get_string_in);
	TYPE_MAPPING(k_in, k_string, false, k_reference, c_native_module_string_reference, get_string_reference_in);
	TYPE_MAPPING(k_out, k_string, false, k_value, c_native_module_string &, get_string_out);
	TYPE_MAPPING(k_out, k_string, false, k_reference, c_native_module_string_reference &, get_string_reference_out);
	TYPE_MAPPING(k_in, k_real, true, k_value, const c_native_module_real_array &, get_real_array_in);
	TYPE_MAPPING(k_in, k_real, true, k_reference, const c_native_module_real_reference_array &, get_real_reference_array_in);
	TYPE_MAPPING(k_out, k_real, true, k_value, c_native_module_real_array &, get_real_array_out);
	TYPE_MAPPING(k_out, k_real, true, k_reference, c_native_module_real_reference_array &, get_real_reference_array_out);
	TYPE_MAPPING(k_in, k_bool, true, k_value, const c_native_module_bool_array &, get_bool_array_in);
	TYPE_MAPPING(k_in, k_bool, true, k_reference, const c_native_module_bool_reference_array &, get_bool_reference_array_in);
	TYPE_MAPPING(k_out, k_bool, true, k_value, c_native_module_bool_array &, get_bool_array_out);
	TYPE_MAPPING(k_out, k_bool, true, k_reference, c_native_module_bool_reference_array &, get_bool_reference_array_out);
	TYPE_MAPPING(k_in, k_string, true, k_value, const c_native_module_string_array &, get_string_array_in);
	TYPE_MAPPING(k_in, k_string, true, k_reference, const c_native_module_string_reference_array &, get_string_reference_array_in);
	TYPE_MAPPING(k_out, k_string, true, k_value, c_native_module_string_array &, get_string_array_out);
	TYPE_MAPPING(k_out, k_string, true, k_reference, c_native_module_string_reference_array &, get_string_reference_array_out);

#undef TYPE_MAPPING

	template<
		e_native_module_argument_direction k_argument_direction,
		e_native_module_primitive_type k_primitive_type,
		bool k_is_array,
		uint32 k_upsample_factor,
		e_native_module_data_mutability k_data_mutability,
		e_native_module_data_access k_data_access,
		bool k_is_return,
		typename t_name>
	struct s_native_module_bound_argument {
		static constexpr e_native_module_argument_direction k_argument_direction = k_argument_direction;
		static constexpr e_native_module_primitive_type k_primitive_type = k_primitive_type;
		static constexpr bool k_is_array = k_is_array;
		static constexpr uint32 k_upsample_factor = k_upsample_factor;
		static constexpr e_native_module_data_mutability k_data_mutability = k_data_mutability;
		static constexpr e_native_module_data_access k_data_access = k_data_access;
		static constexpr bool k_is_return = k_is_return;
		static constexpr const char *k_name = t_name::k_value;

		// $TODO we should find a way to enforce that k_data_mutability is constant for strings. Currently, primitive
		// type traits aren't constexpr so we can't STATIC_ASSERT on those.

		using t_type_mapping = s_native_module_native_type_mapping<
			k_argument_direction,
			k_primitive_type,
			k_is_array,
			k_data_access>;
		using t_native_type = typename t_type_mapping::t_native_type;

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
	struct s_native_module_type_info {
		using t_function_type_info = s_function_type_info<t_function>;
		using t_return_type = typename t_function_type_info::t_return_type;
		using t_argument_types = typename t_function_type_info::t_argument_types;

		STATIC_ASSERT_MSG(
			(std::is_same_v<t_return_type, t_expected_return_type>),
			"Incorrect native module return type");

		static constexpr bool k_first_argument_is_context =
			std::tuple_size_v<t_argument_types> > 0
			&& std::is_same_v<std::tuple_element_t<0, t_argument_types>, const s_native_module_context &>;
		using t_script_argument_types =
			typename s_tuple_remove_leading_element<k_first_argument_is_context, t_argument_types>::t_type;
		static constexpr size_t k_script_argument_count = std::tuple_size_v<t_script_argument_types>;
	};

	using s_argument_index_map = s_static_array<size_t, k_max_native_module_arguments>;

	// k_argument_index_map: not all task function calls necessarily use all arguments (e.g. the memory query). This
	// structure maps the function call arguments to the arguments defined in s_native_module
	template<auto k_function, typename t_return_type, const s_argument_index_map *k_argument_index_map = nullptr>
	t_return_type native_module_call_wrapper(const s_native_module_context &context) {
		using t_function_pointer = std::remove_reference_t<decltype(k_function)> *;
		using t_info = s_native_module_type_info<t_function_pointer, t_return_type>;

		// Call the correct s_native_module_context function for each argument and store the results as a tuple
		static constexpr auto k_argument_indices =
			build_argument_indices(std::make_index_sequence<t_info::k_script_argument_count>());
		auto script_argument_values = tuple_apply(
			k_argument_indices,
			[&](auto argument_index) {
				// argument_index is of type s_argument_index, so grab the index
				static constexpr size_t k_argument_index = decltype(argument_index)::k_index;

				// t_argument is of type s_native_module_bound_argument. Grab the type mapping.
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
	void populate_native_module_arguments(s_native_module &native_module) {
		using t_function_pointer = std::remove_reference_t<t_function> *;
		using t_info = s_native_module_type_info<t_function_pointer, t_return_type>;

		static constexpr auto k_argument_indices =
			build_argument_indices(std::make_index_sequence<t_info::k_script_argument_count>());
		native_module.argument_count = t_info::k_script_argument_count;
		native_module.return_argument_index = k_invalid_native_module_argument_index;
		tuple_for_each(
			k_argument_indices,
			[&](auto argument_index) {
				// argument_index is of type s_argument_index, so grab the index
				static constexpr size_t k_argument_index = decltype(argument_index)::k_index;

				// t_argument is of type s_native_module_bound_argument. Grab the type mapping.
				using t_argument = std::tuple_element_t<k_argument_index, typename t_info::t_script_argument_types>;

				s_native_module_argument &argument = native_module.arguments[k_argument_index];
				argument.name.set_verify(t_argument::k_name);
				argument.argument_direction = t_argument::k_argument_direction;
				argument.type = c_native_module_qualified_data_type(
					c_native_module_data_type(
						t_argument::k_primitive_type,
						t_argument::k_is_array,
						t_argument::k_upsample_factor),
					t_argument::k_data_mutability);
				argument.data_access = t_argument::k_data_access;

				if (t_argument::k_is_return) {
					wl_assert(native_module.return_argument_index == k_invalid_native_module_argument_index);
					native_module.return_argument_index = k_argument_index;
				}
			});
	}

}

#define NATIVE_MODULE_BOUND_ARGUMENT(script_type_string, name)										\
	native_module_binding::s_native_module_bound_argument<											\
		native_module_binding::parse_script_type_string(#script_type_string).argument_direction,	\
		native_module_binding::parse_script_type_string(#script_type_string).primitive_type,		\
		native_module_binding::parse_script_type_string(#script_type_string).is_array,				\
		native_module_binding::parse_script_type_string(#script_type_string).upsample_factor,		\
		native_module_binding::parse_script_type_string(#script_type_string).data_mutability,		\
		native_module_binding::parse_script_type_string(#script_type_string).data_access,			\
		native_module_binding::parse_script_type_string(#script_type_string).is_return,				\
		TEMPLATE_STRING(#name)>

#define wl_argument(script_type_string, name) NATIVE_MODULE_BOUND_ARGUMENT(script_type_string, name) name
