#pragma once

#include "common/asserts.h"
#include "common/types.h"

#include <limits>
#include <type_traits>

template<typename t_to, typename t_from, bool to_unsigned, bool from_unsigned>
struct s_cast_integer_verify_internal {};

template<typename t_to, typename t_from>
struct s_cast_integer_verify_internal<t_to, t_from, false, false> {
	static void verify(t_from value) {
#if IS_TRUE(ASSERTS_ENABLED)
		static const size_t k_larger_size = STATIC_MAX(sizeof(t_to), sizeof(t_from));
		typedef typename s_integer_type<k_larger_size, false>::type t_larger_type;
		static const t_larger_type k_to_min = std::numeric_limits<t_to>::min();
		static const t_larger_type k_to_max = std::numeric_limits<t_to>::max();
		wl_assert(value >= k_to_min && value <= k_to_max);
#endif // IS_TRUE(ASSERTS_ENABLED)
	}
};

template<typename t_to, typename t_from>
struct s_cast_integer_verify_internal<t_to, t_from, true, true> {
	static void verify(t_from value) {
#if IS_TRUE(ASSERTS_ENABLED)
		static const size_t k_larger_size = STATIC_MAX(sizeof(t_to), sizeof(t_from));
		typedef typename s_integer_type<k_larger_size, true>::type t_larger_type;
		static const t_larger_type k_to_min = std::numeric_limits<t_to>::min();
		static const t_larger_type k_to_max = std::numeric_limits<t_to>::max();
		wl_assert(value >= k_to_min);
		wl_assert(value <= k_to_max);
#endif // IS_TRUE(ASSERTS_ENABLED)
	}
};

template<typename t_to, typename t_from>
struct s_cast_integer_verify_internal<t_to, t_from, false, true> {
	static void verify(t_from value) {
#if IS_TRUE(ASSERTS_ENABLED)
		static const size_t k_larger_size = STATIC_MAX(sizeof(t_to), sizeof(t_from));
		// We're casting from an unsigned type to a signed type, so we only need to check that value <= to_max. The
		// comparison type we use is unsigned, because signed max will always fit in an unsigned int of >= size
		typedef typename s_integer_type<k_larger_size, true>::type t_larger_type;
		static const t_larger_type k_to_max = static_cast<t_larger_type>(std::numeric_limits<t_to>::max());
		wl_assert(value <= k_to_max);
#endif // IS_TRUE(ASSERTS_ENABLED)
	}
};

template<typename t_to, typename t_from>
struct s_cast_integer_verify_internal<t_to, t_from, true, false> {
	static void verify(t_from value) {
#if IS_TRUE(ASSERTS_ENABLED)
		static const size_t k_larger_size = STATIC_MAX(sizeof(t_to), sizeof(t_from));
		// We're casting from a signed type to an unsigned type, so we need to check >= 0 and also check that value <=
		// to_max. The comparison type we use is unsigned, because signed max will always fit in an unsigned int of >=
		// size.
		typedef typename s_integer_type<k_larger_size, true>::type t_larger_type;
		static const t_larger_type k_to_max = std::numeric_limits<t_to>::max();
		wl_assert(value >= 0);
		// Tested >= 0 first, so it is safe to now cast to unsigned for the max comparison
		wl_assert(static_cast<t_larger_type>(value) <= k_to_max);
#endif // IS_TRUE(ASSERTS_ENABLED)
	}
};

// Safe integer conversion functions
template<typename t_to, typename t_from> t_to cast_integer_verify(t_from value) {
	static_assert(std::is_integral<t_to>::value, "t_to is not an integer");
	static_assert(std::is_integral<t_from>::value, "t_from is not an integer");
	static const bool k_to_unsigned = std::is_unsigned<t_to>::value;
	static const bool k_from_unsigned = std::is_unsigned<t_from>::value;

	s_cast_integer_verify_internal<t_to, t_from, k_to_unsigned, k_from_unsigned>::verify(value);
	return static_cast<t_to>(value);
}

