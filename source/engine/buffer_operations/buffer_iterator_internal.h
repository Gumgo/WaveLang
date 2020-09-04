#pragma once

#include "engine/buffer_operations/buffer_iterator_types.h"

#include <tuple>

namespace buffer_iterator_internal {
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
			[&](auto &&... x) { int32 swallow[] = { (function(x), 0)..., 0 }; (void) swallow; },
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

	template<typename t_iterators>
	decltype(auto) get_iterands(t_iterators &&iterators) {
		return tuple_apply(
			std::forward<t_iterators>(iterators),
			// We need to be explicit about the return type or we lose references
			[](auto &&iter) -> decltype(iter.get()) { return iter.get(); });
	}

	// Intermediate buffer iteration function which contains the final iteration logic
	template<
		size_t k_stride,
		bool k_cico,
		typename t_function,
		typename t_last_iteration_function,
		typename t_iterators>
		bool iterate_buffers_internal(
			t_function &&function,
			t_last_iteration_function &&last_iteration_function,
			size_t count,
			bool all_constant,
			t_iterators &&iterators) {
		static_assert(std::tuple_size_v<std::remove_reference_t<t_iterators>> > 0, "No iterators provided");

		// Determine whether we'll have trailing unaligned samples at the end
		constexpr bool k_has_last_iteration_function = !std::is_same_v<t_last_iteration_function, std::nullptr_t>;

		if (k_cico && all_constant) {
			// If we support the constant-in, constant-out case and all our input buffers are constant, just iterate
			// once and mark all output buffers as constant.
			auto iterator_arguments = get_iterands(iterators);
			auto arguments = std::tuple_cat(std::make_tuple<size_t>(0), std::move(iterator_arguments));
			if constexpr (k_has_last_iteration_function) {
				// Handle the case where we should call our last iteration function because the total iteration count is
				// less than the stride
				if (count < k_stride) {
					std_apply(std::forward<t_last_iteration_function>(last_iteration_function), std::move(arguments));
				} else {
					std_apply(std::forward<t_function>(function), std::move(arguments));
				}
			} else {
				std_apply(std::forward<t_function>(function), std::move(arguments));
			}
			tuple_for_each(iterators, [](auto &&iter) { iter.advance(); });
			tuple_for_each(iterators, [](auto &&iter) { iter.iteration_finished(); });
			tuple_for_each(std::forward<t_iterators>(iterators), [](auto &&iter) { iter.set_is_constant(true); });
			return true;
		} else {
			size_t iteration_limit;
			if constexpr (k_has_last_iteration_function) {
				// Run the last iteration function on the final iteration
				iteration_limit = (count / k_stride) * k_stride;
			} else {
				iteration_limit = count;
			}

			size_t i;
			bool keep_iterating = true;
			for (i = 0; keep_iterating && i < iteration_limit; i += k_stride) {
				// We don't use std::forward on the function inputs because they're being reused each loop iteration
				auto iterator_arguments = get_iterands(iterators);
				auto arguments = std::tuple_cat(std::make_tuple(i), std::move(iterator_arguments));

				constexpr bool k_function_returns_bool =
					std::is_same_v<bool, decltype(std_apply(function, std::move(arguments)))>;
				if constexpr (k_function_returns_bool) {
					keep_iterating = std_apply(function, std::move(arguments));
				} else {
					std_apply(function, std::move(arguments));
				}

				tuple_for_each(iterators, [](auto &&iter) { iter.advance(); });
			}

			if constexpr (k_has_last_iteration_function) {
				if (keep_iterating && iteration_limit < count) {
					// Run the last iteration if we have remaining samples
					auto iterator_arguments = get_iterands(iterators);
					auto arguments = std::tuple_cat(std::make_tuple(i), std::move(iterator_arguments));
					std_apply(std::forward<t_last_iteration_function>(last_iteration_function), std::move(arguments));
					tuple_for_each(iterators, [](auto &&iter) { iter.advance(); });
				}
			}

			tuple_for_each(iterators, [](auto &&iter) { iter.iteration_finished(); });
			tuple_for_each(std::forward<t_iterators>(iterators), [](auto &&iter) { iter.set_is_constant(false); });

			return false;
		}
	}

