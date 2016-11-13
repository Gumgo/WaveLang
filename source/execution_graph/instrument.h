#ifndef WAVELANG_EXECUTION_GRAPH_INSTRUMENT_H__
#define WAVELANG_EXECUTION_GRAPH_INSTRUMENT_H__

#include "common/common.h"

#include "execution_graph/instrument_constants.h"

#include <vector>

class c_execution_graph;

// Used to select the appropriate execution graph from an instrument
struct s_execution_graph_requirements {
	uint32 sample_rate;
};

enum e_execution_graph_for_requirements_result {
	k_execution_graph_for_requirements_result_success,
	k_execution_graph_for_requirements_result_no_match,
	k_execution_graph_for_requirements_result_ambiguous_matches,

	k_execution_graph_for_requirements_result_count
};

// An instrument contains compiled execution graphs. An instrument may contain more than one execution graph if the
// execution graph globals are context-specific - e.g. an execution graph may be compiled several times for different
// sample rates.
class c_instrument {
public:
	c_instrument();
	~c_instrument();

	e_instrument_result save(const char *fname) const;
	e_instrument_result load(const char *fname);

	bool validate() const;

	// Takes ownership of the execution graph, which should be allocated using new
	void add_execution_graph(c_execution_graph *execution_graph);

	uint32 get_execution_graph_count() const;
	const c_execution_graph *get_execution_graph(uint32 index) const;

	// Chooses the execution graph which best matches the given requirements
	e_execution_graph_for_requirements_result get_execution_graph_for_requirements(
		const s_execution_graph_requirements &requirements, uint32 &out_execution_graph_index) const;

private:
	std::vector<c_execution_graph *> m_execution_graphs;
};

#endif // WAVELANG_EXECUTION_GRAPH_INSTRUMENT_H__
