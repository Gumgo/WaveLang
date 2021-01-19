#include "common/utility/file_utility.h"

#include "instrument/instrument.h"
#include "instrument/instrument_globals.h"
#include "instrument/native_module_graph.h"

static constexpr char k_format_identifier[] = { 'w', 'a', 'v', 'e', 'l', 'a', 'n', 'g' };

void s_delete_native_module_graph::operator()(c_native_module_graph *native_module_graph) {
	delete native_module_graph;
}

c_instrument_variant::c_instrument_variant() {
	zero_type(&m_instrument_globals);
}

e_instrument_result c_instrument_variant::save(std::ofstream &out) const {
	c_binary_file_writer writer(out);

	// Write the globals
	writer.write(m_instrument_globals.max_voices);
	writer.write(m_instrument_globals.sample_rate);
	writer.write(m_instrument_globals.chunk_size);
	writer.write(m_instrument_globals.activate_fx_immediately);

	// Write each graph
	writer.write(m_voice_native_module_graph != nullptr);
	if (m_voice_native_module_graph) {
		e_instrument_result graph_result = m_voice_native_module_graph->save(out);
		if (graph_result != e_instrument_result::k_success) {
			return graph_result;
		}
	}

	writer.write(m_fx_native_module_graph != nullptr);
	if (m_fx_native_module_graph) {
		e_instrument_result graph_result = m_fx_native_module_graph->save(out);
		if (graph_result != e_instrument_result::k_success) {
			return graph_result;
		}
	}

	if (out.fail()) {
		return e_instrument_result::k_failed_to_write;
	}

	return e_instrument_result::k_success;
}

e_instrument_result c_instrument_variant::load(std::ifstream &in) {
	c_binary_file_reader reader(in);

	// Read the globals
	if (!reader.read(m_instrument_globals.max_voices)
		|| !reader.read(m_instrument_globals.sample_rate)
		|| !reader.read(m_instrument_globals.chunk_size)
		|| !reader.read(m_instrument_globals.activate_fx_immediately)) {
		return in.eof() ? e_instrument_result::k_invalid_globals : e_instrument_result::k_failed_to_read;
	}

	// Read each graph
	bool has_voice_graph;
	if (!reader.read(has_voice_graph)) {
		return in.eof() ? e_instrument_result::k_invalid_graph : e_instrument_result::k_failed_to_read;
	}

	if (has_voice_graph) {
		m_voice_native_module_graph.reset(new c_native_module_graph());
		e_instrument_result graph_result = m_voice_native_module_graph->load(in);
		if (graph_result != e_instrument_result::k_success) {
			return graph_result;
		}
	}

	bool has_fx_graph;
	if (!reader.read(has_fx_graph)) {
		return in.eof() ? e_instrument_result::k_invalid_graph : e_instrument_result::k_failed_to_read;
	}

	if (has_fx_graph) {
		m_fx_native_module_graph.reset(new c_native_module_graph());
		e_instrument_result graph_result = m_fx_native_module_graph->load(in);
		if (graph_result != e_instrument_result::k_success) {
			return graph_result;
		}
	}

	return e_instrument_result::k_success;
}

bool c_instrument_variant::validate() const {
	if (!m_voice_native_module_graph && !m_fx_native_module_graph) {
		return false;
	}

	if ((m_voice_native_module_graph && !m_voice_native_module_graph->validate())
		|| (m_fx_native_module_graph && !m_fx_native_module_graph->validate())) {
		return false;
	}

	if (m_voice_native_module_graph && m_fx_native_module_graph) {
		uint32 voice_graph_output_count = 0;
		for (h_graph_node node_handle = m_voice_native_module_graph->nodes_begin();
			node_handle.is_valid();
			node_handle = m_voice_native_module_graph->nodes_next(node_handle)) {
			if (m_voice_native_module_graph->get_node_type(node_handle) == e_native_module_graph_node_type::k_output) {
				uint32 output_index = m_voice_native_module_graph->get_output_node_output_index(node_handle);
				if (output_index != c_native_module_graph::k_remain_active_output_index) {
					voice_graph_output_count++;
				}
			}
		}

		uint32 fx_graph_input_count = 0;
		for (h_graph_node node_handle = m_fx_native_module_graph->nodes_begin();
			node_handle.is_valid();
			node_handle = m_fx_native_module_graph->nodes_next(node_handle)) {
			if (m_fx_native_module_graph->get_node_type(node_handle) == e_native_module_graph_node_type::k_input) {
				fx_graph_input_count++;
			}
		}

		if (voice_graph_output_count != fx_graph_input_count) {
			return false;
		}
	}

	// Validate globals
	if (m_instrument_globals.max_voices < 1) {
		return false; // $TODO $INPUT input-only graphs can have 0 voices
	}

	return true;
}

void c_instrument_variant::set_instrument_globals(const s_instrument_globals &instrument_globals) {
	m_instrument_globals = instrument_globals;
}

void c_instrument_variant::set_voice_native_module_graph(c_native_module_graph *native_module_graph) {
	wl_assert(native_module_graph);
	wl_assert(!m_voice_native_module_graph);
	m_voice_native_module_graph.reset(native_module_graph);
}

void c_instrument_variant::set_fx_native_module_graph(c_native_module_graph *native_module_graph) {
	wl_assert(native_module_graph);
	wl_assert(!m_fx_native_module_graph);
	m_fx_native_module_graph.reset(native_module_graph);
}

const s_instrument_globals &c_instrument_variant::get_instrument_globals() const {
	return m_instrument_globals;
}

