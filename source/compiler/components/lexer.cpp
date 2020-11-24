#include "common/utility/handle.h"

#include "compiler/components/lexer.h"
#include "compiler/token.h"

#include "generated/wavelang_grammar.h"

#include <stdexcept>
#include <unordered_map>

static constexpr const char *k_token_type_constant_bool_false_string = "false";
static constexpr const char *k_token_type_constant_bool_true_string = "true";

// This is a mini DFA used to detect symbols (exact matches only, not general regex)
class c_symbol_detector {
public:
	struct s_state_handle_identifier {};
	using h_state = c_handle<s_state_handle_identifier, size_t>;

	c_symbol_detector() {
		m_states.push_back({});
	}

	void clear() {
		m_states.clear();
		m_states.shrink_to_fit();
		m_transitions.clear();
		m_transitions.shrink_to_fit();
	}

	void add_symbol(e_token_type token_type, const char *symbol_string) {
		h_state current_state_handle = h_state::construct(0);

		while (true) {
			char c = *symbol_string;
			if (c == '\0') {
				break;
			}

			s_state &current_state = m_states[current_state_handle.get_data()];
			h_transition transition_handle = current_state.first_transition_handle;
			while (transition_handle.is_valid()) {
				const s_transition &transition = m_transitions[transition_handle.get_data()];
				if (transition.character == c) {
					break;
				}

				transition_handle = transition.next_transition_handle;
			}

			if (!transition_handle.is_valid()) {
				// No transition currently exists, make a new state and a transition to that state
				transition_handle = h_transition::construct(m_transitions.size());
				h_state new_state_handle = h_state::construct(m_states.size());

				s_transition transition = { c, new_state_handle, current_state.first_transition_handle };
				current_state.first_transition_handle = transition_handle;

				m_transitions.push_back(transition);
				m_states.push_back({}); // Note: current_state could now be invalid due to realloc
			}

			wl_assert(m_transitions[transition_handle.get_data()].character == c);
			current_state_handle = m_transitions[transition_handle.get_data()].next_state_handle;
		}

		s_state &final_state = m_states[current_state_handle.get_data()];

		// If this fires, it means we have a conflict (two identical symbol strings)
		wl_assert(final_state.token_type == e_token_type::k_invalid);
		final_state.token_type = token_type;
	}

	// Attempts to advance to the next state, returning true if successful. If the new state has an associated token, it
	// is stored in token_out; otherwise, token_out is set to invalid.
	bool advance(char c, h_state &state_handle_inout, e_token_type &token_out) const {
		if (!state_handle_inout.is_valid()) {
			state_handle_inout = h_state::construct(0);
		}

		token_out = e_token_type::k_invalid;
		const s_state &state = m_states[state_handle_inout.get_data()];
		h_transition transition_handle = state.first_transition_handle;
		while (transition_handle.is_valid()) {
			const s_transition &transition = m_transitions[transition_handle.get_data()];
			if (c == transition.character) {
				state_handle_inout = transition.next_state_handle;
				const s_state &new_state = m_states[transition.next_state_handle.get_data()];
				token_out = new_state.token_type;
			}

			transition_handle = transition.next_transition_handle;
		}

		return false;
	}

private:
	struct s_transition_handle_identifier {};
	using h_transition = c_handle<s_transition_handle_identifier, size_t>;

	struct s_state {
		// The token type this state produces, or invalid
		e_token_type token_type = e_token_type::k_invalid;

		// The handle of the first possible transition for this state
		h_transition first_transition_handle = h_transition::invalid();
	};

	struct s_transition {
		char character;							// The character causing this transition
		h_state next_state_handle;				// The state being transitioned to
		h_transition next_transition_handle;	// The handle of the next transition in the list
	};

	std::vector<s_state> m_states;
	std::vector<s_transition> m_transitions;
};

using c_keyword_table = std::unordered_map<std::string_view, e_token_type>;

struct s_lexer_globals {
	c_keyword_table keyword_table;
	c_symbol_detector symbol_detector;
};

// Utility class to keep track of where we are in the source file
class c_lexer_location {
public:
	c_lexer_location(std::string_view source, h_compiler_source_file source_file_handle) {
		m_remaining_source = source;
		m_source_location.source_file_handle = source_file_handle;
		m_source_location.line = 1;
		m_source_location.character = 1;
	}

