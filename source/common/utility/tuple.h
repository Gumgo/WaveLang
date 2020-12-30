#pragma once

#include <tuple>

// There seems to be a bug with MSVC's implementation of std::apply() (an issue with _C_invoke overloads) so here's
// a replacement:

template<typename t_function, typename t_tuple, size_t... k_indices>
decltype(auto) std_apply_internal(t_function &&function, t_tuple &&tuple, std::index_sequence<k_indices...>) {
	return function(std::get<k_indices>(std::forward<t_tuple>(tuple))...);
}

template<typename t_function, typename t_tuple>
decltype(auto) std_apply(t_function &&function, t_tuple &&tuple) {
	return std_apply_internal(
		std::forward<t_function>(function),
		std::forward<t_tuple>(tuple),
		std::make_index_sequence<std::tuple_size_v<std::remove_reference_t<t_tuple>>>());
}

// Calls the provided function on each element of the tuple
template<typename t_tuple, typename t_function>
void tuple_for_each(t_tuple &&tuple, t_function &&function) {
	std_apply(
		[&](auto &&... x) { int32 swallow[] = { (function(x), 0)..., 0 }; (void)swallow; },
		std::forward<t_tuple>(tuple));
}

// Calls the provided function on each element of the tuple and returns a tuple containing the returned values
template<typename t_tuple, typename t_function>
decltype(auto) tuple_apply(t_tuple &&tuple, t_function &&function) {
	return std_apply(
		// We need to be explicit about our tuple type or we will lose references
		[&](auto &&... x) { return std::tuple<decltype(function(x))...>(function(x)...); },
		std::forward<t_tuple>(tuple));
}

template<typename t_tuple_a, typename t_tuple_b, size_t... k_indices>
decltype(auto) tuple_zip_internal(t_tuple_a &&tuple_a, t_tuple_b &&tuple_b, std::index_sequence<k_indices...>) {
	return std::make_tuple(
		std::make_tuple(
			std::get<k_indices>(std::forward<t_tuple_a>(tuple_a)),
			std::get<k_indices>(std::forward<t_tuple_b>(tuple_b)))...);
}

// Zips two tuples together to produce a tuple of tuples
template<typename t_tuple_a, typename t_tuple_b>
decltype(auto) tuple_zip(t_tuple_a &&tuple_a, t_tuple_b &&tuple_b) {
	static constexpr size_t k_size = std::tuple_size_v<t_tuple_a>;
	STATIC_ASSERT(k_size == std::tuple_size_v<t_tuple_b>);
	return tuple_zip_internal(
		std::forward<t_tuple_a>(tuple_a),
		std::forward<t_tuple_b>(tuple_b),
		std::make_index_sequence<k_size>());
}

template<bool k_remove, typename... t_elements>
struct s_remove_leading_element {
	using t_type = std::tuple<t_elements...>;
};

template<typename t_first_element, typename... t_elements>
struct s_remove_leading_element<true, t_first_element, t_elements...> {
	using t_type = std::tuple<t_elements...>;
};

template<bool k_remove, typename... t_elements>
struct s_tuple_remove_leading_element {};

// Returns std::tuple<T...> for std::tuple<T1, T...> if k_remove is true
template<bool k_remove, typename... t_elements>
struct s_tuple_remove_leading_element<k_remove, std::tuple<t_elements...>> {
	using t_type = typename s_remove_leading_element<k_remove, t_elements...>::t_type;
};
