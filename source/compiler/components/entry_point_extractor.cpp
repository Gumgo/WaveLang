#include "compiler/components/entry_point_extractor.h"

static constexpr const char *k_voice_entry_point_module_name = "voice_main";
static constexpr const char *k_fx_entry_point_module_name = "fx_main";

void c_entry_point_extractor::extract_entry_points(
	c_compiler_context &context,
	c_ast_node_module_declaration *&voice_entry_point_out,
	c_ast_node_module_declaration *&fx_entry_point_out) {
	voice_entry_point_out = nullptr;
	fx_entry_point_out = nullptr;

	h_compiler_source_file source_file_handle = h_compiler_source_file::construct(0);
	const s_compiler_source_file &source_file = context.get_source_file(source_file_handle);
	const c_ast_node_scope *global_scope = source_file.ast->get_as<c_ast_node_scope>();

	static constexpr size_t k_voice_entry_point_index = 0;
	static constexpr size_t k_fx_entry_point_index = 1;
	static constexpr const char *k_entry_point_module_names[] = {
		k_voice_entry_point_module_name,
		k_fx_entry_point_module_name
	};

	bool failed = false;
	c_ast_node_module_declaration *entry_points[] = { nullptr, nullptr };
	size_t entry_point_input_counts[] = { 0, 0 };
	size_t entry_point_output_counts[] = { 0, 0 };
	std::vector<c_ast_node_declaration *> lookup_buffer;
	for (size_t index = 0; index < array_count(k_entry_point_module_names); index++) {
		global_scope->lookup_declarations_by_name(k_entry_point_module_names[index], lookup_buffer);

		if (lookup_buffer.empty()) {
			continue;
		}

		c_ast_node_module_declaration *entry_point = nullptr;
		for (c_ast_node_declaration *declaration : lookup_buffer) {
			c_ast_node_module_declaration *module_declaration =
				declaration->try_get_as<c_ast_node_module_declaration>();
			if (!module_declaration) {
				// Skip over declarations of the incorrect type rather than erroring
				continue;
			}

			if (entry_point) {
				// The entry point was ambiguous. Maybe in the future we'll support entry point overloads in case users
				// want synths that support a variable number of input/output channels.
				failed = true;
				entry_point = nullptr;
				context.error(
					e_compiler_error::k_ambiguous_entry_point,
					module_declaration->get_source_location(),
					"Entry point '%s' cannot be overloaded",
					k_entry_point_module_names[index]);
				break;
			}

			entry_point = module_declaration;
		}

		entry_points[index] = entry_point;

		if (entry_point) {
			c_ast_qualified_data_type real_data_type(
				c_ast_data_type(e_ast_primitive_type::k_real, false, 1),
				e_ast_data_mutability::k_variable);
			c_ast_qualified_data_type bool_data_type(
				c_ast_data_type(e_ast_primitive_type::k_bool, false, 1),
				e_ast_data_mutability::k_variable);

			// Validate input/output types
			if (!is_ast_data_type_assignable(entry_point->get_return_type(), bool_data_type)) {
				failed = true;
				context.error(
					e_compiler_error::k_invalid_entry_point,
					entry_point->get_source_location(),
					"Entry point '%s' must have return type '%s', not '%s'",
					entry_point->get_name(),
					bool_data_type.to_string().c_str(),
					entry_point->get_return_type().to_string().c_str());
			}

			for (size_t argument_index = 0; argument_index < entry_point->get_argument_count(); argument_index++) {
				const c_ast_node_module_declaration_argument *argument = entry_point->get_argument(argument_index);
				c_ast_qualified_data_type argument_data_type = argument->get_value_declaration()->get_data_type();

				bool assignable;
				if (argument->get_argument_direction() == e_ast_argument_direction::k_in) {
					if (entry_point_output_counts[index] > 0) {
						failed = true;
						context.error(
							e_compiler_error::k_invalid_entry_point,
							argument->get_source_location(),
							"Entry point '%s' in argument '%s' comes after an out argument",
							entry_point->get_name(),
							argument->get_name());
					}

					entry_point_input_counts[index]++;
					assignable = is_ast_data_type_assignable(real_data_type, argument_data_type);
				} else {
					wl_assert(argument->get_argument_direction() == e_ast_argument_direction::k_out);
					entry_point_output_counts[index]++;
					assignable = is_ast_data_type_assignable(argument_data_type, real_data_type);
				}

				if (!assignable) {
					failed = true;
					context.error(
						e_compiler_error::k_invalid_entry_point,
						argument->get_source_location(),
						"Entry point '%s' argument '%s' must be of type '%s', not '%s'",
						entry_point->get_name(),
						argument->get_name(),
						real_data_type.to_string().c_str(),
						argument_data_type.to_string().c_str());
				}

				if (argument->get_initialization_expression()) {
					context.warning(
						e_compiler_warning::k_entry_point_argument_initializer_ignored,
						argument->get_initialization_expression()->get_source_location(),
						"Initializer for entry point '%s' argument '%s' will be ignored",
						entry_point->get_name(),
						argument->get_name());
				}
			}

			if (entry_point_output_counts[index] == 0) {
				failed = true;
				context.error(
					e_compiler_error::k_invalid_entry_point,
					"Entry point '%s' has no out arguments",
					entry_point->get_name());
			}
		}
	}

	if (failed) {
		return;
	}

	if (!entry_points[k_voice_entry_point_index] && !entry_points[k_fx_entry_point_index]) {
		context.error(e_compiler_error::k_missing_entry_point, "No entry point provided");
		return;
	}

	if (entry_points[k_voice_entry_point_index] && entry_points[k_fx_entry_point_index]) {
		// Validate that the two entry point argument sets line up
		size_t voice_input_count = entry_point_input_counts[k_voice_entry_point_index];
		size_t voice_output_count = entry_point_output_counts[k_voice_entry_point_index];
		size_t fx_input_count = entry_point_input_counts[k_fx_entry_point_index];

		bool compatible;
		if (voice_input_count == 0) {
			compatible = fx_input_count >= voice_output_count;
		} else {
			compatible = fx_input_count == voice_output_count
				|| fx_input_count == voice_input_count + voice_output_count;
		}

		if (!compatible) {
			context.error(
				e_compiler_error::k_incompatible_entry_points,
				"Entry point '%s' is not compatible with entry point '%s'",
				entry_points[k_voice_entry_point_index]->get_name(),
				entry_points[k_fx_entry_point_index]->get_name());
			return;
		}
	}

	voice_entry_point_out = entry_points[k_voice_entry_point_index];
	fx_entry_point_out = entry_points[k_fx_entry_point_index];
}
