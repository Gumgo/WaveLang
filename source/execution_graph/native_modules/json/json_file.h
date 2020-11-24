#pragma once

#include "common/common.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>

// Note: we could perform 2 parsing passes to reserve memory up front

enum class e_json_node_type {
	k_null,
	k_number,
	k_string,
	k_boolean,
	k_array,
	k_object,

	k_count
};

class c_json_node {
public:
	e_json_node_type get_type() const {
		return m_type;
	}

	template<typename t_node>
	t_node &get_as() {
		wl_assert(m_type == t_node::k_type);
		return static_cast<t_node &>(*this);
	}

	template<typename t_node>
	const t_node &get_as() const {
		wl_assert(m_type == t_node::k_type);
		return static_cast<const t_node &>(*this);
	}

	template<typename t_node>
	t_node *try_get_as() {
		return (m_type == t_node::k_type) ? static_cast<t_node *>(this) : nullptr;
	}

	template<typename t_node>
	const t_node *try_get_as() const {
		return (m_type == t_node::k_type) ? static_cast<const t_node *>(this) : nullptr;
	}

protected:
	c_json_node(e_json_node_type type)
		: m_type(type) {}

private:
	e_json_node_type m_type;
};

template<e_json_node_type k_node_type>
class c_json_typed_node : public c_json_node {
public:
	static const e_json_node_type k_type = k_node_type;

protected:
	c_json_typed_node()
		: c_json_node(k_type) {}
};

class c_json_node_null : public c_json_typed_node<e_json_node_type::k_null> {
public:
	c_json_node_null() {}
};

class c_json_node_number : public c_json_typed_node<e_json_node_type::k_number> {
public:
	c_json_node_number(real64 value)
		: m_value(value) {}

	real64 get_value() const {
		return m_value;
	}

private:
	real64 m_value;
};

// $TODO $UNICODE
class c_json_node_string : public c_json_typed_node<e_json_node_type::k_string> {
public:
	c_json_node_string(const char *value)
		: m_value(value) {}

	const char *get_value() const {
		return m_value.c_str();
	}

private:
	std::string m_value;
};

class c_json_node_boolean : public c_json_typed_node<e_json_node_type::k_boolean> {
public:
	c_json_node_boolean(bool value)
		: m_value(value) {}

	bool get_value() const {
		return m_value;
	}

private:
	bool m_value;
};

class c_json_node_array : public c_json_typed_node<e_json_node_type::k_array> {
public:
	c_json_node_array() {}

	// Takes ownership of element_node
	void add_element(c_json_node *element_node) {
		m_elements.emplace_back(std::move(element_node));
		m_pointers.push_back(element_node);
	}

	c_wrapped_array<const c_json_node *const> get_value() const {
		return c_wrapped_array<const c_json_node *const>(m_pointers);
	}

private:
	std::vector<std::unique_ptr<c_json_node>> m_elements;
	std::vector<const c_json_node *> m_pointers; // Raw pointers for get_value()
};

class c_json_node_object : public c_json_typed_node<e_json_node_type::k_object> {
public:
	c_json_node_object() {}

	// Returns false if an element with the given name already exists; takes ownership of element_node on success
	bool add_element(const char *name, c_json_node *element_node) {
		if (m_element_lookup.find(name) != m_element_lookup.end()) {
			return false;
		}

		// When m_names grows beyond its current capacity, all elements are moved into the newly allocated memory. Even
		// with move constructors, we still have to rebuild our string views because std::string may use short string
		// optimization, so moving a string may still copy memory to the new location.
		bool rebuild = m_names.size() == m_names.capacity();
		m_names.push_back(name);
		m_elements.emplace_back(std::unique_ptr<c_json_node>(element_node));
		if (rebuild) {
			m_element_lookup.clear();
			for (size_t index = 0; index < m_names.size(); index++) {
				std::string_view key(m_names[index].c_str());
				m_element_lookup.insert(std::make_pair(key, m_elements[index].get()));
			}
		} else {
			std::string_view key(m_names.back().c_str());
			m_element_lookup.insert(std::make_pair(key, element_node));
		}

		return true;
	}

	size_t get_count() const {
		return m_elements.size();
	}

	const c_json_node *get_element(const char *name) const {
		std::string_view key(name);
		auto result = m_element_lookup.find(key);
		return result == m_element_lookup.end() ? nullptr : result->second;
	}

private:
	std::vector<std::string> m_names; // Used to avoid allocation inside of get_element()
	std::vector<std::unique_ptr<c_json_node>> m_elements;
	std::unordered_map<std::string_view, c_json_node *> m_element_lookup;
};

enum class e_json_result {
	k_success,
	k_file_error,
	k_parse_error,

	k_count
};

struct s_json_result {
	e_json_result result;
	uint32 parse_error_line;
	uint32 parse_error_character;
};

class c_json_file {
public:
	c_json_file() = default;
	UNCOPYABLE_MOVABLE(c_json_file);

	s_json_result load(const char *filename);
	s_json_result parse(const char *buffer);
	const c_json_node *get_root() const;

private:
	struct s_buffer_with_offset {
		const char *buffer;
		size_t offset;
		uint32 line;
		uint32 character;

		void initialize_file_offset() {
			line = 1;
			character = 1;
		}

		char current_character() const {
			return buffer[offset];
		}

		void increment() {
			if (current_character() == '\n') {
				line++;
				character = 1;
			} else {
				character++;
			}

			offset++;
		}
	};

	// See https://www.json.org/ for the parse tree
	c_json_node *parse(s_buffer_with_offset &buffer_with_offset) const;
	bool parse_whitespace(s_buffer_with_offset &buffer_with_offset) const;
	c_json_node *parse_value(s_buffer_with_offset &buffer_with_offset) const;
	c_json_node_number *parse_number(s_buffer_with_offset &buffer_with_offset) const;
	c_json_node_string *parse_string(s_buffer_with_offset &buffer_with_offset) const;
	c_json_node_array *parse_array(s_buffer_with_offset &buffer_with_offset) const;
	c_json_node_object *parse_object(s_buffer_with_offset &buffer_with_offset) const;

	std::unique_ptr<c_json_node> m_root;
};

// Looks up a node based on a string path. Strings are of the form a.b[i].c. Returns null on failure. Examples:
// - "" return the root node
// - "a" returns the element "a" of the object root node
// - "[2]" returns the element at index 2 of the array root node
// - "a.b[2]" returns the element at index 2 of the element "a" of the root node
const c_json_node *resolve_json_node_path(const c_json_node *root_node, const char *node_path);
