#include "common/string.h"

#include <vector>

bool string_compare_case_insensitive(const char *str_a, const char *str_b) {
	for (size_t index = 0; true; index++) {
		if (str_a[index] == '\0' && str_b[index] == '\0') {
			// Both same length, and we haven't yet found a mismatch
			return true;
		} else if (::tolower(str_a[index]) != ::tolower(str_b[index])) {
			// If either character differs, return false
			// This will also trigger if one string is shorter than the other, because one character will be \0
			return false;
		}
	}
}

bool is_string_empty(const char *str) {
	wl_assert(str);
	return *str == '\0';
}

bool string_starts_with(const char *str, const char *prefix) {
	wl_assert(str);
	wl_assert(prefix);

	const char *str_ptr = str;
	const char *prefix_ptr = prefix;

	while (true) {
		char prefix_char = *prefix_ptr;
		if (prefix_char == '\0') {
			// Reached end of the prefix without finding a mismatch
			return true;
		} else {
			char str_char = *str_ptr;
			if (str_char != prefix_char) {
				// Found a mismatch - accounts for null terminator of str
				return false;
			}
		}

		str_ptr++;
		prefix_ptr++;
	}
}

std::string str_format(const char *format, ...) {
	va_list args;
	va_start(args, format);
	std::string result = str_vformat(format, args);
	va_end(args);
	return result;
}

std::string str_vformat(const char *format, va_list args) {
	int32 required_length = std::vsnprintf(nullptr, 0, format, args);

	// A negative value indicates an encoding error
	wl_assert(required_length >= 0);

	std::vector<char> buffer(static_cast<size_t>(required_length) + 1, 0);

	IF_ASSERTS_ENABLED(int32 length_verify = ) std::vsnprintf(buffer.data(), buffer.size(), format, args);
	wl_assert(required_length == length_verify);

	return std::string(buffer.data());
}
