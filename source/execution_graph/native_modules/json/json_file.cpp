#include "execution_graph/native_modules/json/json_file.h"

#include <charconv>
#include <fstream>

static bool is_digit(char c);

s_json_result c_json_file::load(const char *filename) {
	wl_assert(!m_root);

	s_json_result result;
	zero_type(&result);

	std::vector<char> file_buffer;
	{
		// First read the whole file
		std::ifstream file(filename, std::ios::binary | std::ios::ate);

		if (!file.is_open()) {
			result.result = e_json_result::k_file_error;
			return result;
		}

		std::streampos full_file_size = file.tellg();
		file.seekg(0);

		if (full_file_size > static_cast<std::streampos>(std::numeric_limits<int32>::max())) {
			// File is too big
			result.result = e_json_result::k_file_error;
			return result;
		}

		size_t file_size = static_cast<size_t>(full_file_size);
		wl_assert(static_cast<std::streampos>(file_size) == full_file_size);

		file_buffer.resize(file_size + 1);
		if (file_size > 0) {
			file.read(&file_buffer.front(), file_size);
		}

		// Place a null terminator at the end so we always have a valid offset
		file_buffer.back() = '\0';

		if (file.fail()) {
			result.result = e_json_result::k_file_error;
			return result;
		}
	}

	s_buffer_with_offset buffer_with_offset;
	buffer_with_offset.buffer = &file_buffer.front();
	buffer_with_offset.offset = 0;
	buffer_with_offset.initialize_file_offset();

	std::unique_ptr<c_json_node> root(std::move(parse(buffer_with_offset)));
	if (!root) {
		result.result = e_json_result::k_parse_error;
		result.parse_error_line = buffer_with_offset.line;
		result.parse_error_character = buffer_with_offset.character;
		return result;
	}

	if (buffer_with_offset.offset != file_buffer.size() - 1) {
		// There was extra content at the end of the file
		result.result = e_json_result::k_parse_error;
		result.parse_error_line = buffer_with_offset.line;
		result.parse_error_character = buffer_with_offset.character;
		return result;
	}

	m_root.swap(root);
	result.result = e_json_result::k_success;
	return result;
}

s_json_result c_json_file::parse(const char *buffer) {
	wl_assert(!m_root);

	s_json_result result;
	zero_type(&result);

	s_buffer_with_offset buffer_with_offset;
	buffer_with_offset.buffer = buffer;
	buffer_with_offset.offset = 0;
	buffer_with_offset.initialize_file_offset();

	std::unique_ptr<c_json_node> root(std::move(parse_value(buffer_with_offset)));
	if (!root) {
		result.result = e_json_result::k_parse_error;
		result.parse_error_line = buffer_with_offset.line;
		result.parse_error_character = buffer_with_offset.character;
		return result;
	}

	if (buffer_with_offset.offset != strlen(buffer)) {
		// There was extra content at the end of the bufer
		result.result = e_json_result::k_parse_error;
		result.parse_error_line = buffer_with_offset.line;
		result.parse_error_character = buffer_with_offset.character;
		return result;
	}

	m_root.swap(root);
	result.result = e_json_result::k_success;
	return result;
}

const c_json_node *c_json_file::get_root() const {
	return m_root.get();
}


c_json_node *c_json_file::parse(s_buffer_with_offset &buffer_with_offset) const {
	return parse_value(buffer_with_offset);
}

bool c_json_file::parse_whitespace(s_buffer_with_offset &buffer_with_offset) const {
	while (true) {
		char c = buffer_with_offset.current_character();
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
			buffer_with_offset.increment();
		} else {
			return true;
		}
	}
}