	const std::string_view &remaining_source() const {
		return m_remaining_source;
	}

	const s_compiler_source_location &source_location() const {
		return m_source_location;
	}

	void increment(size_t amount = 1) {
		wl_assert(!m_remaining_source.empty());
		for (size_t i = 0; i < amount; i++) {
			if (m_remaining_source.front() == '\n') {
				m_source_location.line++;
				m_source_location.character = 1;
			} else {
				m_source_location.character++;
			}

			m_remaining_source.remove_prefix(1);
		}
	}

	bool eof() const {
		return m_remaining_source.empty();
	}

	char next_character(size_t index = 0) const {
		return index >= m_remaining_source.size() ? '\0' : m_remaining_source[index];
	}

	bool do_next_characters_match(const char *str) {
		for (size_t offset = 0;; offset++, str++) {
			char c = *str;
			if (c == '\0') {
				return true;
			} else if (offset == m_remaining_source.size() || m_remaining_source[offset] != *str) {
				return false;
			}
		}
	}

	bool is_next_character_whitespace() const {
		char c = next_character();
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	}

	bool is_next_character_digit(size_t index = 0) const {
		char c = next_character(index);
		return c >= '0' && c <= '9';
	}

	bool is_next_character_identifier_start_character() {
		char c = next_character();
		return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c == '_');
	}

	bool is_next_character_identifier_character() {
		return is_next_character_identifier_start_character() || is_next_character_digit();
	}


private:
	std::string_view m_remaining_source;
	s_compiler_source_location m_source_location;
};

static bool g_lexer_initialized = false;
static s_lexer_globals g_lexer_globals;

static s_token read_next_token(c_lexer_location &location);
static bool try_read_identifier(c_lexer_location &location, s_token &result_out);
static bool try_read_real_constant(c_lexer_location &location, s_token &result_out);
static bool try_read_string_constant(c_lexer_location &location, s_token &result_out);
static s_token read_symbol(c_lexer_location &location);

void c_lexer::initialize() {
	wl_assert(!g_lexer_initialized);

	for (e_token_type token_type = enum_next(e_token_type::k_keywords_start);
		token_type < e_token_type::k_keywords_end;
		token_type = enum_next(token_type)) {
		wl_assert(!is_string_empty(get_wavelang_terminal_string(token_type)));
		g_lexer_globals.keyword_table.insert(std::make_pair(get_wavelang_terminal_string(token_type), token_type));
	}

	// Add bool values to the keyword table
	g_lexer_globals.keyword_table.insert(
		std::make_pair(k_token_type_constant_bool_false_string, e_token_type::k_literal_bool));
	g_lexer_globals.keyword_table.insert(
		std::make_pair(k_token_type_constant_bool_true_string, e_token_type::k_literal_bool));

	for (e_token_type token_type = enum_next(e_token_type::k_symbols_start);
		token_type <= e_token_type::k_symbols_end;
		token_type = enum_next(token_type)) {
		if (!is_string_empty(get_wavelang_terminal_string(token_type))) {
			g_lexer_globals.symbol_detector.add_symbol(token_type, get_wavelang_terminal_string(token_type));
		}
	}

	g_lexer_initialized = true;
}

void c_lexer::deinitialize() {
	wl_assert(g_lexer_initialized);

	g_lexer_globals.keyword_table.clear();
	g_lexer_globals.symbol_detector.clear();

	g_lexer_initialized = false;
}

bool c_lexer::process(c_compiler_context &context, h_compiler_source_file source_file_handle) {
	wl_assert(g_lexer_initialized);

	bool result = true;
	s_compiler_source_file &source_file = context.get_source_file(source_file_handle);
	c_lexer_location location(
		std::string_view(source_file.source.empty() ? nullptr : &source_file.source.front(), source_file.source.size()),
		source_file_handle);

	size_t invalid_token_count = 0;
	static constexpr size_t k_invalid_token_limit = 100;

	while (true) {
		s_token token = read_next_token(location);
		if (token.token_type == e_token_type::k_invalid) {
			result = false;
			context.error(
				e_compiler_error::k_invalid_token,
				location.source_location(),
				"Invalid token '%s'",
				std::string(token.token_string));
			if (invalid_token_count >= k_invalid_token_limit) {
				context.error(
					e_compiler_error::k_invalid_token,
					"%llu invalid tokens were encountered, is this a source file?",
					invalid_token_count);
				break;
			}
		} else if (token.token_type == e_token_type::k_eof) {
			break;
		}
	}

	return result;
}

