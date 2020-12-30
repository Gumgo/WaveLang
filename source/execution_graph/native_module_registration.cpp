#include "common/utility/reporting.h"

#include "execution_graph/native_module_registration.h"
#include "execution_graph/native_module_registry.h"

template<typename t_type>
static void reverse_linked_list(t_type *&list_head);

class c_optimization_rule_generator {
public:
	c_optimization_rule_generator(
		const char *rule_string,
		const std::unordered_map<std::string_view, s_native_module_uid> &native_module_identifier_map);
	bool build();
	const s_native_module_optimization_rule &get_rule() const;

private:
	enum class e_token_type {
		k_invalid,
		k_end,
		k_identifier,
		k_real_constant,
		k_bool_constant,
		k_const,
		k_left_parenthesis,
		k_right_parenthesis,
		k_comma,
		k_source_to_target
	};

	struct s_token {
		e_token_type type;
		std::string_view source;

		union {
			real32 real_constant;
			bool bool_constant;
		};

		s_token(e_token_type token_type, const std::string_view &token_source) {
			type = token_type;
			source = token_source;
			real_constant = 0.0f;
		}

		s_token(real32 real_constant_value, const std::string_view &token_source) {
			type = e_token_type::k_real_constant;
			source = token_source;
			real_constant = real_constant_value;
		}

		s_token(bool bool_constant_value, const std::string_view &token_source) {
			type = e_token_type::k_bool_constant;
			source = token_source;
			bool_constant = bool_constant_value;
		}
	};

	struct s_identifier {
		size_t index;
		bool is_constant;
	};

	s_token get_next_token();
	bool add_symbol(s_native_module_optimization_symbol symbol, bool is_source_symbol);

	s_native_module_optimization_symbol get_identifier_symbol(
		const std::string_view &identifier,
		bool is_constant,
		bool is_building_source);

	s_native_module_optimization_rule m_rule{};
	size_t m_source_symbol_count = 0;
	size_t m_target_symbol_count = 0;

	const char *m_rule_string = nullptr;
	size_t m_rule_string_offset = 0;
	const std::unordered_map<std::string_view, s_native_module_uid> &m_native_module_identifier_map;

	std::unordered_map<std::string_view, s_identifier> m_identifier_map;
	uint32 m_constant_count = 0;
	uint32 m_variable_count = 0;
};

s_native_module_library_registration_entry *&s_native_module_library_registration_entry::registration_list() {
	static s_native_module_library_registration_entry *s_value = nullptr;
	return s_value;
}

s_native_module_library_registration_entry *&s_native_module_library_registration_entry::active_library() {
	static s_native_module_library_registration_entry *s_value = nullptr;
	return s_value;
}

void s_native_module_library_registration_entry::end_active_library_native_module_registration() {
	wl_assert(active_library());
	active_library() = nullptr;
}

void c_native_module_registration_utilities::validate_argument_names(const s_native_module &native_module) {
#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t index_a = 0; index_a < native_module.argument_count; index_a++) {
		for (size_t index_b = index_a + 1; index_b < native_module.argument_count; index_b++) {
			wl_assert(
				strcmp(
					native_module.arguments[index_a].name.get_string(),
					native_module.arguments[index_b].name.get_string()) != 0);
		}
	}
#endif // IS_TRUE(ASSERTS_ENABLED)
}

void c_native_module_registration_utilities::map_arguments(
	const s_native_module &source_native_module,
	const s_native_module &mapped_native_module,
	native_module_binding::s_argument_index_map &argument_index_map) {
	for (size_t i = 0; i < argument_index_map.get_count(); i++) {
		argument_index_map[i] = k_invalid_native_module_argument_index;
	}

	for (size_t mapped_argument_index = 0;
		mapped_argument_index < mapped_native_module.argument_count;
		mapped_argument_index++) {
		const s_native_module_argument &mapped_argument = mapped_native_module.arguments[mapped_argument_index];

		for (size_t source_argument_index = 0;
			source_argument_index < source_native_module.argument_count;
			source_argument_index++) {
			const s_native_module_argument &source_argument = source_native_module.arguments[source_argument_index];

			bool match = strcmp(mapped_argument.name.get_string(), source_argument.name.get_string()) == 0;
			if (match) {
				wl_assert(mapped_argument.argument_direction == source_argument.argument_direction);
				wl_assert(mapped_argument.type == source_argument.type);
				wl_assert(mapped_argument.data_access == source_argument.data_access);

				argument_index_map[mapped_argument_index] = source_argument_index;
				break;
			}
		}

		// Argument not found
		wl_assert(argument_index_map[mapped_argument_index] != k_invalid_native_module_argument_index);
	}
}

