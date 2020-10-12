#include "execution_graph/diagnostic/diagnostic.h"
#include "execution_graph/node_interface/node_interface.h"
#include "execution_graph/native_modules/json/json_file_manager.h"
#include "execution_graph/native_modules/native_modules_json.h"

static const c_json_file *load_json_file_or_report_error(
	c_json_file_manager *json_file_manager,
	c_diagnostic *diagnostic,
	const char *filename);
static const c_json_node *get_json_node_or_report_error(
	const c_json_file *json_file,
	c_diagnostic *diagnostic,
	const char *filename,
	const char *node_path);

static const c_json_file *load_json_file_or_report_error(
	c_json_file_manager *json_file_manager,
	c_diagnostic *diagnostic,
	const char *filename) {
	const c_json_file *json_file;
	s_json_result result = json_file_manager->load_json_file(filename, &json_file);
	switch (result.result) {
	case e_json_result::k_success:
		// Nothing to do
		break;

	case e_json_result::k_file_error:
		diagnostic->error(("Failed to load JSON file '" + std::string(filename) + "'").c_str());
		break;

	case e_json_result::k_parse_error:
		diagnostic->error((
			"Failed to parse JSON file '" + std::string(filename)
			+ "' (line " + std::to_string(result.parse_error_line)
			+ ", character " + std::to_string(result.parse_error_character)
			+ ")").c_str());
		break;

	default:
		wl_unreachable();
	}

	return json_file;
}

static const c_json_node *get_json_node_or_report_error(
	const c_json_file *json_file,
	c_diagnostic *diagnostic,
	const char *filename,
	const char *node_path) {
	const c_json_node *node = resolve_json_node_path(json_file->get_root(), node_path);
	if (!node) {
		diagnostic->error(("Failed to resolve entry '" + std::string(node_path)
			+ "' in JSON file '" + std::string(filename) + "'").c_str());
		return nullptr;
	}

	return node;
}

namespace json_native_modules {

	void *json_library_compiler_initializer() {
		c_json_file_manager *json_file_manager = new c_json_file_manager();
		return json_file_manager;
	}

	void json_library_compiler_deinitializer(void *library_context) {
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(library_context);
	}

	void read_real(
		const s_native_module_context &context,
		const char *filename,
		const char *path,
		real32 &result) {
		result = 0.0f;
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(json_file_manager, context.diagnostic, filename);
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(json_file, context.diagnostic, filename, path);
		if (!node) {
			return;
		}

		const c_json_node_number *number_node = node->try_get_as<c_json_node_number>();
		if (!number_node) {
			context.diagnostic->error(("Node '" + std::string(path) + "' in JSON file '"
				+ std::string(filename) + "' is not a number").c_str());
			return;
		}

		result = static_cast<real32>(number_node->get_value());
	}

	void read_real_array(
		const s_native_module_context &context,
		const char *filename,
		const char *path,
		c_native_module_real_array &result) {
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(json_file_manager, context.diagnostic, filename);
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(json_file, context.diagnostic, filename, path);
		if (!node) {
			return;
		}

		const c_json_node_array *array_node = node->try_get_as<c_json_node_array>();
		if (!array_node) {
			context.diagnostic->error(("Node '" + std::string(path) + "' in JSON file '"
				+ std::string(filename) + "' is not an array").c_str());
			return;
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			if (array_node->get_value()[index]->get_type() != e_json_node_type::k_number) {
				context.diagnostic->error(("Node '" + std::string(path) + "' in JSON file '"
					+ std::string(filename) + "' contains non-number elements").c_str());
				return;
			}
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			result.get_array().push_back(
				context.node_interface->create_constant_node(
					static_cast<real32>(array_node->get_value()[index]->get_as<c_json_node_number>().get_value())));
		}
	}

	void read_bool(
		const s_native_module_context &context,
		const char *filename,
		const char *path,
		bool &result) {
		result = false;
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(json_file_manager, context.diagnostic, filename);
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(json_file, context.diagnostic, filename, path);
		if (!node) {
			return;
		}

		const c_json_node_number *number_node = node->try_get_as<c_json_node_number>();
		if (!number_node) {
			context.diagnostic->error(("Node '" + std::string(path) + "' in JSON file '"
				+ std::string(filename) + "' is not a boolean").c_str());
			return;
		}

		result = number_node->get_value();
	}

	void read_bool_array(
		const s_native_module_context &context,
		const char *filename,
		const char *path,
		c_native_module_bool_array &result) {
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(json_file_manager, context.diagnostic, filename);
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(json_file, context.diagnostic, filename, path);
		if (!node) {
			return;
		}

		const c_json_node_array *array_node = node->try_get_as<c_json_node_array>();
		if (!array_node) {
			context.diagnostic->error(("Node '" + std::string(path) + "' in JSON file '"
				+ std::string(filename) + "' is not an array").c_str());
			return;
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			if (array_node->get_value()[index]->get_type() != e_json_node_type::k_boolean) {
				context.diagnostic->error(("Node '" + std::string(path) + "' in JSON file '"
					+ std::string(filename) + "' contains non-boolean elements").c_str());
				return;
			}
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			result.get_array().push_back(
				context.node_interface->create_constant_node(
					array_node->get_value()[index]->get_as<c_json_node_boolean>().get_value()));
		}
	}

	void read_string(
		const s_native_module_context &context,
		const char *filename,
		const char *path,
		c_native_module_string &result) {
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(json_file_manager, context.diagnostic, filename);
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(json_file, context.diagnostic, filename, path);
		if (!node) {
			return;
		}

		const c_json_node_string *string_node = node->try_get_as<c_json_node_string>();
		if (!string_node) {
			context.diagnostic->error(("Node '" + std::string(path) + "' in JSON file '"
				+ std::string(filename) + "' is not a string").c_str());
			return;
		}

		result.get_string() = string_node->get_value();

	}

	void read_string_array(
		const s_native_module_context &context,
		const char *filename,
		const char *path,
		c_native_module_string_array &result) {
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(json_file_manager, context.diagnostic, filename);
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(json_file, context.diagnostic, filename, path);
		if (!node) {
			return;
		}

		const c_json_node_array *array_node = node->try_get_as<c_json_node_array>();
		if (!array_node) {
			context.diagnostic->error(("Node '" + std::string(path) + "' in JSON file '"
				+ std::string(filename) + "' is not an array").c_str());
			return;
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			if (array_node->get_value()[index]->get_type() != e_json_node_type::k_string) {
				context.diagnostic->error(("Node '" + std::string(path) + "' in JSON file '"
					+ std::string(filename) + "' contains non-string elements").c_str());
				return;
			}
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			result.get_array().push_back(
				context.node_interface->create_constant_node(
					array_node->get_value()[index]->get_as<c_json_node_string>().get_value()));
		}
	}

}
