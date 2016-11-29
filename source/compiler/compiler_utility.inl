inline c_compiler_string::c_compiler_string()
	: m_str(nullptr)
	, m_length(0) {}

inline c_compiler_string::c_compiler_string(const char *str, size_t length)
	: m_str(str)
	, m_length(length) {
	wl_assert(length == 0 || str);
}

inline const char *c_compiler_string::get_str() const {
	return m_str;
}

inline size_t c_compiler_string::get_length() const {
	return m_length;
}

inline bool c_compiler_string::is_empty() const {
	return m_length == 0;
}

inline bool c_compiler_string::operator==(const c_compiler_string &other) const {
	return (m_length == other.m_length) &&
		(memcmp(m_str, other.m_str, m_length) == 0);
}

inline bool c_compiler_string::operator==(const char *other) const {
	// $TODO Could improve implementation to avoid strlen call
	wl_assert(other);
	c_compiler_string other_str(other, strlen(other));
	return *this == other_str;
}

inline bool c_compiler_string::operator!=(const c_compiler_string &other) const {
	return (m_length != other.m_length) ||
		(memcmp(m_str, other.m_str, m_length) != 0);
}

inline bool c_compiler_string::operator!=(const char *other) const {
	// $TODO Could improve implementation to avoid strlen call
	wl_assert(other);
	c_compiler_string other_str(other, strlen(other));
	return *this != other_str;
}

inline char c_compiler_string::operator[](size_t index) const {
	wl_assert(index < m_length);
	return m_str[index];
}

inline c_compiler_string c_compiler_string::advance(size_t count) const {
	wl_assert(count <= m_length);
	return c_compiler_string(m_str + count, m_length - count);
}

inline c_compiler_string c_compiler_string::substr(size_t start, size_t length) const {
	wl_assert(start <= m_length);
	wl_assert(start + length <= m_length);
	return c_compiler_string(m_str + start, length);
}

inline std::string c_compiler_string::to_std_string() const {
	return std::string(m_str, m_str + m_length);
}