void c_native_module_registration_utilities::get_name_from_identifier(
	const char *identifier,
	c_static_string<k_max_native_module_name_length> &name_out) {
	// Replace any $ with a null terminator to remove additional characters
	for (size_t index = 0; index < name_out.get_max_length(); index++) {
		if (identifier[index] == '\0') {
			name_out.set_verify(identifier);
			return;
		} else if (identifier[index] == '$') {
			memcpy(name_out.get_string_unsafe(), identifier, index);
			name_out.get_string_unsafe()[index] = '\0';
			return;
		}
	}

	wl_halt(); // Name is too long
}

std::unordered_map<std::string_view, s_native_module_uid>
c_native_module_registration_utilities::build_native_module_identifier_map(uint32 library_id, uint32 library_version) {
	std::unordered_map<std::string_view, s_native_module_uid> map;

	s_native_module_library_registration_entry *library_entry =
		s_native_module_library_registration_entry::registration_list();
	while (library_entry) {
		if (library_entry->library.id == library_id) {
			wl_assert(library_entry->library.version == library_version);

			s_native_module_registration_entry *native_module_entry = library_entry->native_modules;
			while (native_module_entry) {
				map.insert(std::make_pair(native_module_entry->identifier, native_module_entry->native_module.uid));
				native_module_entry = native_module_entry->next;
			}

			break;
		}

		library_entry = library_entry->next;
	}

	return map;
}

bool register_native_modules() {
	wl_assert(!s_native_module_library_registration_entry::active_library());

	c_native_module_registry::begin_registration();

	// $TODO $PLUGIN iterate plugin DLLs to call "register_native_modules" on each one

	// The registration lists get built in reverse order, so flip them
	reverse_linked_list(s_native_module_library_registration_entry::registration_list());

	s_native_module_library_registration_entry *library_entry =
		s_native_module_library_registration_entry::registration_list();
	while (library_entry) {
		reverse_linked_list(library_entry->native_modules);
		reverse_linked_list(library_entry->optimization_rules);

		if (!c_native_module_registry::register_native_module_library(library_entry->library)) {
			return false; // Error already reported
		}

		s_native_module_registration_entry *native_module_entry = library_entry->native_modules;
		while (native_module_entry) {
			if (!c_native_module_registry::register_native_module(native_module_entry->native_module)) {
				return false; // Error already reported
			}

			native_module_entry = native_module_entry->next;
		}

		std::unordered_map<std::string_view, s_native_module_uid> native_module_identifier_map =
			c_native_module_registration_utilities::build_native_module_identifier_map(
				library_entry->library.id,
				library_entry->library.version);

		s_native_module_optimization_rule_registration_entry *optimization_rule_entry =
			library_entry->optimization_rules;
		while (optimization_rule_entry) {
			c_optimization_rule_generator generator(
				optimization_rule_entry->optimization_rule_string,
				native_module_identifier_map);
			if (!generator.build()) {
				report_error(
					"Failed to register optimization rule '%s' because it contains errors",
					optimization_rule_entry->optimization_rule_string);
				return false;
			}

			if (!c_native_module_registry::register_optimization_rule(generator.get_rule())) {
				return false; // Error already reported
			}

			native_module_entry = native_module_entry->next;
		}

		library_entry = library_entry->next;
	}

	return c_native_module_registry::end_registration();
}

template<typename t_type>
static void reverse_linked_list(t_type *&list_head) {
	t_type *current = list_head;
	t_type *previous = nullptr;
	while (current) {
		t_type *next = current->next;
		current->next = previous;
		if (!next) {
			list_head = current;
		}

		previous = current;
		current = next;
	}
}

c_optimization_rule_generator::c_optimization_rule_generator(
	const char *rule_string,
	const std::unordered_map<std::string_view, s_native_module_uid> &native_module_identifier_map)
	: m_native_module_identifier_map(native_module_identifier_map) {
	m_rule_string = rule_string;

	for (s_native_module_optimization_symbol &symbol : m_rule.source.symbols) {
		symbol = s_native_module_optimization_symbol::invalid();
	}

	for (s_native_module_optimization_symbol &symbol : m_rule.target.symbols) {
		symbol = s_native_module_optimization_symbol::invalid();
	}
}

const s_native_module_optimization_rule &c_optimization_rule_generator::get_rule() const {
	return m_rule;
}