c_json_node *c_json_file::parse_value(s_buffer_with_offset &buffer_with_offset) const {
	std::unique_ptr<c_json_node> result;

	if (!parse_whitespace(buffer_with_offset)) {
		return nullptr;
	}

	char c = buffer_with_offset.current_character();
	if (c == '"') {
		std::unique_ptr<c_json_node> result_inner(parse_string(buffer_with_offset));
		if (!result_inner) {
			return nullptr;
		}

		result.swap(result_inner);
	} else if (is_digit(c) || c == '-') {
		std::unique_ptr<c_json_node> result_inner(parse_number(buffer_with_offset));
		if (!result_inner) {
			return nullptr;
		}

		result.swap(result_inner);
	} else if (c == '{') {
		std::unique_ptr<c_json_node> result_inner(parse_object(buffer_with_offset));
		if (!result_inner) {
			return nullptr;
		}

		result.swap(result_inner);
	} else if (c == '[') {
		std::unique_ptr<c_json_node> result_inner(parse_array(buffer_with_offset));
		if (!result_inner) {
			return nullptr;
		}

		result.swap(result_inner);
	} else {
		// Note: reading until a non-alphabetical character for this step doesn't quite match the JSON spec but I think
		// the behavior ends up being the same.
		size_t start_offset = buffer_with_offset.offset;
		while (buffer_with_offset.current_character() >= 'a' && buffer_with_offset.current_character() <= 'z') {
			buffer_with_offset.increment();
		}

		static const char k_true_string[] = "true";
		static const char k_false_string[] = "false";
		static const char k_null_string[] = "null";

		// Note: subtract 1 from sizeof on the above strings to ignore the null terminator for comparison
		size_t length = buffer_with_offset.offset - start_offset;
		if (length == sizeof(k_true_string) - 1
			&& memcmp(&buffer_with_offset.buffer[start_offset], k_true_string, length) == 0) {
			std::unique_ptr<c_json_node> result_inner(new c_json_node_boolean(true));
			result.swap(result_inner);
		} else if (length == sizeof(k_false_string) - 1
			&& memcmp(&buffer_with_offset.buffer[start_offset], k_false_string, length) == 0) {
			std::unique_ptr<c_json_node> result_inner(new c_json_node_boolean(false));
			result.swap(result_inner);
		} else if (length == sizeof(k_null_string) - 1
			&& memcmp(&buffer_with_offset.buffer[start_offset], k_null_string, length) == 0) {
			std::unique_ptr<c_json_node> result_inner(new c_json_node_null());
			result.swap(result_inner);
		} else {
			return nullptr;
		}
	}

	if (!parse_whitespace(buffer_with_offset)) {
		return nullptr;
	}

	return result.release();
}

c_json_node_number *c_json_file::parse_number(s_buffer_with_offset &buffer_with_offset) const {
	// Parse tree for number:
	// number   : integer fraction exponent
	// integer  : -?([0-9]|([1-9][0-9]+))
	// fraction : ""|("."[0-9]+)
	// exponent : ""|(("E"|"e")(""|"+"|"-")[0-9]+)

	size_t start_offset = buffer_with_offset.offset;

	// Parse integer
	if (buffer_with_offset.current_character() == '-') {
		buffer_with_offset.increment();
	}

	if (!is_digit(buffer_with_offset.current_character())) {
		return nullptr;
	}

	if (buffer_with_offset.current_character() == '0'
		&& is_digit(buffer_with_offset.buffer[buffer_with_offset.offset + 1])) {
		return nullptr;
	}

	do {
		buffer_with_offset.increment();
	} while (is_digit(buffer_with_offset.current_character()));

	// Parse fraction
	if (buffer_with_offset.current_character() == '.') {
		buffer_with_offset.increment();

		if (!is_digit(buffer_with_offset.current_character())) {
			return nullptr;
		}

		do {
			buffer_with_offset.increment();
		} while (is_digit(buffer_with_offset.current_character()));
	}

	// Parse exponent
	if (buffer_with_offset.current_character() == 'E' || buffer_with_offset.current_character() == 'e') {
		buffer_with_offset.increment();

		if (buffer_with_offset.current_character() == '+' || buffer_with_offset.current_character() == '-') {
			buffer_with_offset.increment();
		}

		if (!is_digit(buffer_with_offset.current_character())) {
			return nullptr;
		}

		do {
			buffer_with_offset.increment();
		} while (is_digit(buffer_with_offset.current_character()));
	}

	try {
		size_t length = buffer_with_offset.offset - start_offset;
		wl_assert(length > 0);
		real64 value = std::stod(std::string(&buffer_with_offset.buffer[start_offset], length), nullptr);
		return new c_json_node_number(value);
	} catch (const std::exception &) {
		return nullptr;
	}
}

