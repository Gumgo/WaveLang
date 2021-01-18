#include "engine/runtime_instrument.h"
#include "engine/task_graph.h"

#include "instrument/execution_graph.h"
#include "instrument/instrument.h"

c_runtime_instrument::c_runtime_instrument() {
	zero_type(&m_task_graphs);
	zero_type(&m_instrument_globals);
}

c_runtime_instrument::~c_runtime_instrument() {
	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		delete m_task_graphs[enum_index(instrument_stage)];
	}
}

bool c_runtime_instrument::build(const c_instrument_variant *instrument_variant) {
	wl_assert(instrument_variant->get_voice_execution_graph() || instrument_variant->get_fx_execution_graph());

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		delete m_task_graphs[enum_index(instrument_stage)];
		m_task_graphs[enum_index(instrument_stage)] = nullptr;
	}

	if (instrument_variant->get_voice_execution_graph()) {
		m_task_graphs[enum_index(e_instrument_stage::k_voice)] = new c_task_graph();
		if (!m_task_graphs[enum_index(e_instrument_stage::k_voice)]->build(
			*instrument_variant->get_voice_execution_graph())) {
			return false;
		}
	}

	if (instrument_variant->get_fx_execution_graph()) {
		m_task_graphs[enum_index(e_instrument_stage::k_fx)] = new c_task_graph();
		if (!m_task_graphs[enum_index(e_instrument_stage::k_fx)]->build(
			*instrument_variant->get_fx_execution_graph())) {
			return false;
		}
	}

	m_instrument_globals = instrument_variant->get_instrument_globals();
	return true;
}

const c_task_graph *c_runtime_instrument::get_task_graph(e_instrument_stage instrument_stage) const {
	return m_task_graphs[enum_index(instrument_stage)];
}

const c_task_graph *c_runtime_instrument::get_voice_task_graph() const {
	return m_task_graphs[enum_index(e_instrument_stage::k_voice)];
}

const c_task_graph *c_runtime_instrument::get_fx_task_graph() const {
	return m_task_graphs[enum_index(e_instrument_stage::k_fx)];
}

const s_instrument_globals &c_runtime_instrument::get_instrument_globals() const {
	return m_instrument_globals;
}
