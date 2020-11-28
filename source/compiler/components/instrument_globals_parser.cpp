#include "compiler/components/instrument_globals_parser.h"

#include "execution_graph/execution_graph.h"

#include "generated/wavelang_grammar.h"

#include <unordered_map>

using f_instrument_global_parser = void (*)(
	c_compiler_context &context,
	s_instrument_globals_context &instrument_globals_context,
	const s_token &command_token,
	c_wrapped_array<const s_token> tokens);

struct s_instrument_globals_parser_globals {
	std::unordered_map<std::string, f_instrument_global_parser> parsers;
};

static bool g_instrument_globals_parser_initialized = false;
static s_instrument_globals_parser_globals g_instrument_globals_parser_globals;

// Utilities for parsing arguments
static bool get_unsigned_integer(
	c_compiler_context &context,
	const s_token &command_token,
	const s_token &value_token,
	uint32 &value_out);
static bool get_unsigned_nonzero_integer(
	c_compiler_context &context,
	const s_token &command_token,
	const s_token &value_token,
	uint32 &value_out);
static bool get_bool(
	c_compiler_context &context,
	const s_token &command_token,
	const s_token &value_token,
	bool &value_out);

static void instrument_global_parser_max_voices(
	c_compiler_context &context,
	s_instrument_globals_context &instrument_globals_context,
	const s_token &command_token,
	c_wrapped_array<const s_token> tokens);
static void instrument_global_parser_sample_rate(
	c_compiler_context &context,
	s_instrument_globals_context &instrument_globals_context,
	const s_token &command_token,
	c_wrapped_array<const s_token> tokens);
static void instrument_global_parser_chunk_size(
	c_compiler_context &context,
	s_instrument_globals_context &instrument_globals_context,
	const s_token &command_token,
	c_wrapped_array<const s_token> tokens);
static void instrument_global_parser_activate_fx_immediately(
	c_compiler_context &context,
	s_instrument_globals_context &instrument_globals_context,
	const s_token &command_token,
	c_wrapped_array<const s_token> tokens);

struct s_instrument_global_parser {
	const char *name;
	f_instrument_global_parser parser;
};

// List of all instrument global parsers
static constexpr const s_instrument_global_parser k_instrument_global_parsers[] = {
	{ "max_voices", instrument_global_parser_max_voices },
	{ "sample_rate", instrument_global_parser_sample_rate },
	{ "chunk_size", instrument_global_parser_chunk_size },
	{ "activate_fx_immediately", instrument_global_parser_activate_fx_immediately }
};

class c_instrument_globals_visitor : public c_wavelang_lr_parse_tree_visitor {
public:
	c_instrument_globals_visitor(
		c_compiler_context &context,
		h_compiler_source_file source_file_handle,
		bool is_top_level_source_file,
		s_instrument_globals_context &instrument_globals_context);

protected:
	bool enter_import_list() override { return false; }

	void exit_instrument_global(const s_token &command) override;
	void exit_instrument_global_real_parameter(const s_token &parameter) override;
	void exit_instrument_global_bool_parameter(const s_token &parameter) override;
	void exit_instrument_global_string_parameter(const s_token &parameter) override;

	bool enter_global_scope(
		c_temporary_reference<c_AST_node_scope> &rule_head_context,
		c_AST_node_scope *&item_list) override {
		return false;
	}

private:
	c_compiler_context &m_context;
	h_compiler_source_file m_source_file_handle;
	bool m_is_top_level_source_file;
	s_instrument_globals_context &m_instrument_globals_context;

	// Build up tokens for the current instrument global here
	std::vector<s_token> m_tokens;

	void try_execute_parser(const s_token &command_token);
};

void s_instrument_globals_context::clear() {
	max_voices_command_executed = false;
	max_voices = 0;

	sample_rate_command_executed = false;
	sample_rates.clear();

	chunk_size_command_executed = false;
	chunk_size = 0;

	activate_fx_immediately_command_executed = false;
	activate_fx_immediately = false;
}

