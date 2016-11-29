#ifndef WAVELANG_ENGINE_RUNTIME_INSTRUMENT_H__
#define WAVELANG_ENGINE_RUNTIME_INSTRUMENT_H__

#include "common/common.h"

#include "execution_graph/instrument_globals.h"
#include "execution_graph/instrument_stage.h"

class c_instrument_variant;
class c_task_graph;

class c_runtime_instrument {
public:
	c_runtime_instrument();
	~c_runtime_instrument();

	bool build(const c_instrument_variant *instrument_variant);

	const c_task_graph *get_task_graph(e_instrument_stage instrument_stage) const;
	const c_task_graph *get_voice_task_graph() const;
	const c_task_graph *get_fx_task_graph() const;
	const s_instrument_globals &get_instrument_globals() const;

private:
	s_static_array<c_task_graph *, k_instrument_stage_count> m_task_graphs;
	s_instrument_globals m_instrument_globals;
};

#endif // WAVELANG_ENGINE_RUNTIME_INSTRUMENT_H__
