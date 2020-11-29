#include "common/utility/file_utility.h"

#include "compiler/compiler.h"
#include "compiler/compiler_context.h"
#include "compiler/components/ast_builder.h"
#include "compiler/components/entry_point_extractor.h"
#include "compiler/components/importer.h"
#include "compiler/components/instrument_globals_parser.h"
#include "compiler/components/instrument_variant_builder.h"
#include "compiler/components/instrument_variant_optimizer.h"
#include "compiler/components/lexer.h"
#include "compiler/components/parser.h"

#include "execution_graph/instrument.h"

#include <memory>
#include <vector>

static bool read_source_file(c_compiler_context &context, h_compiler_source_file source_file_handle);

c_instrument *c_compiler::compile(c_wrapped_array<void *> native_module_library_contexts, const char *source_filename) {
	wl_assert(source_filename);

	c_compiler_context context(native_module_library_contexts);

	// Add the initial source
	bool was_added;
	if (!context.get_or_add_source_file(source_filename, was_added).is_valid()) {
		context.error(e_compiler_error::k_failed_to_find_file, "Failed to find file '%s'", source_filename);
		return nullptr;
	}

	wl_assert(was_added);

	c_lexer::initialize();
	c_parser::initialize();
	c_importer::initialize();
	c_instrument_globals_parser::initialize();

	s_instrument_globals_context instrument_globals_context;
	instrument_globals_context.assign_defaults();

	// Iterate and process imports for each source file. This will continue looping until no new imports are discovered.
	for (size_t source_file_index = 0; source_file_index < context.get_source_file_count(); source_file_index++) {
		h_compiler_source_file source_file_handle = h_compiler_source_file::construct(source_file_index);
		s_compiler_source_file &source_file = context.get_source_file(source_file_handle);

		if (read_source_file(context, source_file_handle)) {
			if (c_lexer::process(context, source_file_handle)) {
				if (c_parser::process(context, source_file_handle)) {
					c_importer::resolve_imports(context, source_file_handle);
					c_instrument_globals_parser::parse_instrument_globals(
						context,
						source_file_handle,
						source_file_index == 0,
						instrument_globals_context);
				}
			}
		}
	}

	c_instrument_globals_parser::deinitialize();
	c_importer::deinitialize();
	c_parser::deinitialize();
	c_lexer::deinitialize();

	// Fail early if we ran into lexer, parser, import, or instrument globals errors. They're sure to cause all sorts of
	// issues further down the line.
	if (context.get_error_count() > 0) {
		return nullptr;
	}

	// Build all declarations
	for (size_t source_file_index = 0; source_file_index < context.get_source_file_count(); source_file_index++) {
		h_compiler_source_file source_file_handle = h_compiler_source_file::construct(source_file_index);
		c_ast_builder::build_ast_declarations(context, source_file_handle);
	}

	// Pull in imports and build definitions
	for (size_t source_file_index = 0; source_file_index < context.get_source_file_count(); source_file_index++) {
		h_compiler_source_file source_file_handle = h_compiler_source_file::construct(source_file_index);
		c_importer::add_imports_to_global_scope(context, source_file_handle);
		c_ast_builder::build_ast_definitions(context, source_file_handle);
	}

	// If AST generation had errors, don't attempt to build the instrument
	if (context.get_error_count() > 0) {
		return nullptr;
	}

	c_ast_node_module_declaration *voice_entry_point;
	c_ast_node_module_declaration *fx_entry_point;
	c_entry_point_extractor::extract_entry_points(
		context,
		voice_entry_point,
		fx_entry_point);

	if (!context.get_error_count() > 0) {
		return nullptr;
	}

	std::unique_ptr<c_instrument> instrument(new c_instrument());

	std::vector<s_instrument_globals> instrument_globals_set =
		instrument_globals_context.build_instrument_globals_set();
	for (const s_instrument_globals &instrument_globals : instrument_globals_set) {
		std::unique_ptr<c_instrument_variant> instrument_variant(
			c_instrument_variant_builder::build_instrument_variant(
				context,
				instrument_globals,
				voice_entry_point,
				fx_entry_point));

		if (!instrument_variant) {
			return nullptr;
		}

		if (!c_instrument_variant_optimizer::optimize_instrument_variant(context, *instrument_variant.get())) {
			return nullptr;
		}

		instrument->add_instrument_variant(instrument_variant.release());
	}

	wl_assert(instrument->validate());
	return instrument.release();
}

static bool read_source_file(c_compiler_context &context, h_compiler_source_file source_file_handle) {
	s_compiler_source_file &source_file = context.get_source_file(source_file_handle);

	e_read_full_file_result result = read_full_file(source_file.path.c_str(), source_file.source);
	switch (result) {
	case e_read_full_file_result::k_success:
		return true;

	case e_read_full_file_result::k_failed_to_open:
		context.error(
			e_compiler_error::k_failed_to_open_file,
			s_compiler_source_location(source_file_handle),
			"Failed to open source file '%s'",
			source_file.path.c_str());
		return false;

	case e_read_full_file_result::k_failed_to_read:
		context.error(
			e_compiler_error::k_failed_to_read_file,
			s_compiler_source_location(source_file_handle),
			"Failed to read source file '%s'",
			source_file.path.c_str());
		return false;

	case e_read_full_file_result::k_file_too_big:
		context.error(
			e_compiler_error::k_failed_to_read_file,
			s_compiler_source_location(source_file_handle),
			"Source file '%s' is too big",
			source_file.path.c_str());
		return false;

	default:
		wl_unreachable();
		return false;
	}
}