void s_instrument_globals_context::assign_defaults() {
	if (!max_voices_command_executed) {
		max_voices = 1;
	}

	if (!sample_rate_command_executed) {
		wl_assert(sample_rates.empty());
		sample_rates.push_back(0);
	}

	if (!chunk_size_command_executed) {
		chunk_size = 0;
	}

	if (!activate_fx_immediately_command_executed) {
		activate_fx_immediately = false;
	}
}

std::vector<s_instrument_globals> s_instrument_globals_context::build_instrument_globals_set() const {
	std::vector<s_instrument_globals> result;

	// Add loop nesting for each multi-valued execution graph global:
	for (size_t sample_rate_index = 0; sample_rate_index < sample_rates.size(); sample_rate_index++) {
		s_instrument_globals globals;
		globals.max_voices = max_voices;
		globals.sample_rate = sample_rates[sample_rate_index];
		globals.chunk_size = chunk_size;
		globals.activate_fx_immediately = activate_fx_immediately;

		result.push_back(globals);
	}

	return result;
}

void c_instrument_globals_parser::initialize() {
	wl_assert(!g_instrument_globals_parser_initialized);

	for (const s_instrument_global_parser &parser : k_instrument_global_parsers) {
		g_instrument_globals_parser_globals.parsers.insert(std::make_pair(parser.name, parser.parser));
	}

	g_instrument_globals_parser_initialized = true;
}

void c_instrument_globals_parser::deinitialize() {
	wl_assert(g_instrument_globals_parser_initialized);

	g_instrument_globals_parser_globals.parsers.clear();

	g_instrument_globals_parser_initialized = true;
}

void c_instrument_globals_parser::parse_instrument_globals(
	c_compiler_context &context,
	h_compiler_source_file source_file_handle,
	bool is_top_level_source_file,
	s_instrument_globals_context &instrument_globals_context) {
	wl_assert(g_instrument_globals_parser_initialized);
	c_instrument_globals_visitor visitor(
		context,
		source_file_handle,
		is_top_level_source_file,
		instrument_globals_context);
	visitor.visit();
}

static bool get_unsigned_integer(
	c_compiler_context &context,
	const s_token &command_token,
	const s_token &value_token,
	uint32 &value_out) {
	if (value_token.token_type != e_token_type::k_literal_real
		|| value_token.value.real_value < 0.0f) {
		context.error(
			e_compiler_error::k_invalid_instrument_global_parameters,
			value_token.source_location,
			"Instrument global '%s' value '%s' is not an unsigned integer",
			escape_token_string_for_output(command_token.token_string).c_str(),
			escape_token_string_for_output(value_token.token_string).c_str());
		return false;
	}

	uint32 value = static_cast<uint32>(value_token.value.real_value);
	if (value_token.value.real_value != static_cast<real32>(value)) {
		context.error(
			e_compiler_error::k_invalid_instrument_global_parameters,
			value_token.source_location,
			"Instrument global '%s' value '%s' is out of range",
			escape_token_string_for_output(command_token.token_string).c_str(),
			escape_token_string_for_output(value_token.token_string).c_str());
		return false;
	}

	value_out = value;
	return true;
}

static bool get_unsigned_nonzero_integer(
	c_compiler_context &context,
	const s_token &command_token,
	const s_token &value_token,
	uint32 &value_out) {
	if (value_token.token_type != e_token_type::k_literal_real
		|| value_token.value.real_value < 0.0f) {
		context.error(
			e_compiler_error::k_invalid_instrument_global_parameters,
			value_token.source_location,
			"Instrument global '%s' value '%s' is not a nonzero unsigned integer",
			escape_token_string_for_output(command_token.token_string).c_str(),
			escape_token_string_for_output(value_token.token_string).c_str());
		return false;
	}

	uint32 value = static_cast<uint32>(value_token.value.real_value);
	if (value_token.value.real_value != static_cast<real32>(value)) {
		context.error(
			e_compiler_error::k_invalid_instrument_global_parameters,
			value_token.source_location,
			"Instrument global '%s' value '%s' is out of range",
			escape_token_string_for_output(command_token.token_string).c_str(),
			escape_token_string_for_output(value_token.token_string).c_str());
		return false;
	}

	if (value == 0) {
		context.error(
			e_compiler_error::k_invalid_instrument_global_parameters,
			value_token.source_location,
			"Instrument global '%s' value '%s' is not a nonzero unsigned integer",
			escape_token_string_for_output(command_token.token_string).c_str(),
			escape_token_string_for_output(value_token.token_string).c_str());
		return false;
	}

	value_out = value;
	return true;
}

