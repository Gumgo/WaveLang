#pragma once

#include "common/common.h"

#include "engine/task_graph.h"

#include "instrument/instrument_globals.h"
#include "instrument/instrument_stage.h"

class c_instrument_variant;

class c_runtime_instrument {
public:
	c_runtime_instrument() = default;

	bool build(const c_instrument_variant *instrument_variant);

	const c_task_graph *get_task_graph(e_instrument_stage instrument_stage) const;
	const c_task_graph *get_voice_task_graph() const;
	const c_task_graph *get_fx_task_graph() const;
	const s_instrument_globals &get_instrument_globals() const;

	// The number of input channels consumed by this instrument
	uint32 get_input_channel_count() const;

private:
	s_static_array<std::unique_ptr<c_task_graph>, enum_count<e_instrument_stage>()> m_task_graphs;
	s_instrument_globals m_instrument_globals;
	uint32 m_input_channel_count = 0;
};