c_json_node_string *c_json_file::parse_string(s_buffer_with_offset &buffer_with_offset) const {
	if (buffer_with_offset.current_character() != '"') {
		return nullptr;
	}

	buffer_with_offset.increment();

	std::string result;

	bool done = false;
	bool escape = false;
	while (!done) {
		char c = buffer_with_offset.current_character();
		if (!escape) {
			if (c < 0x20) {
				return nullptr;
			} else {
				buffer_with_offset.increment();
				if (c == '"') {
					done = true;
				} else if (c == '\\') {
					escape = true;
				} else {
					result.push_back(c);
				}
			}
		} else {
			escape = false;
			switch (c) {
			case '"':
				buffer_with_offset.increment();
				result.push_back('"');
				break;

			case '\\':
				buffer_with_offset.increment();
				result.push_back('\\');
				break;

			case '/':
				buffer_with_offset.increment();
				result.push_back('/');
				break;

			case 'b':
				buffer_with_offset.increment();
				result.push_back('\b');
				break;

			case 'f':
				buffer_with_offset.increment();
				result.push_back('\f');
				break;

			case 'n':
				buffer_with_offset.increment();
				result.push_back('\n');
				break;

			case 'r':
				buffer_with_offset.increment();
				result.push_back('\r');
				break;

			case 't':
				buffer_with_offset.increment();
				result.push_back('\t');
				break;

			case 'u':
				{
					buffer_with_offset.increment();
					uint32 unicode_value = 0;

					for (uint32 i = 0; i < 4; i++) {
						uint32 byte_value = 0;
						char b = buffer_with_offset.current_character();

						if (b >= '0' && b <= '9') {
							buffer_with_offset.increment();
							byte_value = cast_integer_verify<uint32>(b - '0');
						} else if (b >= 'A' && b <= 'F') {
							buffer_with_offset.increment();
							byte_value = cast_integer_verify<uint32>(b - 'A' + 10);
						} else if (b >= 'a' && b <= 'f') {
							buffer_with_offset.increment();
							byte_value = cast_integer_verify<uint32>(b - 'a' + 10);
						} else {
							return nullptr;
						}

						wl_assert(byte_value < 16);
						unicode_value |= byte_value << (4 * (3 - i));
					}

					// $TODO $UNICODE we currently only support ASCII characters
					if (unicode_value >= 128) {
						return nullptr;
					}

					result.push_back(cast_integer_verify<char>(unicode_value));
				}
				break;

			default:
				return nullptr;
			}
		}
	}

	return new c_json_node_string(result.c_str());
}

c_json_node_array *c_json_file::parse_array(s_buffer_with_offset &buffer_with_offset) const {
	if (buffer_with_offset.current_character() != '[') {
		return nullptr;
	}

	buffer_with_offset.increment();

	if (!parse_whitespace(buffer_with_offset)) {
		return nullptr;
	}

	std::unique_ptr<c_json_node_array> result(new c_json_node_array());

	if (buffer_with_offset.current_character() == ']') {
		// Empty array
		buffer_with_offset.increment();
		return result.release();
	}

	// Read the first element
	std::unique_ptr<c_json_node> first_element(parse_value(buffer_with_offset));
	if (!first_element) {
		return nullptr;
	}

	result->add_element(first_element.release());

	while (buffer_with_offset.current_character() != ']') {
		if (buffer_with_offset.current_character() != ',') {
			return nullptr;
		}

		buffer_with_offset.increment();

		std::unique_ptr<c_json_node> element(parse_value(buffer_with_offset));
		if (!element) {
			return nullptr;
		}

		result->add_element(element.release());
	}

	buffer_with_offset.increment();

	return result.release();
}

