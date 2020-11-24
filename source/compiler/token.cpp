#include "compiler/token.h"

#include "generated/wavelang_grammar.h"

#include <charconv>

// Note: keep this code in sync with try_read_string_constant() in lexer.cpp
std::string get_unescaped_token_string(const s_token &token) {
	wl_assert(token.token_type == e_token_type::k_literal_string);
	wl_assert(token.token_string.size() >= 2);
	wl_assert(token.token_string.front() == '"');
	wl_assert(token.token_string.back() == '"');

	std::string_view input = token.token_string.substr(1, token.token_string.size() - 2);
	std::string output;
	output.reserve(input.length());

	bool done = false;
	bool escape = false;
	size_t offset = 0;
	while (!done) {
		if (!escape && offset == input.size()) {
			break;
		}

		wl_assert(offset < input.size());
		char c = input[offset++];

		if (!escape) {
			wl_assert(c >= 0x20);
			if (c == '\\') {
				escape = true;
			} else {
				output.push_back(c);
			}
		} else {
			escape = false;
			switch (c) {
			case '"':
				output.push_back('"');
				break;

			case '\\':
				output.push_back('\\');
				break;

			case 'b':
				output.push_back('\b');
				break;

			case 'f':
				output.push_back('\f');
				break;

			case 'n':
				output.push_back('\n');
				break;

			case 'r':
				output.push_back('\r');
				break;

			case 't':
				output.push_back('\t');
				break;

			case 'u':
			{
				uint32 unicode_value = 0;

				for (uint32 i = 0; i < 4; i++) {
					uint32 byte_value = 0;
					wl_assert(offset < input.size());
					char b = input[offset++];

					if (b >= '0' && b <= '9') {
						byte_value = cast_integer_verify<uint32>(b - '0');
					} else if (b >= 'A' && b <= 'F') {
						byte_value = cast_integer_verify<uint32>(b - 'A' + 10);
					} else if (b >= 'a' && b <= 'f') {
						byte_value = cast_integer_verify<uint32>(b - 'a' + 10);
					} else {
						wl_unreachable();
					}

					wl_assert(byte_value < 16);
					unicode_value |= byte_value << (4 * (3 - i));
				}

				// $TODO $UNICODE we currently only support ASCII characters
				wl_assert(unicode_value < 128);
			}
			break;

			default:
				wl_unreachable();
			}
		}
	}

	return output;
}

std::string escape_token_string_for_output(const std::string_view &token_string) {
	std::string output;
	output.reserve(token_string.size());

	for (char c : token_string) {
		if (c >= 0x20) {
			output.push_back(c);
		} else {
			switch (c) {
			case '\b':
				output.append("\\b");
				break;

			case '\f':
				output.append("\\f");
				break;

			case '\n':
				output.append("\\n");
				break;

			case '\r':
				output.append("\\r");
				break;

			case '\t':
				output.append("\\t");
				break;

			default:
			{
				// $TODO $UNICODE
				char hex_buffer[3];
				std::to_chars_result result =
					std::to_chars(hex_buffer, hex_buffer + sizeof(hex_buffer), static_cast<uint8>(c), 16);
				wl_assert(result.ec == std::errc());
				if (result.ptr == hex_buffer + 1) {
					hex_buffer[1] = hex_buffer[0];
					hex_buffer[0] = '0';
				} else {
					wl_assert(result.ptr == hex_buffer + 2);
				}
				hex_buffer[2] = '\0';

				output.append("\\x");
				output.append(hex_buffer);
			}
			}
		}
	}

	return output;
}