	// Intermediate buffer iteration function which distinguishes between constant and non-constant buffers
	template<
		size_t k_stride,
		bool k_cico,
		typename t_function,
		typename t_last_iteration_function,
		typename t_iterators,
		typename t_buffer,
		typename... t_remaining_buffers>
	bool iterate_buffers_internal(
		t_function &&function,
		t_last_iteration_function &&last_iteration_function,
		size_t count,
		bool all_constant,
		t_iterators &&iterators,
		t_buffer &&buffer,
		t_remaining_buffers &&... remaining_buffers) {
		using t_buffer_decayed = std::decay_t<t_buffer>;
		// If t_buffer is not convertible to a non-const pointer, it must be a non-const pointer and therefore an output
		if constexpr (std::is_convertible_v<t_buffer, c_buffer *>) {
			// Output buffers - might be constant if it's shared with an input buffer, but never compile-time constant
			wl_assert(!buffer->is_compile_time_constant());
			c_buffer_iterator<k_stride, e_buffer_iterator_type::k_output, t_buffer_decayed> iterator(
				std::forward<t_buffer>(buffer));
			auto appended_iterators = std::tuple_cat(
				std::forward<t_iterators>(iterators),
				std::make_tuple(std::move(iterator)));
			return iterate_buffers_internal<k_stride, k_cico>(
				std::forward<t_function>(function),
				std::forward<t_last_iteration_function>(last_iteration_function),
				count,
				all_constant,
				std::move(appended_iterators),
				std::forward<t_remaining_buffers>(remaining_buffers)...);
		} else {
			// Input buffers
			if (buffer->is_constant()) {
				// This buffer contains a constant value
				c_buffer_iterator<k_stride, e_buffer_iterator_type::k_constant, t_buffer_decayed> iterator(
					std::forward<t_buffer>(buffer));
				auto appended_iterators = std::tuple_cat(
					std::forward<t_iterators>(iterators),
					std::make_tuple(std::move(iterator)));
				return iterate_buffers_internal<k_stride, k_cico>(
					std::forward<t_function>(function),
					std::forward<t_last_iteration_function>(last_iteration_function),
					count,
					all_constant,
					std::move(appended_iterators),
					std::forward<t_remaining_buffers>(remaining_buffers)...);
			} else {
				// This buffer contains non-constant data
				c_buffer_iterator<k_stride, e_buffer_iterator_type::k_input, t_buffer_decayed> iterator(
					std::forward<t_buffer>(buffer));
				auto appended_iterators = std::tuple_cat(
					std::forward<t_iterators>(iterators),
					std::make_tuple(std::move(iterator)));
				return iterate_buffers_internal<k_stride, k_cico>(
					std::forward<t_function>(function),
					std::forward<t_last_iteration_function>(last_iteration_function),
					count,
					false,
					std::move(appended_iterators),
					std::forward<t_remaining_buffers>(remaining_buffers)...);
			}
		}
	}

	// This function swaps the order of the function and buffer arguments so that the buffer arguments come last. This
	// allows them to be represented using a parameter pack.
	template<
		size_t k_stride,
		bool k_cico,
		size_t... k_function_sequence,
		size_t... k_buffers_sequence,
		typename... t_args>
	bool move_buffers_to_end_2(
		size_t count,
		std::index_sequence<k_function_sequence...>,
		std::index_sequence<k_buffers_sequence...>,
		std::tuple<t_args...> args) {
		if constexpr (sizeof...(k_function_sequence) == 1) {
			// No last iteration function was provided, so provide a fake placeholder one which will be ignored
			// Note: it's safe to call std::move twice on args because it only "moves" the provided indices
			return iterate_buffers_internal<k_stride, k_cico>(
				std::get<k_function_sequence>(std::move(args))...,
				nullptr, // Placeholder for last iteration function
				count,
				true,
				std::make_tuple(),
				std::get<k_buffers_sequence>(std::move(args))...);
		} else {
			static_assert(sizeof...(k_function_sequence) == 2, "Unexpected number of functions provided");
			// Note: it's safe to call std::move twice on args because it only "moves" the provided indices
			return iterate_buffers_internal<k_stride, k_cico>(
				std::get<k_function_sequence>(std::move(args))...,
				count,
				true,
				std::make_tuple(),
				std::get<k_buffers_sequence>(std::move(args))...);
		}
	}

	template<typename t_type>
	constexpr bool is_buffer() {
		return std::is_convertible_v<t_type, const c_buffer *>;
	}

	// We want to call iterate_buffers and provide the buffers before the function, but parameter pack types can only be
	// deduced if they come at the end of the template argument list. Therefore, we just bundle buffers and functions
	// into a single parameter pack, "args". This function rearranges the arguments so that the buffers come last and
	// are contained in their own parameter pack.
	template<size_t k_stride, bool k_cico, typename... t_args>
	bool move_buffers_to_end(size_t count, t_args &&... args) {
		// Create tuple of bools representing whether each argument is a buffer
		constexpr auto k_args_are_buffers = std::tuple(is_buffer<t_args>()...);

		// All but the last argument or the last two arguments should be buffers
		constexpr size_t k_args_count = std::tuple_size<decltype(k_args_are_buffers)>::value;
		static_assert(k_args_count >= 1 && !std::get<k_args_count - 1>(k_args_are_buffers), "No function provided");

		constexpr bool k_has_last_iteration_function = k_args_count >= 2 && !std::get<k_args_count - 2>(k_args_are_buffers);
		constexpr size_t k_function_count = k_has_last_iteration_function ? 2 : 1;

		if constexpr (k_function_count == 1) {
			return move_buffers_to_end_2<k_stride, k_cico>(
				count,
				std::index_sequence<k_args_count - 1>(),
				std::make_index_sequence<k_args_count - 1>(),
				std::forward_as_tuple(std::forward<t_args>(args)...));
		} else {
			static_assert(k_function_count == 2, "Unexpected number of functions provided");
			return move_buffers_to_end_2<k_stride, k_cico>(
				count,
				std::index_sequence<k_args_count - 2, k_args_count - 1>(),
				std::make_index_sequence<k_args_count - 2>(),
				std::forward_as_tuple(std::forward<t_args>(args)...));
		}
	}
}