static s_token read_next_token(c_lexer_location &location) {
	wl_assert(g_lexer_initialized);

	// Skip whitespace and comments
	bool try_skipping = true;
	while (try_skipping) {
		// If we detect whitespace or comments, we should try skipping again
		try_skipping = false;

		while (location.is_next_character_whitespace()) {
			try_skipping = true;
			location.increment();
		}

		if (location.do_next_characters_match("//")) {
			try_skipping = true;
			location.increment(2);
			while (!location.eof() != '\0' && location.next_character() != '\n') {
				location.increment();
			}
		}
	}

	s_token result;
	result.token_type = e_token_type::k_invalid;
	result.source_location = location.source_location();
	zero_type(&result.value);

	if (location.eof()) {
		// EOF detected
		result.token_type = e_token_type::k_eof;
		return result;
	} else if (try_read_identifier(location, result)) {
		return result;
	} else if (try_read_real_constant(location, result)) {
		return result;
	} else if (try_read_string_constant(location, result)) {
		return result;
	} else {
		// This will detect any invalid characters
		return read_symbol(location);
	}
}

static bool try_read_identifier(c_lexer_location &location, s_token &result_out) {
	if (!location.is_next_character_identifier_start_character()) {
		return false;
	}

	// Identifier detected, keep reading all valid identifier characters
	result_out.token_string = location.remaining_source();
	size_t length = 0;

	while (location.is_next_character_identifier_character()) {
		location.increment();
		length++;
	}

	result_out.token_string = result_out.token_string.substr(0, length);

	// Check to see whether this token is in the keyword table
	c_keyword_table::const_iterator keyword_result = g_lexer_globals.keyword_table.find(result_out.token_string);
	if (keyword_result != g_lexer_globals.keyword_table.end()) {
		result_out.token_type = keyword_result->second;
		if (result_out.token_type == e_token_type::k_literal_bool) {
			wl_assert(
				result_out.token_string == k_token_type_constant_bool_false_string
				|| result_out.token_string == k_token_type_constant_bool_true_string);
			result_out.value.bool_value = (result_out.token_string == k_token_type_constant_bool_true_string);
		}
	} else {
		// It's not a keyword, it's just an ordinary identifier
		result_out.token_type = e_token_type::k_identifier;
	}

	return true;
}

static bool try_read_real_constant(c_lexer_location &location, s_token &result_out) {
	// Parse tree for number, taken from JSON
	// number   : integer fraction exponent
	// integer  : -?([0-9]|([1-9][0-9]+))
	// fraction : ""|("."[0-9]+)
	// exponent : ""|(("E"|"e")(""|"+"|"-")[0-9]+)

	if (!location.is_next_character_digit() && !location.next_character() == '-') {
		return false;
	}

	result_out.token_string = location.remaining_source();
	size_t length = 0;

	if (location.next_character() == '-') {
		if (!location.is_next_character_digit(1)) {
			// A minus sign is also a valid symbol - if we don't detect a number, return for symbol detection
			return false;
		}

		location.increment();
		length++;
	}

	wl_assert(location.is_next_character_digit());

	// Parse integer
	if (location.next_character() == '0') {
		location.increment();
		length++;

		if (location.is_next_character_digit()) {
			// Leading 0 not allowed, return an invalid token consisting of all remaining digits
			while (location.is_next_character_digit()) {
				location.increment();
				length++;
			}

			result_out.token_type = e_token_type::k_invalid;
			result_out.token_string = result_out.token_string.substr(0, length);
			return true;
		}
	}

	while (location.is_next_character_digit()) {
		location.increment();
		length++;
	}

	// Parse fraction
	if (location.next_character() == '.') {
		location.increment();
		length++;

		if (!location.is_next_character_digit()) {
			result_out.token_type = e_token_type::k_invalid;
			result_out.token_string = result_out.token_string.substr(0, length);
			return true;
		}

		do {
			location.increment();
			length++;
		} while (location.is_next_character_digit());
	}

	// Parse exponent
	if (location.next_character() == 'E' || location.next_character() == 'e') {
		location.increment();
		length++;

		if (location.next_character() == '+' || location.next_character() == '-') {
			location.increment();
			length++;
		}

		if (!location.is_next_character_digit()) {
			result_out.token_type = e_token_type::k_invalid;
			result_out.token_string = result_out.token_string.substr(0, length);
			return true;
		}

		do {
			location.increment();
			length++;
		} while (location.is_next_character_digit());
	}

	result_out.token_type = e_token_type::k_literal_real;
	result_out.token_string = result_out.token_string.substr(0, length);
	try {
		result_out.value.real_value = std::stof(std::string(result_out.token_string));
	} catch (const std::exception &) {
		result_out.token_type = e_token_type::k_invalid;
	}

	return true;
}