c_json_node_object *c_json_file::parse_object(s_buffer_with_offset &buffer_with_offset) const {
	if (buffer_with_offset.current_character() != '{') {
		return nullptr;
	}

	buffer_with_offset.increment();

	if (!parse_whitespace(buffer_with_offset)) {
		return nullptr;
	}

	std::unique_ptr<c_json_node_object> result(new c_json_node_object());

	if (buffer_with_offset.current_character() == '}') {
		// Empty object
		buffer_with_offset.increment();
		return result.release();
	}

	// Read the first element
	std::unique_ptr<c_json_node_string> first_element_name(parse_string(buffer_with_offset));
	if (!first_element_name) {
		return nullptr;
	}

	if (!parse_whitespace(buffer_with_offset)) {
		return nullptr;
	}

	if (buffer_with_offset.current_character() != ':') {
		return nullptr;
	}

	buffer_with_offset.increment();

	std::unique_ptr<c_json_node> first_element(parse_value(buffer_with_offset));
	if (!first_element) {
		return nullptr;
	}

	result->add_element(first_element_name->get_value(), first_element.release());

	while (buffer_with_offset.current_character() != '}') {
		if (buffer_with_offset.current_character() != ',') {
			return nullptr;
		}

		buffer_with_offset.increment();

		if (!parse_whitespace(buffer_with_offset)) {
			return nullptr;
		}

		std::unique_ptr<c_json_node_string> element_name(parse_string(buffer_with_offset));
		if (!element_name) {
			return nullptr;
		}

		if (!parse_whitespace(buffer_with_offset)) {
			return nullptr;
		}

		if (buffer_with_offset.current_character() != ':') {
			return nullptr;
		}

		buffer_with_offset.increment();

		std::unique_ptr<c_json_node> element(parse_value(buffer_with_offset));
		if (!element) {
			return nullptr;
		}

		result->add_element(element_name->get_value(), element.release());
	}

	buffer_with_offset.increment();

	return result.release();
}

const c_json_node *resolve_json_node_path(const c_json_node *root_node, const char *node_path) {
	const c_json_node *current_node = root_node;
	size_t node_path_offset = 0;
	std::string name_buffer; // This is necessary to transform escape sequences
	while (true) {
		char c = node_path[node_path_offset];
		if (c == '\0') {
			break;
		} else if (c == '[') {
			// This is an array index
			node_path_offset++;
			size_t index_start_offset = node_path_offset;
			size_t index_end_offset = node_path_offset;
			while (true) {
				c = node_path[node_path_offset];
				if (c == ']') {
					index_end_offset = node_path_offset;
					node_path_offset++;
					break;
				} else if (c >= '0' && c <= '9') {
					node_path_offset++;
				} else {
					// We didn't set the end offset so we will fail
					break;
				}
			}

			if (index_start_offset == index_end_offset) {
				return nullptr;
			}

			size_t index;
			std::from_chars_result index_result = std::from_chars(
				node_path + index_start_offset,
				node_path + index_end_offset,
				index);
			if (index_result.ec != std::errc()) {
				return nullptr;
			}

			const c_json_node_array *array_node = current_node->try_get_as<c_json_node_array>();
			if (!array_node || index >= array_node->get_value().get_count()) {
				return nullptr;
			}

			current_node = array_node->get_value()[index];
		} else if (node_path_offset == 0 || c == '.') {
			// This is a named lookup
			if (c == '.') {
				node_path_offset++;
			}

			name_buffer.clear();
			bool escape = false;
			while (true) {
				c = node_path[node_path_offset];
				node_path_offset++;

				if (escape) {
					if (c == '.' || c == '[' || c == '\\') {
						name_buffer.push_back(c);
						escape = false;
					} else {
						// Unknown escape sequence
						return nullptr;
					}
				} else {
					if (c == '\0' || c == '.' || c == '[') {
						node_path_offset--; // Process these character again in the outer loop
						break;
					} else if (c == '\\') {
						escape = true;
					} else {
						name_buffer.push_back(c);
					}
				}
			}

			const c_json_node_object *object_node = current_node->try_get_as<c_json_node_object>();
			if (!object_node) {
				return nullptr;
			}

			const c_json_node *element_node = object_node->get_element(name_buffer.c_str());
			if (!element_node) {
				return nullptr;
			}

			current_node = element_node;
		} else {
			return nullptr;
		}
	}

	return current_node;
}

static bool is_digit(char c) {
	return c >= '0' && c <= '9';
}
