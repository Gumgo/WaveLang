#include "common/utility/file_utility.h"

#include "execution_graph/execution_graph.h"
#include "execution_graph/instrument.h"

#include <fstream>

static const char k_format_identifier[] = { 'w', 'a', 'v', 'e', 'l', 'a', 'n', 'g' };

c_instrument::c_instrument() {
}

c_instrument::~c_instrument() {
	for (size_t index = 0; index < m_execution_graphs.size(); index++) {
		delete m_execution_graphs[index];
	}
}

e_instrument_result c_instrument::save(const char *fname) const {
	wl_assert(validate());

	std::ofstream out(fname, std::ios::binary);
	if (!out.is_open()) {
		return k_instrument_result_failed_to_write;
	}

	c_binary_file_writer writer(out);

	// Write identifier at the beginning
	for (size_t ch = 0; ch < NUMBEROF(k_format_identifier); ch++) {
		writer.write(k_format_identifier[ch]);
	}

	writer.write(k_instrument_format_version);

	// Write execution graph count followed by the list of execution graphs
	// $TODO a better format might be a table with file offsets to each graph
	writer.write(cast_integer_verify<uint32>(m_execution_graphs.size()));

	for (uint32 index = 0; index < m_execution_graphs.size(); index++) {
		e_instrument_result graph_result = m_execution_graphs[index]->save(out);
		if (graph_result != k_instrument_result_success) {
			return graph_result;
		}
	}

	if (out.fail()) {
		return k_instrument_result_failed_to_write;
	}

	return k_instrument_result_success;
}

e_instrument_result c_instrument::load(const char *fname) {
	wl_assert(m_execution_graphs.empty());

	std::ifstream in(fname, std::ios::binary);
	if (!in.is_open()) {
		return k_instrument_result_failed_to_read;
	}

	c_binary_file_reader reader(in);

	// Read the identifiers at the beginning of the file
	s_static_array<char, NUMBEROF(k_format_identifier)> format_identifier_buffer;
	for (size_t ch = 0; ch < NUMBEROF(k_format_identifier); ch++) {
		if (!reader.read(format_identifier_buffer[ch])) {
			return in.eof() ? k_instrument_result_invalid_header : k_instrument_result_failed_to_read;
		}
	}

	uint32 format_version;
	if (!reader.read(format_version)) {
		return in.eof() ? k_instrument_result_invalid_header : k_instrument_result_failed_to_read;
	}

	if (memcmp(k_format_identifier, &format_identifier_buffer, sizeof(k_format_identifier)) != 0) {
		return k_instrument_result_invalid_header;
	}

	if (format_version != k_instrument_format_version) {
		return k_instrument_result_version_mismatch;
	}

	uint32 execution_graph_count;
	if (!reader.read(execution_graph_count)) {
		return k_instrument_result_invalid_header;
	}

	for (uint32 index = 0; index < execution_graph_count; index++) {
		c_execution_graph *graph = new c_execution_graph();
		m_execution_graphs.push_back(graph);
		e_instrument_result graph_result = graph->load(in);
		if (graph_result != k_instrument_result_success) {
			return graph_result;
		}
	}

	return k_instrument_result_success;
}

bool c_instrument::validate() const {
	for (size_t index = 0; index < m_execution_graphs.size(); index++) {
		if (!m_execution_graphs[index]->validate()) {
			return false;
		}
	}

	return true;
}

void c_instrument::add_execution_graph(c_execution_graph *execution_graph) {
	m_execution_graphs.push_back(execution_graph);
}

uint32 c_instrument::get_execution_graph_count() const {
	return cast_integer_verify<uint32>(m_execution_graphs.size());
}

const c_execution_graph *c_instrument::get_execution_graph(uint32 index) const {
	return m_execution_graphs[index];
}

e_execution_graph_for_requirements_result c_instrument::get_execution_graph_for_requirements(
	const s_execution_graph_requirements &requirements, uint32 &out_execution_graph_index) const {
	int32 best_match_score = -1;
	size_t matches_for_this_score = 0;
	uint32 execution_graph_index = 0;

	for (uint32 index = 0; index < m_execution_graphs.size(); index++) {
		const s_execution_graph_globals &globals = m_execution_graphs[index]->get_globals();

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
			execution_graph_index = index;
		}
	}

	out_execution_graph_index = execution_graph_index;
	if (best_match_score == -1) {
		return k_execution_graph_for_requirements_result_no_match;
	} else if (matches_for_this_score > 1) {
		return k_execution_graph_for_requirements_result_ambiguous_matches;
	} else {
		return k_execution_graph_for_requirements_result_success;
	}
}
