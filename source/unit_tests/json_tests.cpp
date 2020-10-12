#include "common/common.h"

#include "execution_graph/native_modules/json/json_file.h"

#include <gtest/gtest.h>

template<typename t_node>
const t_node *try_get_node(const c_json_node_object *root, const char *name) {
	const c_json_node *element_node = root->get_element(name);
	EXPECT_NE(element_node, nullptr);
	if (element_node) {
		const t_node *element_node_typed = element_node->try_get_as<t_node>();
		EXPECT_NE(element_node_typed, nullptr);
		return element_node_typed;
	} else {
		return nullptr;
	}
}

TEST(Json, Json) {
	const char *k_json_buffer =
		"{\n"
		"  \"null\": null,\n"
		"  \"number_a\": 2.0,\n"
		"  \"number_b\": -2.0,\n"
		"  \"number_c\": 3e2,\n"
		"  \"string\": \"String with\\tescape \\\"chars\\u0030\\\"\",\n"
		"  \"boolean_false\": false,\n"
		"  \"boolean_true\": true,\n"
		"  \"array\": [3.0, \"string\", true],\n"
		"  \"object\": { \"a\": 1.0, \"b\": null }\n"
		"}\n   "; // Test trailing whitespace
	c_json_file json_file;
	s_json_result result = json_file.parse(k_json_buffer);
	EXPECT_EQ(result.result, e_json_result::k_success);
	if (result.result == e_json_result::k_success) {
		const c_json_node_object *root = json_file.get_root()->try_get_as<c_json_node_object>();
		EXPECT_NE(root, nullptr);
		EXPECT_EQ(root, resolve_json_node_path(root, ""));
		if (root) {
			const c_json_node_null *null_node = try_get_node<c_json_node_null>(root, "null");
			EXPECT_EQ(null_node, resolve_json_node_path(root, "null"));

			const c_json_node_number *number_a_node = try_get_node<c_json_node_number>(root, "number_a");
			EXPECT_EQ(number_a_node, resolve_json_node_path(root, "number_a"));
			if (number_a_node) {
				EXPECT_EQ(number_a_node->get_value(), 2.0);
			}

			const c_json_node_number *number_b_node = try_get_node<c_json_node_number>(root, "number_b");
			EXPECT_EQ(number_b_node, resolve_json_node_path(root, "number_b"));
			if (number_b_node) {
				EXPECT_EQ(number_b_node->get_value(), -2.0);
			}

			const c_json_node_number *number_c_node = try_get_node<c_json_node_number>(root, "number_c");
			EXPECT_EQ(number_c_node, resolve_json_node_path(root, "number_c"));
			if (number_c_node) {
				EXPECT_EQ(number_c_node->get_value(), 3e2);
			}

			const c_json_node_boolean *boolean_false_node = try_get_node<c_json_node_boolean>(root, "boolean_false");
			EXPECT_EQ(boolean_false_node, resolve_json_node_path(root, "boolean_false"));
			if (boolean_false_node) {
				EXPECT_FALSE(boolean_false_node->get_value());
			}

			const c_json_node_boolean *boolean_true_node = try_get_node<c_json_node_boolean>(root, "boolean_true");
			EXPECT_EQ(boolean_true_node, resolve_json_node_path(root, "boolean_true"));
			if (boolean_true_node) {
				EXPECT_TRUE(boolean_true_node->get_value());
			}

			const c_json_node_string *string_node = try_get_node<c_json_node_string>(root, "string");
			EXPECT_EQ(string_node, resolve_json_node_path(root, "string"));
			if (string_node) {
				EXPECT_EQ(strcmp(string_node->get_value(), "String with\tescape \"chars0\""), 0);
			}

			const c_json_node_array *array_node = try_get_node<c_json_node_array>(root, "array");
			EXPECT_EQ(array_node, resolve_json_node_path(root, "array"));
			if (array_node) {
				EXPECT_EQ(array_node->get_value().get_count(), 3);
				if (array_node->get_value().get_count() == 3) {
					const c_json_node_number *element_0_node =
						array_node->get_value()[0]->try_get_as<c_json_node_number>();
					EXPECT_NE(element_0_node, nullptr);
					EXPECT_EQ(element_0_node, resolve_json_node_path(root, "array[0]"));
					if (element_0_node) {
						EXPECT_EQ(element_0_node->get_value(), 3.0);
					}

					const c_json_node_string *element_1_node =
						array_node->get_value()[1]->try_get_as<c_json_node_string>();
					EXPECT_NE(element_1_node, nullptr);
					EXPECT_EQ(element_1_node, resolve_json_node_path(root, "array[1]"));
					if (element_1_node) {
						EXPECT_EQ(strcmp(element_1_node->get_value(), "string"), 0);
					}

					const c_json_node_boolean *element_2_node =
						array_node->get_value()[2]->try_get_as<c_json_node_boolean>();
					EXPECT_NE(element_2_node, nullptr);
					EXPECT_EQ(element_2_node, resolve_json_node_path(root, "array[2]"));
					if (element_2_node) {
						EXPECT_TRUE(element_2_node->get_value());
					}
				}
			}

			const c_json_node_object *object_node = try_get_node<c_json_node_object>(root, "object");
			EXPECT_EQ(object_node, resolve_json_node_path(root, "object"));
			if (object_node) {
				const c_json_node_number *a_node = try_get_node<c_json_node_number>(object_node, "a");
				EXPECT_EQ(a_node, resolve_json_node_path(root, "object.a"));
				if (a_node) {
					EXPECT_EQ(a_node->get_value(), 1.0);
				}

				const c_json_node_null *b_node = try_get_node<c_json_node_null>(object_node, "b");
				EXPECT_EQ(b_node, resolve_json_node_path(root, "object.b"));
			}
		}
	}

	const char *k_json_buffer_trailing_content = "{} {}";
	c_json_file json_file_trailing_content;
	s_json_result result_trailing_content = json_file_trailing_content.parse(k_json_buffer_trailing_content);
	EXPECT_EQ(result_trailing_content.result, e_json_result::k_parse_error);
}
