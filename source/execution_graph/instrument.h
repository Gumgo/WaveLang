#pragma once

#include "common/common.h"

#include "execution_graph/instrument_constants.h"
#include "execution_graph/instrument_globals.h"

#include <fstream>
#include <vector>

class c_execution_graph;

// Used to select the appropriate instrument variant from an instrument
struct s_instrument_variant_requirements {
	uint32 sample_rate;
};

enum class e_instrument_variant_for_requirements_result {
	k_success,
	k_no_match,
	k_ambiguous_matches,

	k_count
};

// An instrument variant consists of an instance of instrument globals and either a voice execution graph, an FX
// execution graph, or both
class c_instrument_variant {
public:
	c_instrument_variant();
	~c_instrument_variant();

	e_instrument_result save(std::ofstream &out) const;
	e_instrument_result load(std::ifstream &in);

	bool validate() const;

	void set_instrument_globals(const s_instrument_globals &instrument_globals);

	// These functions take ownership of the execution graph, which should be allocated using new
	void set_voice_execution_graph(c_execution_graph *execution_graph);
	void set_fx_execution_graph(c_execution_graph *execution_graph);

	const s_instrument_globals &get_instrument_globals() const;
	c_execution_graph *get_voice_execution_graph();
	const c_execution_graph *get_voice_execution_graph() const;
	c_execution_graph *get_fx_execution_graph();
	const c_execution_graph *get_fx_execution_graph() const;

private:
	s_instrument_globals m_instrument_globals;
	c_execution_graph *m_voice_execution_graph;
	c_execution_graph *m_fx_execution_graph;
};

// An instrument contains one or more instrument variants. An instrument may contain more than one instrument variant if
// the instrument globals are context-specific - e.g. an execution graph may be compiled several times for
// different sample rates.
class c_instrument {
public:
	c_instrument();
	~c_instrument();

	e_instrument_result save(const char *fname) const;
	e_instrument_result load(const char *fname);

	bool validate() const;

	// Takes ownership of the instrument variant, which should be allocated using new
	void add_instrument_variant(c_instrument_variant *instrument_variant);

	uint32 get_instrument_variant_count() const;
	const c_instrument_variant *get_instrument_variant(uint32 index) const;

	// Chooses the instrument variant which best matches the given requirements
	e_instrument_variant_for_requirements_result get_instrument_variant_for_requirements(
		const s_instrument_variant_requirements &requirements,
		uint32 &instrument_variant_index_out) const;

private:
	std::vector<c_instrument_variant *> m_instrument_variants;
};

