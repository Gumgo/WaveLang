#include "instrument/native_module_registration.h"
#include "instrument/native_modules/json/json_file_manager.h"

static const c_json_file *load_json_file_or_report_error(
	c_json_file_manager *json_file_manager,
	c_native_module_diagnostic_interface *diagnostic,
	const char *filename);
static const c_json_node *get_json_node_or_report_error(
	const c_json_file *json_file,
	c_native_module_diagnostic_interface *diagnostic,
	const char *filename,
	const char *node_path);

static const c_json_file *load_json_file_or_report_error(
	c_json_file_manager *json_file_manager,
	c_native_module_diagnostic_interface *diagnostic,
	const char *filename) {
	const c_json_file *json_file;
	s_json_result result = json_file_manager->load_json_file(filename, &json_file);
	switch (result.result) {
	case e_json_result::k_success:
		// Nothing to do
		break;

	case e_json_result::k_file_error:
		diagnostic->error("Failed to load JSON file '%s'", filename);
		break;

	case e_json_result::k_parse_error:
		diagnostic->error(
			"Failed to parse JSON file '%s' (line %u, character %u)",
			filename,
			result.parse_error_line,
			result.parse_error_character);
		break;

	default:
		wl_unreachable();
	}

	return json_file;
}

static const c_json_node *get_json_node_or_report_error(
	const c_json_file *json_file,
	c_native_module_diagnostic_interface *diagnostic,
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
		wl_argument(in const string, filename),
		wl_argument(in const string, path),
		wl_argument(return out const real, result)) {
		*result = 0.0f;
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(
			json_file_manager,
			context.diagnostic_interface,
			filename->get_string().c_str());
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(
			json_file,
			context.diagnostic_interface,
			filename->get_string().c_str(),
			path->get_string().c_str());
		if (!node) {
			return;
		}

		const c_json_node_number *number_node = node->try_get_as<c_json_node_number>();
		if (!number_node) {
			context.diagnostic_interface->error(
				"Node '%s' in JSON file '%s' is not a number",
				path->get_string().c_str(),
				filename->get_string().c_str());
			return;
		}

		*result = static_cast<real32>(number_node->get_value());
	}

	void read_real_array(
		const s_native_module_context &context,
		wl_argument(in const string, filename),
		wl_argument(in const string, path),
		wl_argument(return out const real[], result)) {
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(
			json_file_manager,
			context.diagnostic_interface,
			filename->get_string().c_str());
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(
			json_file,
			context.diagnostic_interface,
			filename->get_string().c_str(),
			path->get_string().c_str());
		if (!node) {
			return;
		}

		const c_json_node_array *array_node = node->try_get_as<c_json_node_array>();
		if (!array_node) {
			context.diagnostic_interface->error(
				"Node '%s' in JSON file '%s' is not an array",
				path->get_string().c_str(),
				filename->get_string().c_str());
			return;
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			if (array_node->get_value()[index]->get_type() != e_json_node_type::k_number) {
				context.diagnostic_interface->error(
					"Node '%s' in JSON file '%s' contains non-number elements",
					path->get_string().c_str(),
					filename->get_string().c_str());
				return;
			}
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			result->get_array().push_back(
				static_cast<real32>(array_node->get_value()[index]->get_as<c_json_node_number>().get_value()));
		}
	}

	void read_bool(
		const s_native_module_context &context,
		wl_argument(in const string, filename),
		wl_argument(in const string, path),
		wl_argument(return out const bool, result)) {
		*result = false;
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(
			json_file_manager,
			context.diagnostic_interface,
			filename->get_string().c_str());
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(
			json_file,
			context.diagnostic_interface,
			filename->get_string().c_str(),
			path->get_string().c_str());
		if (!node) {
			return;
		}

		const c_json_node_number *number_node = node->try_get_as<c_json_node_number>();
		if (!number_node) {
			context.diagnostic_interface->error(
				"Node '%s' in JSON file '%s' is not a boolean",
				path->get_string().c_str(),
				filename->get_string().c_str());
			return;
		}

		*result = number_node->get_value();
	}

	void read_bool_array(
		const s_native_module_context &context,
		wl_argument(in const string, filename),
		wl_argument(in const string, path),
		wl_argument(return out const bool[], result)) {
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(
			json_file_manager,
			context.diagnostic_interface,
			filename->get_string().c_str());
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(
			json_file,
			context.diagnostic_interface,
			filename->get_string().c_str(),
			path->get_string().c_str());
		if (!node) {
			return;
		}

		const c_json_node_array *array_node = node->try_get_as<c_json_node_array>();
		if (!array_node) {
			context.diagnostic_interface->error(
				"Node '%s' in JSON file '%s' is not an array",
				path->get_string().c_str(),
				filename->get_string().c_str());
			return;
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			if (array_node->get_value()[index]->get_type() != e_json_node_type::k_boolean) {
				context.diagnostic_interface->error(
					"Node '%s' in JSON file '%s' contains non-boolean elements",
					path->get_string().c_str(),
					filename->get_string().c_str());
				return;
			}
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			result->get_array().push_back(
				array_node->get_value()[index]->get_as<c_json_node_boolean>().get_value());
		}
	}

	void read_string(
		const s_native_module_context &context,
		wl_argument(in const string, filename),
		wl_argument(in const string, path),
		wl_argument(return out const string, result)) {
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(
			json_file_manager,
			context.diagnostic_interface,
			filename->get_string().c_str());
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(
			json_file,
			context.diagnostic_interface,
			filename->get_string().c_str(),
			path->get_string().c_str());
		if (!node) {
			return;
		}

		const c_json_node_string *string_node = node->try_get_as<c_json_node_string>();
		if (!string_node) {
			context.diagnostic_interface->error(
				"Node '%s' in JSON file '%s' is not a string",
				path->get_string().c_str(),
				filename->get_string().c_str());
			return;
		}

		result->get_string() = string_node->get_value();
	}

	void read_string_array(
		const s_native_module_context &context,
		wl_argument(in const string, filename),
		wl_argument(in const string, path),
		wl_argument(return out const string[], result)) {
		c_json_file_manager *json_file_manager = static_cast<c_json_file_manager *>(context.library_context);
		const c_json_file *json_file = load_json_file_or_report_error(
			json_file_manager,
			context.diagnostic_interface,
			filename->get_string().c_str());
		if (!json_file) {
			return;
		}

		const c_json_node *node = get_json_node_or_report_error(
			json_file,
			context.diagnostic_interface,
			filename->get_string().c_str(),
			path->get_string().c_str());
		if (!node) {
			return;
		}

		const c_json_node_array *array_node = node->try_get_as<c_json_node_array>();
		if (!array_node) {
			context.diagnostic_interface->error(
				"Node '%s' in JSON file '%s' is not an array",
				path->get_string().c_str(),
				filename->get_string().c_str());
			return;
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			if (array_node->get_value()[index]->get_type() != e_json_node_type::k_string) {
				context.diagnostic_interface->error(
					"Node '%s' in JSON file '%s' contains non-string elements",
					path->get_string().c_str(),
					filename->get_string().c_str());
				return;
			}
		}

		for (size_t index = 0; index < array_node->get_value().get_count(); index++) {
			result->get_array().push_back(
				array_node->get_value()[index]->get_as<c_json_node_string>().get_value());
		}
	}


	void scrape_native_modules() {
		static constexpr uint32 k_json_library_id = 9;
		wl_native_module_library(k_json_library_id, "json", 0)
			.set_compiler_initializer(json_library_compiler_initializer)
			.set_compiler_deinitializer(json_library_compiler_deinitializer);

		wl_native_module(0x24e6ff0c, "read_real")
			.set_compile_time_call<read_real>();

		wl_native_module(0x24ed0917, "read_real_array")
			.set_compile_time_call<read_real_array>();

		wl_native_module(0x77adcb5c, "read_bool")
			.set_compile_time_call<read_bool>();

		wl_native_module(0xf8c9553d, "read_bool_array")
			.set_compile_time_call<read_bool_array>();

		wl_native_module(0x4a8c9808, "read_string")
			.set_compile_time_call<read_string>();

		wl_native_module(0xa684c942, "read_string_array")
			.set_compile_time_call<read_string_array>();

		wl_end_active_library_native_module_registration();
	}

}
