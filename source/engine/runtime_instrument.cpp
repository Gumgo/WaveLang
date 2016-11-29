#include "engine/runtime_instrument.h"
#include "engine/task_graph.h"

#include "execution_graph/execution_graph.h"
#include "execution_graph/instrument.h"

c_runtime_instrument::c_runtime_instrument() {
	ZERO_STRUCT(&m_task_graphs);
	ZERO_STRUCT(&m_instrument_globals);
}

c_runtime_instrument::~c_runtime_instrument() {
	for (size_t instrument_stage = 0; instrument_stage < k_instrument_stage_count; instrument_stage++) {
		delete m_task_graphs[instrument_stage];
	}
}

bool c_runtime_instrument::build(const c_instrument_variant *instrument_variant) {
	wl_assert(instrument_variant->get_voice_execution_graph() || instrument_variant->get_fx_execution_graph());

	for (size_t instrument_stage = 0; instrument_stage < k_instrument_stage_count; instrument_stage++) {
		delete m_task_graphs[instrument_stage];
		m_task_graphs[instrument_stage] = nullptr;
	}

	if (instrument_variant->get_voice_execution_graph()) {
		m_task_graphs[k_instrument_stage_voice] = new c_task_graph();
		if (!m_task_graphs[k_instrument_stage_voice]->build(*instrument_variant->get_voice_execution_graph())) {
			return false;
		}
	}

	if (instrument_variant->get_fx_execution_graph()) {
		m_task_graphs[k_instrument_stage_fx] = new c_task_graph();
		if (!m_task_graphs[k_instrument_stage_fx]->build(*instrument_variant->get_fx_execution_graph())) {
			return false;
		}
	}

	m_instrument_globals = instrument_variant->get_instrument_globals();
	return true;
}

const c_task_graph *c_runtime_instrument::get_task_graph(e_instrument_stage instrument_stage) const {
	return m_task_graphs[instrument_stage];
}

const c_task_graph *c_runtime_instrument::get_voice_task_graph() const {
	return m_task_graphs[k_instrument_stage_voice];
}

const c_task_graph *c_runtime_instrument::get_fx_task_graph() const {
	return m_task_graphs[k_instrument_stage_fx];
}

const s_instrument_globals &c_runtime_instrument::get_instrument_globals() const {
	return m_instrument_globals;
}
