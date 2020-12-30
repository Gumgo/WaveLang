#pragma once

#include "common/common.h"

// TEMPLATE_STRING("hello") produces a type with a static field k_value containing the string "hello"
// The implementation uses macro hacks and will be improved with C++20

template<char... k_chars>
struct s_template_string {
	static constexpr size_t k_length = sizeof...(k_chars) - 1;
	static constexpr char k_value[] = { k_chars... };
};

// k_char_count is the number of non-null terminator characters
// k_chars has at least 1 null terminator
template<size_t k_char_count, char... k_chars>
struct s_template_string_untrimmed {
	STATIC_ASSERT(k_char_count < sizeof...(k_chars));

	template<size_t... k_indices>
	static constexpr auto get_trimmed(std::index_sequence<k_indices...>) {
		static constexpr char k_all_chars[] = { k_chars... };
		return s_template_string<k_all_chars[k_indices]...>();
	}

	using t_template_string = decltype(get_trimmed(std::make_index_sequence<k_char_count + 1>()));
};

template<size_t k_count>
constexpr size_t constexpr_string_length(const char(&str)[k_count]) {
	for (size_t i = 0; i < k_count; i++) {
		if (str[i] == '\0') {
			return i;
		}
	}

	CONSTEXPR_ERROR();
	return 0;
}

#define TEMPLATE_STRING_1(str, offset) (offset) < (sizeof(str) - 1) ? str[offset] : '\0',
#define TEMPLATE_STRING_2(str, offset) TEMPLATE_STRING_1(str, offset) TEMPLATE_STRING_1(str, offset + 1)
#define TEMPLATE_STRING_4(str, offset) TEMPLATE_STRING_2(str, offset) TEMPLATE_STRING_2(str, offset + 2)
#define TEMPLATE_STRING_8(str, offset) TEMPLATE_STRING_4(str, offset) TEMPLATE_STRING_4(str, offset + 4)
#define TEMPLATE_STRING_16(str, offset) TEMPLATE_STRING_8(str, offset) TEMPLATE_STRING_8(str, offset + 8)
#define TEMPLATE_STRING_32(str, offset) TEMPLATE_STRING_16(str, offset) TEMPLATE_STRING_16(str, offset + 16)
#define TEMPLATE_STRING_64(str, offset) TEMPLATE_STRING_32(str, offset) TEMPLATE_STRING_32(str, offset + 32)
#define TEMPLATE_STRING_128(str, offset) TEMPLATE_STRING_64(str, offset) TEMPLATE_STRING_64(str, offset + 64)
#define TEMPLATE_STRING_256(str, offset) TEMPLATE_STRING_128(str, offset) TEMPLATE_STRING_128(str, offset + 128)

#define TEMPLATE_STRING(str)									\
	typename s_template_string_untrimmed<						\
		constexpr_string_length(str),							\
		TEMPLATE_STRING_256(str, 0) '\0'>::t_template_string