bool c_optimization_rule_generator::build() {
	// Note: this function doesn't test for all error cases and can definitely produce invalid rules. It can also
	// probably produce valid rules from invalid syntax. I'm not too worried about it for now though, since we'll
	// validate the rules when they're registered at runtime, which is what matters most.
	zero_type(&m_rule);

	bool building_source = true;
	int32 depth = 0;
	bool expect_comma_or_rparen = false;
	bool expect_source_to_target = false;
	bool expect_end = false;

	s_token next_token = get_next_token(); // 1-token lookahead
	while (true) {
		s_token token = next_token;
		next_token = get_next_token();

		if (expect_comma_or_rparen) {
			expect_comma_or_rparen = false;
			if (token.type == e_token_type::k_comma) {
				continue;
			} else if (token.type != e_token_type::k_right_parenthesis) {
				return false;
			}
		}

		if (expect_source_to_target) {
			expect_source_to_target = false;
			wl_assert(depth == 0);
			wl_assert(building_source);
			if (token.type != e_token_type::k_source_to_target) {
				return false;
			}

			building_source = false;
			continue;
		}

		if (expect_end) {
			expect_end = false;
			wl_assert(depth == 0);
			wl_assert(!building_source);
			if (token.type != e_token_type::k_end) {
				return false;
			}

			break;
		}

		bool is_const = false;
		if (token.type == e_token_type::k_const) {
			if (next_token.type != e_token_type::k_identifier) {
				// Only identifiers can have the const qualifier
				return false;
			}

			is_const = true;
			token = next_token;
			next_token = get_next_token();
		}

		if (token.type == e_token_type::k_identifier) {
			if (next_token.type == e_token_type::k_left_parenthesis) {
				// Native module call
				if (is_const) {
					// Native modules can't be const
					return false;
				}

				auto iter = m_native_module_identifier_map.find(next_token.source);
				if (iter == m_native_module_identifier_map.end()) {
					return false;
				}

				s_native_module_optimization_symbol native_module_symbol =
					s_native_module_optimization_symbol::build_native_module(iter->second);
				if (!add_symbol(native_module_symbol, building_source)) {
					return false;
				}

				depth++;
			} else {
				s_native_module_optimization_symbol identifier_symbol =
					get_identifier_symbol(token.source, is_const, building_source);
				if (!identifier_symbol.is_valid()) {
					return false;
				}

				if (!add_symbol(identifier_symbol, building_source)) {
					return false;
				}

				expect_comma_or_rparen = (depth > 0);
			}
		} else if (token.type == e_token_type::k_real_constant) {
			s_native_module_optimization_symbol constant_symbol =
				s_native_module_optimization_symbol::build_real_value(token.real_constant);
			if (!add_symbol(constant_symbol, building_source)) {
				return false;
			}

			expect_comma_or_rparen = (depth > 0);
		} else if (token.type == e_token_type::k_bool_constant) {
			s_native_module_optimization_symbol constant_symbol =
				s_native_module_optimization_symbol::build_bool_value(token.bool_constant);
			if (!add_symbol(constant_symbol, building_source)) {
				return false;
			}

			expect_comma_or_rparen = (depth > 0);
		} else if (token.type == e_token_type::k_right_parenthesis) {
			depth--;
			if (depth < 0) {
				return false;
			}

			if (!add_symbol(s_native_module_optimization_symbol::build_native_module_end(), building_source)) {
				return false;
			}

			expect_comma_or_rparen = (depth > 0);
		} else {
			// Anything else isn't allowed
			return false;
		}

		if (depth == 0) {
			if (building_source) {
				expect_source_to_target = true;
			} else {
				expect_end = true;
			}
		}
	}

	return true;
}

