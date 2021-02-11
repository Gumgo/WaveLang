#include "engine/runtime_instrument.h"

#include "instrument/instrument.h"
#include "instrument/native_module_graph.h"

bool c_runtime_instrument::build(const c_instrument_variant *instrument_variant) {
	wl_assert(instrument_variant->get_voice_native_module_graph() || instrument_variant->get_fx_native_module_graph());

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		m_task_graphs[enum_index(instrument_stage)].reset();
	}

	size_t voice_output_count = 0;
	size_t voice_input_count = 0;
	if (instrument_variant->get_voice_native_module_graph()) {
		m_task_graphs[enum_index(e_instrument_stage::k_voice)] = std::make_unique<c_task_graph>();
		if (!m_task_graphs[enum_index(e_instrument_stage::k_voice)]->build(
			*instrument_variant->get_voice_native_module_graph())) {
			return false;
		}

		voice_input_count = m_task_graphs[enum_index(e_instrument_stage::k_voice)]->get_inputs().get_count();
		voice_output_count = m_task_graphs[enum_index(e_instrument_stage::k_voice)]->get_outputs().get_count();
	}

	size_t fx_input_count = 0;
	if (instrument_variant->get_fx_native_module_graph()) {
		m_task_graphs[enum_index(e_instrument_stage::k_fx)] = std::make_unique<c_task_graph>();
		if (!m_task_graphs[enum_index(e_instrument_stage::k_fx)]->build(
			*instrument_variant->get_fx_native_module_graph())) {
			return false;
		}

		fx_input_count = m_task_graphs[enum_index(e_instrument_stage::k_fx)]->get_inputs().get_count();
	}

	m_instrument_globals = instrument_variant->get_instrument_globals();

	if (instrument_variant->get_voice_native_module_graph()) {
		if (instrument_variant->get_fx_native_module_graph()) {
			wl_assert(fx_input_count >= voice_output_count);
			if (voice_input_count == 0) {
				m_input_channel_count = cast_integer_verify<uint32>(fx_input_count - voice_output_count);
			} else {
				wl_assert(
					fx_input_count == voice_output_count || fx_input_count == voice_input_count + voice_output_count);
				m_input_channel_count = cast_integer_verify<uint32>(voice_input_count);
			}
		} else {
			m_input_channel_count = cast_integer_verify<uint32>(voice_input_count);
		}
	} else {
		wl_assert(instrument_variant->get_fx_native_module_graph());
		m_input_channel_count = cast_integer_verify<uint32>(fx_input_count);
	}

	return true;
}

const c_task_graph *c_runtime_instrument::get_task_graph(e_instrument_stage instrument_stage) const {
	return m_task_graphs[enum_index(instrument_stage)].get();
}

const c_task_graph *c_runtime_instrument::get_voice_task_graph() const {
	return m_task_graphs[enum_index(e_instrument_stage::k_voice)].get();
}

const c_task_graph *c_runtime_instrument::get_fx_task_graph() const {
	return m_task_graphs[enum_index(e_instrument_stage::k_fx)].get();
}

const s_instrument_globals &c_runtime_instrument::get_instrument_globals() const {
	return m_instrument_globals;
}

uint32 c_runtime_instrument::get_input_channel_count() const {
	return m_input_channel_count;
}
