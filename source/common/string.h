#pragma once

#include "common/asserts.h"
#include "common/macros.h"
#include "common/types.h"

#include <cstdarg>
#include <cstring>
#include <string>
#include <string_view>

bool string_compare_case_insensitive(const char *str_a, const char *str_b);
bool is_string_empty(const char *str);
bool string_starts_with(const char *str, const char *prefix);

std::string str_format(const char *format, ...);
std::string str_vformat(const char *format, va_list args);

template<size_t k_buffer_size>
class c_static_string {
public:
	STATIC_ASSERT_MSG(k_buffer_size > 0, "c_static_string must have a nonzero buffer size");

	static c_static_string<k_buffer_size> construct_empty() {
		c_static_string<k_buffer_size> result;
		result.clear();
		return result;
	}

	static c_static_string<k_buffer_size> construct_truncate(const char *str) {
		c_static_string<k_buffer_size> result;
		result.set_truncate(str);
		return result;
	}

	static c_static_string<k_buffer_size> construct_verify(const char *str) {
		c_static_string<k_buffer_size> result;
		result.set_verify(str);
		return result;
	}

	void clear() {
		m_buffer[0] = '\0';
	}

	void set_truncate(const char *str) {
		clear();
		append_truncate(str);
	}

	void set_verify(const char *str) {
		clear();
		append_verify(str);
	}

	void append_truncate(const char *str) {
		size_t offset = get_length();
		size_t index;
		for (index = 0; str[index] != '\0' && offset + index + 1 < k_buffer_size; index++) {
			m_buffer[offset + index] = str[index];
		}

		// offset + index will either be at the end of str, or at the end of the buffer
		m_buffer[offset + index] = '\0';
	}

	void append_verify(const char *str) {
		size_t offset = get_length();
		size_t index;
		for (index = 0; str[index] != '\0'; index++) {
			// Make sure we have space for a null terminator
			wl_assert(offset + index + 1 < k_buffer_size);
			m_buffer[offset + index] = str[index];
		}

		// offset + index will be at the end of str
		m_buffer[offset + index] = '\0';
	}

	void truncate_to_length(size_t new_length) {
		if (new_length >= k_buffer_size) {
			return;
		}

		// If the string is already shorter, this will have no effect
		m_buffer[new_length] = '\0';
	}

	const char *get_string() const {
		return m_buffer;
	}

	// Returns non-const pointer to the string - if using this function, be careful of buffer overruns!
	char *get_string_unsafe() {
		return m_buffer;
	}

	size_t get_length() const {
		return strlen(m_buffer);
	}

	size_t get_max_length() const {
		return k_buffer_size - 1;
	}

	size_t get_buffer_size() const {
		return k_buffer_size;
	}

	bool is_empty() const {
		return m_buffer[0] == '\0';
	}

	char operator[](size_t index) const {
		return m_buffer[index];
	}

	char &operator[](size_t index) {
		return m_buffer[index];
	}

	template<size_t k_other_buffer_size>
	bool operator==(const c_static_string<k_other_buffer_size> &other) const {
		return strcmp(get_string(), other.get_string()) == 0;
	}

	bool operator==(const char *str) const {
		return strcmp(get_string(), str) == 0;
	}

	bool operator==(const std::string &str) const {
		return strcmp(get_string(), str.c_str()) == 0;
	}

	bool operator==(const std::string_view &str) const {
		return str.compare(m_buffer) == 0;
	}

	template<size_t k_other_buffer_size>
	bool operator!=(const c_static_string<k_other_buffer_size> &other) const {
		return strcmp(get_string(), other.get_string()) != 0;
	}

	bool operator!=(const char *str) const {
		return strcmp(get_string(), str) != 0;
	}

	bool operator!=(const std::string &str) const {
		return strcmp(get_string(), str.c_str()) != 0;
	}

	bool operator!=(const std::string_view &str) const {
		return str.compare(m_buffer) != 0;
	}

	// Makes sure the string has a null terminator
	void validate() const {
#if IS_TRUE(ASSERTS_ENABLED)
		bool null_terminator_found = false;
		for (size_t index = 0; !null_terminator_found && index < k_buffer_size; index++) {
			if (m_buffer[index] == '\0') {
				null_terminator_found = true;
			}
		}

		wl_assert(null_terminator_found);
#endif // IS_TRUE(ASSERTS_ENABLED)
	}

private:
	char m_buffer[k_buffer_size];
};