static bool get_bool(
	c_compiler_context &context,
	const s_token &command_token,
	const s_token &value_token,
	bool &value_out) {
	if (value_token.token_type != e_token_type::k_literal_bool) {
		context.error(
			e_compiler_error::k_invalid_instrument_global_parameters,
			value_token.source_location,
			"Instrument global '%s' value '%s' is not a boolean",
			escape_token_string_for_output(command_token.token_string).c_str(),
			escape_token_string_for_output(value_token.token_string).c_str());
		return false;
	}

	value_out = value_token.value.bool_value;
	return true;
}

static void instrument_global_parser_max_voices(
	c_compiler_context &context,
	s_instrument_globals_context &instrument_globals_context,
	const s_token &command_token,
	c_wrapped_array<const s_token> tokens) {
	if (instrument_globals_context.max_voices_command_executed) {
		context.error(
			e_compiler_error::k_duplicate_instrument_global,
			command_token.source_location,
			"Instrument global '%s' specified multiple times",
			escape_token_string_for_output(command_token.token_string).c_str());
		return;
	}

	if (tokens.get_count() != 1) {
		context.error(
			e_compiler_error::k_invalid_instrument_global_parameters,
			command_token.source_location,
			"Incorrect number of values specified for instrument global '%s'",
			escape_token_string_for_output(command_token.token_string).c_str());
		return;
	}

	if (!get_unsigned_nonzero_integer(context, command_token, tokens[0], instrument_globals_context.max_voices)) {
		return;
	}

	instrument_globals_context.max_voices_command_executed = true;
}

static void instrument_global_parser_sample_rate(
	c_compiler_context &context,
	s_instrument_globals_context &instrument_globals_context,
	const s_token &command_token,
	c_wrapped_array<const s_token> tokens) {
	if (instrument_globals_context.sample_rate_command_executed) {
		context.error(
			e_compiler_error::k_duplicate_instrument_global,
			command_token.source_location,
			"Instrument global '%s' specified multiple times",
			escape_token_string_for_output(command_token.token_string).c_str());
		return;
	}

	if (tokens.get_count() == 0) {
		context.error(
			e_compiler_error::k_invalid_instrument_global_parameters,
			command_token.source_location,
			"Incorrect number of values specified for instrument global '%s'",
			escape_token_string_for_output(command_token.token_string).c_str());
		return;
	}

	for (const s_token &token : tokens) {
		uint32 sample_rate;
		if (!get_unsigned_nonzero_integer(context, command_token, token, sample_rate)) {
			return;
		}

		for (uint32 existing_sample_rate : instrument_globals_context.sample_rates) {
			if (sample_rate == existing_sample_rate) {
				context.error(
					e_compiler_error::k_invalid_instrument_global_parameters,
					command_token.source_location,
					"Value '%u' specified multiple times for instrument global '%s'",
					sample_rate,
					escape_token_string_for_output(command_token.token_string).c_str());
				return;
			}
		}

		instrument_globals_context.sample_rates.push_back(sample_rate);
	}

	instrument_globals_context.sample_rate_command_executed = true;
}

