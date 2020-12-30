#pragma once

#include "common/common.h"
#include "common/utility/tuple.h"

template<typename t_return_type, typename... t_argument_types>
std::tuple<t_return_type, t_argument_types...> extract_function_type_info(
	t_return_type (*function)(t_argument_types...));

// t_function should be a function pointer type
template<typename t_function>
struct s_function_type_info {
	using t_return_and_argument_types = decltype(extract_function_type_info(t_function(nullptr)));
	using t_return_type = std::tuple_element_t<0, t_return_and_argument_types>;
	using t_argument_types = typename s_tuple_remove_leading_element<true, t_return_and_argument_types>::t_type;
};
