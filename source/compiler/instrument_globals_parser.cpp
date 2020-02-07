#include "compiler/instrument_globals_parser.h"
#include "compiler/preprocessor.h"

#include "execution_graph/execution_graph.h"

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

// Utilities for parsing arguments
static bool get_unsigned_integer(const s_token &token, uint32 &out_value);
static bool get_unsigned_nonzero_integer(const s_token &token, uint32 &out_value);
static bool get_bool(const s_token &token, bool &out_value);

// Preprocessor commands
static s_compiler_result preprocessor_command_max_voices(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output);

static s_compiler_result preprocessor_command_sample_rate(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output);

static s_compiler_result preprocessor_command_chunk_size(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output);

static s_compiler_result preprocessor_command_activate_fx_immediately(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output);

void c_instrument_globals_parser::register_preprocessor_commands(
	s_instrument_globals_context *globals_context) {
	c_preprocessor::register_preprocessor_command("max_voices", globals_context, preprocessor_command_max_voices);
	c_preprocessor::register_preprocessor_command("sample_rate", globals_context, preprocessor_command_sample_rate);
	c_preprocessor::register_preprocessor_command("chunk_size", globals_context, preprocessor_command_chunk_size);
	c_preprocessor::register_preprocessor_command(
		"activate_fx_immediately", globals_context, preprocessor_command_activate_fx_immediately);
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

static bool get_bool(const s_token &token, bool &out_value) {
	if (token.token_type != k_token_type_constant_bool) {
		return false;
	}

	wl_assert(token.token_string == k_token_type_constant_bool_false_string ||
		token.token_string == k_token_type_constant_bool_true_string);
	out_value = (token.token_string != k_token_type_constant_bool_false_string);
	return true;
}

static s_compiler_result preprocessor_command_max_voices(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output) {
	s_instrument_globals_context *globals_context = static_cast<s_instrument_globals_context *>(context);

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

	if (!get_unsigned_nonzero_integer(arguments.get_argument(0), globals_context->max_voices)) {
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
	s_instrument_globals_context *globals_context = static_cast<s_instrument_globals_context *>(context);

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

	for (size_t index = 0; index < arguments.get_argument_count(); index++) {
		uint32 sample_rate;
		if (!get_unsigned_nonzero_integer(arguments.get_argument(index), sample_rate)) {
			result.result = k_compiler_result_invalid_globals;
			result.source_location = arguments.get_argument(index).source_location;
			result.message = "Invalid sample_rate value '" +
				arguments.get_argument(index).token_string.to_std_string() + "'";
			return result;
		}

		for (size_t existing_index = 0; existing_index < globals_context->sample_rates.size(); existing_index++) {
			if (sample_rate == globals_context->sample_rates[existing_index]) {
				result.result = k_compiler_result_invalid_globals;
				result.source_location = arguments.get_argument(index).source_location;
				result.message = "Duplicate sample_rate value '" + std::to_string(sample_rate) + "' specified";
				return result;
			}
		}

		globals_context->sample_rates.push_back(sample_rate);
	}

	globals_context->sample_rate_command_executed = true;

	return result;
}

static s_compiler_result preprocessor_command_chunk_size(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output) {
	s_instrument_globals_context *globals_context = static_cast<s_instrument_globals_context *>(context);

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

	if (!get_unsigned_nonzero_integer(arguments.get_argument(0), globals_context->chunk_size)) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_argument(0).source_location;
		result.message = "Invalid chunk_size value '" + arguments.get_argument(0).token_string.to_std_string() + "'";
		return result;
	}

	globals_context->chunk_size_command_executed = true;

	return result;
}

static s_compiler_result preprocessor_command_activate_fx_immediately(
	void *context, const c_preprocessor_command_arguments &arguments, c_preprocessor_output &output) {
	s_instrument_globals_context *globals_context = static_cast<s_instrument_globals_context *>(context);

	s_compiler_result result;
	result.clear();

	if (globals_context->activate_fx_immediately_command_executed) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_command().source_location;
		result.message = "activate_fx_immediately specified multiple times";
		return result;
	}

	if (arguments.get_argument_count() != 1) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_command().source_location;
		result.message = "Incorrect number of activate_fx_immediately values specified";
		return result;
	}

	if (!get_bool(arguments.get_argument(0), globals_context->activate_fx_immediately)) {
		result.result = k_compiler_result_invalid_globals;
		result.source_location = arguments.get_argument(0).source_location;
		result.message = "Invalid activate_fx_immediately value '" +
			arguments.get_argument(0).token_string.to_std_string() + "'";
		return result;
	}

	globals_context->activate_fx_immediately_command_executed = true;

	return result;
}