static void instrument_global_parser_chunk_size(
	c_compiler_context &context,
	s_instrument_globals_context &instrument_globals_context,
	const s_token &command_token,
	c_wrapped_array<const s_token> tokens) {
	if (instrument_globals_context.chunk_size_command_executed) {
		context.error(
			e_compiler_error::k_duplicate_instrument_global,
			command_token.source_location,
			"Instrument global '%s' specified multiple times",
			escape_token_string_for_output(command_token.token_string).c_str());
		return;
	}

	if (tokens.get_count() != 1) {
		context.error(
			e_compiler_error::k_invalid_instrument_global_parameters,
			command_token.source_location,
			"Incorrect number of values specified for instrument global '%s'",
			escape_token_string_for_output(command_token.token_string).c_str());
		return;
	}

	if (!get_unsigned_nonzero_integer(context, command_token, tokens[0], instrument_globals_context.chunk_size)) {
		return;
	}

	instrument_globals_context.chunk_size_command_executed = true;
}

static void instrument_global_parser_activate_fx_immediately(
	c_compiler_context &context,
	s_instrument_globals_context &instrument_globals_context,
	const s_token &command_token,
	c_wrapped_array<const s_token> tokens) {
	if (instrument_globals_context.activate_fx_immediately_command_executed) {
		context.error(
			e_compiler_error::k_duplicate_instrument_global,
			command_token.source_location,
			"Instrument global '%s' specified multiple times",
			escape_token_string_for_output(command_token.token_string).c_str());
		return;
	}

	if (tokens.get_count() != 1) {
		context.error(
			e_compiler_error::k_invalid_instrument_global_parameters,
			command_token.source_location,
			"Incorrect number of values specified for instrument global '%s'",
			escape_token_string_for_output(command_token.token_string).c_str());
		return;
	}

	if (!get_bool(context, command_token, tokens[0], instrument_globals_context.activate_fx_immediately)) {
		return;
	}

	instrument_globals_context.activate_fx_immediately_command_executed = true;
}

c_instrument_globals_visitor::c_instrument_globals_visitor(
	c_compiler_context &context,
	h_compiler_source_file source_file_handle,
	bool is_top_level_source_file,
	s_instrument_globals_context &instrument_globals_context)
	: c_wavelang_lr_parse_tree_visitor(
		context.get_source_file(source_file_handle).parse_tree,
		context.get_source_file(source_file_handle).tokens)
	, m_context(context)
	, m_source_file_handle(source_file_handle)
	, m_is_top_level_source_file(is_top_level_source_file)
	, m_instrument_globals_context(instrument_globals_context) {}

void c_instrument_globals_visitor::exit_instrument_global(const s_token &command) {
	try_execute_parser(command);
	m_tokens.clear();
}

void c_instrument_globals_visitor::exit_instrument_global_real_parameter(const s_token &parameter) {
	m_tokens.push_back(parameter);
}

void c_instrument_globals_visitor::exit_instrument_global_bool_parameter(const s_token &parameter) {
	m_tokens.push_back(parameter);
}

void c_instrument_globals_visitor::exit_instrument_global_string_parameter(const s_token &parameter) {
	m_tokens.push_back(parameter);
}

void c_instrument_globals_visitor::try_execute_parser(const s_token &command_token) {
	auto iter = g_instrument_globals_parser_globals.parsers.find(std::string(command_token.token_string));
	if (iter == g_instrument_globals_parser_globals.parsers.end()) {
		m_context.error(
			e_compiler_error::k_unrecognized_instrument_global,
			command_token.source_location,
			"Unrecognized instrument global '%s'",
			escape_token_string_for_output(command_token.token_string).c_str());
		return;
	}

	if (m_is_top_level_source_file) {
		m_context.error(
			e_compiler_error::k_illegal_instrument_global,
			command_token.source_location,
			"Instrument globals must be specified in the top-level source file",
			escape_token_string_for_output(command_token.token_string).c_str());
		return;
	}

	iter->second(
		m_context,
		m_instrument_globals_context,
		command_token,
		c_wrapped_array<const s_token>(m_tokens));
}