c_native_module_graph *c_instrument_variant::get_voice_native_module_graph() {
	return m_voice_native_module_graph.get();
}

const c_native_module_graph *c_instrument_variant::get_voice_native_module_graph() const {
	return m_voice_native_module_graph.get();
}

c_native_module_graph *c_instrument_variant::get_fx_native_module_graph() {
	return m_fx_native_module_graph.get();
}

const c_native_module_graph *c_instrument_variant::get_fx_native_module_graph() const {
	return m_fx_native_module_graph.get();
}

e_instrument_result c_instrument::save(const char *fname) const {
	wl_assert(validate());

	std::ofstream out(fname, std::ios::binary);
	if (!out.is_open()) {
		return e_instrument_result::k_failed_to_write;
	}

	c_binary_file_writer writer(out);

	// Write identifier at the beginning
	for (size_t ch = 0; ch < array_count(k_format_identifier); ch++) {
		writer.write(k_format_identifier[ch]);
	}

	writer.write(k_instrument_format_version);

	// $TODO $PLUGIN write out all library IDs and versions used in any graph

	// Write instrument variant count followed by the list of instrument variants
	// $TODO a better format might be a table with file offsets to each variant
	writer.write(cast_integer_verify<uint32>(m_instrument_variants.size()));

	for (uint32 index = 0; index < m_instrument_variants.size(); index++) {
		e_instrument_result instrument_variant_result = m_instrument_variants[index]->save(out);
		if (instrument_variant_result != e_instrument_result::k_success) {
			return instrument_variant_result;
		}
	}

	if (out.fail()) {
		return e_instrument_result::k_failed_to_write;
	}

	return e_instrument_result::k_success;
}

e_instrument_result c_instrument::load(const char *fname) {
	wl_assert(m_instrument_variants.empty());

	std::ifstream in(fname, std::ios::binary);
	if (!in.is_open()) {
		return e_instrument_result::k_failed_to_read;
	}

	c_binary_file_reader reader(in);

	// Read the identifiers at the beginning of the file
	s_static_array<char, array_count(k_format_identifier)> format_identifier_buffer;
	for (size_t ch = 0; ch < array_count(k_format_identifier); ch++) {
		if (!reader.read(format_identifier_buffer[ch])) {
			return in.eof() ? e_instrument_result::k_invalid_header : e_instrument_result::k_failed_to_read;
		}
	}

	uint32 format_version;
	if (!reader.read(format_version)) {
		return in.eof() ? e_instrument_result::k_invalid_header : e_instrument_result::k_failed_to_read;
	}

	if (memcmp(k_format_identifier, &format_identifier_buffer, sizeof(k_format_identifier)) != 0) {
		return e_instrument_result::k_invalid_header;
	}

	if (format_version != k_instrument_format_version) {
		return e_instrument_result::k_version_mismatch;
	}

	uint32 instrument_variant_count;
	if (!reader.read(instrument_variant_count)) {
		return e_instrument_result::k_invalid_header;
	}

	for (uint32 index = 0; index < instrument_variant_count; index++) {
		c_instrument_variant *variant = new c_instrument_variant();
		m_instrument_variants.emplace_back(variant);
		e_instrument_result instrument_variant_result = variant->load(in);
		if (instrument_variant_result != e_instrument_result::k_success) {
			return instrument_variant_result;
		}
	}

	if (!validate()) {
		return e_instrument_result::k_validation_failure;
	}

	return e_instrument_result::k_success;
}

bool c_instrument::validate() const {
	for (size_t index = 0; index < m_instrument_variants.size(); index++) {
		if (!m_instrument_variants[index]->validate()) {
			return false;
		}
	}

	return true;
}

void c_instrument::add_instrument_variant(c_instrument_variant *instrument_variant) {
	m_instrument_variants.emplace_back(instrument_variant);
}

uint32 c_instrument::get_instrument_variant_count() const {
	return cast_integer_verify<uint32>(m_instrument_variants.size());
}

const c_instrument_variant *c_instrument::get_instrument_variant(uint32 index) const {
	return m_instrument_variants[index].get();
}

e_instrument_variant_for_requirements_result c_instrument::get_instrument_variant_for_requirements(
	const s_instrument_variant_requirements &requirements,
	uint32 &instrument_variant_index_out) const {
	int32 best_match_score = -1;
	size_t matches_for_this_score = 0;
	uint32 instrument_variant_index = 0;

	for (uint32 index = 0; index < m_instrument_variants.size(); index++) {
		const s_instrument_globals &globals = m_instrument_variants[index]->get_instrument_globals();

		int32 score = 0;

		// Currently sample rate is the only thing that can contribute to a match, so ambiguity is not possible

		if (globals.sample_rate == 0) {
			// Match, but don't increase the score because 0 matches any sample rate
		} else if (globals.sample_rate == requirements.sample_rate) {
			// Sample rate matches, increase the score
			score++;
		} else {
			// Sample rate doesn't match
			continue;
		}

		if (score == best_match_score) {
			matches_for_this_score++;
		} else if (score > best_match_score) {
			best_match_score = score;
			matches_for_this_score = 1;
			instrument_variant_index = index;
		}
	}

	instrument_variant_index_out = instrument_variant_index;
	if (best_match_score == -1) {
		return e_instrument_variant_for_requirements_result::k_no_match;
	} else if (matches_for_this_score > 1) {
		return e_instrument_variant_for_requirements_result::k_ambiguous_matches;
	} else {
		return e_instrument_variant_for_requirements_result::k_success;
	}
}