// $TODO $COMPILER we should probably disallows null terminators inside of strings. Same for JSON strings.
// Note: keep this code in sync with get_unescaped_token_string() in token.cpp
static bool try_read_string_constant(c_lexer_location &location, s_token &result_out) {
	if (location.next_character() != '"') {
		return false;
	}

	result_out.token_string = location.remaining_source();

	location.increment();
	size_t length = 1;

	// $TODO $COMPILER we will need to re-escape this string later
	bool failed = false;
	bool done = false;
	bool escape = false;
	while (!done) {
		if (location.eof()) {
			failed = true;
			done = true;
		} else {
			char c = location.next_character();
			location.increment();
			length++;

			if (!escape) {
				if (c < 0x20) {
					failed = true;
				} else {
					if (c == '"') {
						done = true;
					} else if (c == '\\') {
						escape = true;
					}
				}
			} else {
				escape = false;
				switch (c) {
				case '"':
				case '\\':
				case 'b':
				case 'f':
				case 'n':
				case 'r':
				case 't':
					break;

				case 'u':
				{
					uint32 unicode_value = 0;

					for (uint32 i = 0; i < 4; i++) {
						uint32 byte_value = 0;
						char b = location.next_character();

						if (b >= '0' && b <= '9') {
							location.increment();
							length++;
							byte_value = cast_integer_verify<uint32>(b - '0');
						} else if (b >= 'A' && b <= 'F') {
							location.increment();
							length++;
							byte_value = cast_integer_verify<uint32>(b - 'A' + 10);
						} else if (b >= 'a' && b <= 'f') {
							location.increment();
							length++;
							byte_value = cast_integer_verify<uint32>(b - 'a' + 10);
						} else {
							failed = true;
						}

						if (!failed) {
							wl_assert(byte_value < 16);
							unicode_value |= byte_value << (4 * (3 - i));
						}
					}

					// $TODO $UNICODE we currently only support ASCII characters
					if (unicode_value >= 128) {
						failed = true;
					}
				}
				break;

				default:
					failed = true;
				}
			}
		}
	}

	result_out.token_type = failed ? e_token_type::k_invalid : e_token_type::k_literal_string;
	result_out.token_string = result_out.token_string.substr(0, length);
	return true;
}

static s_token read_symbol(c_lexer_location &location) {
	// We should never run this function if we're at the end of the source file. We should always be able to return a
	// token of length 1 if we encounter an invalid character.
	wl_assert(!location.eof());

	s_token result;
	result.token_type = e_token_type::k_invalid;
	result.source_location = location.source_location();
	zero_type(&result.value);

	e_token_type last_matched_token_type = e_token_type::k_invalid;
	size_t last_matched_length = 0;
	size_t offset = 0;
	c_symbol_detector::h_state state = c_symbol_detector::h_state::invalid();
	while (true) {
		// Feed characters to the symbol detector and store whatever tokens it spits out - we will keep the longest one.
		// If we fail to advance it means that no valid token types are possible from the characters we've input so far,
		// so we break from the loop.
		e_token_type output_token_type;
		bool advanced = g_lexer_globals.symbol_detector.advance(
			location.next_character(offset),
			state,
			output_token_type);
		if (advanced) {
			offset++;
			if (output_token_type != e_token_type::k_invalid) {
				last_matched_token_type = output_token_type;
				last_matched_length = offset;
			}
		} else {
			break;
		}
	}

	size_t length = (last_matched_token_type == e_token_type::k_invalid) ? 1 : last_matched_length;
	result.token_type = last_matched_token_type;
	result.token_string = location.remaining_source().substr(0, length);

	location.increment(length);
	return result;
}
