#include "compiler/execution_graph_globals_parser.h"
#include "compiler/preprocessor.h"
#include "execution_graph/execution_graph.h"

void s_execution_graph_globals_context::clear() {
	max_voices_command_executed = false;
	sample_rate_command_executed = false;
	chunk_size_command_executed = false;

	globals->max_voices = 1;
	// $TODO other defaults as they are created
}

// Utilities for parsing arguments
static bool get_unsigned_integer(const s_token &token, uint32 &out_value);
static bool get_unsigned_nonzero_integer(const s_token &token, uint32 &out_value);

// Preprocessor commands
static s_compiler_result preprocessor_command_max_voices(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output);

static s_compiler_result preprocessor_command_sample_rate(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output);

static s_compiler_result preprocessor_command_chunk_size(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output);

void c_execution_graph_globals_parser::register_preprocessor_commands(
	s_execution_graph_globals_context *globals_context) {
	c_preprocessor::register_preprocessor_command("max_voices", globals_context, preprocessor_command_max_voices);
	c_preprocessor::register_preprocessor_command("sample_rate", globals_context, preprocessor_command_sample_rate);
	c_preprocessor::register_preprocessor_command("chunk_size", globals_context, preprocessor_command_chunk_size);
}

static bool get_unsigned_integer(const s_token &token, uint32 &out_value) {
	if (token.token_type != k_token_type_constant_real) {
		return false;
	}

	real32 value = std::stof(token.token_string.to_std_string());
	if (value < 0.0f || value != std::floor(value)) {
		return false;
	}

	out_value = static_cast<uint32>(value);
	return true;
}

static bool get_unsigned_nonzero_integer(const s_token &token, uint32 &out_value) {
	uint32 unsigned_value = 0;
	if (!get_unsigned_integer(token, unsigned_value) || unsigned_value == 0) {
		return false;
	}

	out_value = unsigned_value;
	return true;
}

static s_compiler_result preprocessor_command_max_voices(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output) {
	s_execution_graph_globals_context *globals_context = static_cast<s_execution_graph_globals_context *>(context);

	s_compiler_result result;
	result.clear();

	if (globals_context->max_voices_command_executed) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_command().source_location;
		result.message = "max_voices specified multiple times";
		return result;
	}

	if (arguments.get_argument_count() != 1) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_command().source_location;
		result.message = "Incorrect number of max_voices values specified";
		return result;
	}

	if (!get_unsigned_nonzero_integer(arguments.get_argument(0), globals_context->globals->max_voices)) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_argument(0).source_location;
		result.message = "Invalid max_voices value '" + arguments.get_argument(0).token_string.to_std_string() + "'";
		return result;
	}

	globals_context->max_voices_command_executed = true;

	return result;
}

static s_compiler_result preprocessor_command_sample_rate(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output) {
	s_execution_graph_globals_context *globals_context = static_cast<s_execution_graph_globals_context *>(context);

	s_compiler_result result;
	result.clear();

	if (globals_context->sample_rate_command_executed) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_command().source_location;
		result.message = "sample_rate specified multiple times";
		return result;
	}

	if (arguments.get_argument_count() == 0) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_command().source_location;
		result.message = "No sample_rate values specified";
		return result;
	}

	// $TODO resize sample_rate array to the argument size
	for (size_t index = 0; index < arguments.get_argument_count(); index++) {
		uint32 sample_rate; // $TODO assign to the real value
		if (!get_unsigned_nonzero_integer(arguments.get_argument(index), sample_rate)) {
			result.result = k_compiler_result_invalid_globals;
			result.source_location = arguments.get_argument(index).source_location;
			result.message = "Invalid sample_rate value '" +
				arguments.get_argument(index).token_string.to_std_string() + "'";
			return result;
		}
	}

	globals_context->sample_rate_command_executed = true;

	return result;
}

static s_compiler_result preprocessor_command_chunk_size(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output) {
	s_execution_graph_globals_context *globals_context = static_cast<s_execution_graph_globals_context *>(context);

	s_compiler_result result;
	result.clear();

	if (globals_context->chunk_size_command_executed) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_command().source_location;
		result.message = "chunk_size specified multiple times";
		return result;
	}

	if (arguments.get_argument_count() != 1) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_command().source_location;
		result.message = "Incorrect number of chunk_size values specified";
		return result;
	}

	uint32 chunk_size; // $TODO assign to the real value
	if (!get_unsigned_nonzero_integer(arguments.get_argument(0), chunk_size)) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_argument(0).source_location;
		result.message = "Invalid chunk_size value '" + arguments.get_argument(0).token_string.to_std_string() + "'";
		return result;
	}

	globals_context->chunk_size_command_executed = true;

	return result;
}