c_optimization_rule_generator::s_token c_optimization_rule_generator::get_next_token() {
	// Simple parser - recognizes ( ) , -> and numbers and identifiers
	// First skip whitespace
	while (true) {
		char c = m_rule_string[m_rule_string_offset];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
			m_rule_string_offset++;
		} else {
			break;
		}
	}

	bool is_identifier = false;
	bool is_number = false;
	char c = m_rule_string[m_rule_string_offset];
	switch (c) {
	case '\0':
		return { e_token_type::k_end, {} }; // Don't advance m_rule_string_offset so this will keep getting returned

	case '(':
		return { e_token_type::k_left_parenthesis, std::string_view(&m_rule_string[m_rule_string_offset++], 1) };

	case ')':
		return { e_token_type::k_right_parenthesis, std::string_view(&m_rule_string[m_rule_string_offset++], 1) };

	case ',':
		return { e_token_type::k_comma, std::string_view(&m_rule_string[m_rule_string_offset++], 1) };

	case '-':
		if (m_rule_string[m_rule_string_offset + 1] == '>') {
			s_token result = {
				e_token_type::k_source_to_target,
				std::string_view(&m_rule_string[m_rule_string_offset], 1)
			};

			m_rule_string_offset += 2;
			return result;
		} else {
			is_number = true;
			break;
		}

	default:
		if (c >= '0' && c <= '0') {
			is_number = true;
		} else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_')) { // $TODO $UNICODE
			is_identifier = true;
		}
		break;
	}

	if (is_identifier) {
		size_t start_offset = m_rule_string_offset;
		while (true) {
			char c = m_rule_string[m_rule_string_offset++];
			if ((c >= 'A' && c <= 'Z')
				|| (c >= 'a' && c <= 'z')
				|| (c >= '0' && c <= '9')
				|| (c == '_')
				|| (c == '$')) { // $ is a valid character for native module identifiers
				// Valid character
			} else {
				std::string_view token_string(&m_rule_string[start_offset], m_rule_string_offset - start_offset);
				if (token_string == "const") {
					return { e_token_type::k_const, token_string };
				} else if (token_string == "false") {
					return { false, token_string };
				} else if (token_string == "true") {
					return { true, token_string };
				} else {
					return { e_token_type::k_identifier, token_string };
				}
			}
		}
	} else if (is_number) {
		// This is super simple, we only support integers. We could easily add more if needed.
		size_t start_offset = m_rule_string_offset;
		int32 value = 0;

		bool is_negative = (m_rule_string[m_rule_string_offset] == '-');
		if (is_negative) {
			m_rule_string_offset++;
		}

		// Loop until we hit a non-digit character - could improve this if needed
		while (true) {
			char c = m_rule_string[m_rule_string_offset++];
			if (c >= '0' && c <= '9') {
				value *= 10;
				value += (c - '0');
			} else {
				if (is_negative) {
					value = -value;
				}

				return {
					static_cast<real32>(value),
					std::string_view(&m_rule_string[start_offset], m_rule_string_offset - start_offset)
				};
			}
		}
	}

	return { e_token_type::k_invalid, {} };
}

bool c_optimization_rule_generator::add_symbol(s_native_module_optimization_symbol symbol, bool is_source_symbol) {
	s_native_module_optimization_pattern &pattern = is_source_symbol ? m_rule.source : m_rule.target;
	size_t symbol_count = is_source_symbol ? m_source_symbol_count : m_target_symbol_count;

	if (symbol_count >= pattern.symbols.get_count()) {
		return false;
	}

	pattern.symbols[symbol_count++] = symbol;
	return true;
}

s_native_module_optimization_symbol c_optimization_rule_generator::get_identifier_symbol(
	const std::string_view &identifier,
	bool is_constant,
	bool is_building_source) {
	auto it = m_identifier_map.find(identifier);

	if (it == m_identifier_map.end()) {
		if (!is_building_source) {
			// If we're building the target pattern and this identifier doesn't exist, then the source pattern never
			// declared it which is an error
			return s_native_module_optimization_symbol::invalid();
		}

		s_identifier new_identifier;
		new_identifier.index = is_constant ? m_constant_count : m_variable_count;
		new_identifier.is_constant = is_constant;

		if (new_identifier.index >= s_native_module_optimization_symbol::k_max_matched_symbols) {
			// We only support up to a fixed number of matched symbols
			return s_native_module_optimization_symbol::invalid();
		}

		if (is_constant) {
			m_constant_count++;
		} else {
			m_variable_count++;
		}

		m_identifier_map.insert(std::make_pair(identifier, new_identifier));
	} else {
		if (is_building_source) {
			// We can't declare the same variable twice in the source pattern because we don't support checking whether
			// two variables in the source pattern hold the same value
			return s_native_module_optimization_symbol::invalid();
		} else if (is_constant) {
			// We aren't allowed to declare variables as constant in the target pattern because the target isn't what
			// we're matching against
			return s_native_module_optimization_symbol::invalid();
		}
	}

	// Re-lookup the identifier - it should definitely exist now
	it = m_identifier_map.find(identifier);
	wl_assert(it != m_identifier_map.end());

	const s_identifier &result = it->second;
	return result.is_constant
		? s_native_module_optimization_symbol::build_constant(result.index)
		: s_native_module_optimization_symbol::build_variable(result.index);
}
