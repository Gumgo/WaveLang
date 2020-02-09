#pragma once

#include "common/common.h"
#include "common/threading/lock_free.h"

#include "engine/runtime_instrument.h"

#include "execution_graph/instrument_stage.h"

#include <vector>

class c_task_memory_manager {
public:
	using f_task_memory_query_callback = size_t (*)(void *context, const c_task_graph *task_graph, uint32 task_index);

	void initialize(
		const c_runtime_instrument *runtime_instrument,
		f_task_memory_query_callback task_memory_query_callback,
		void *task_memory_query_callback_context);

	inline void *get_task_memory(e_instrument_stage instrument_stage, uint32 task_index, uint32 voice_index) const {
		wl_assert(instrument_stage == e_instrument_stage::k_voice || voice_index == 0);
		uint32 task_memory_index = m_voice_graph_task_count * voice_index + task_index;
		return m_task_memory_pointers[enum_index(instrument_stage)][task_memory_index];
	}

private:
	size_t calculate_required_memory_for_task_graph(
		const c_task_graph *task_graph,
		f_task_memory_query_callback task_memory_query_callback,
		void *task_memory_query_callback_context,
		std::vector<void *> &task_memory_pointers);

	void resolve_task_memory_pointers(
		void *task_memory_base,
		std::vector<void *> &task_memory_pointers,
		uint32 max_voices,
		size_t required_memory_per_voice);

	// Allocator for task-persistent memory
	c_lock_free_aligned_allocator<uint8> m_task_memory_allocator;

	// Pointer to each task's persistent memory for each graph (voice and FX)
	// For the voice graph, the layout of this array is: voice_0:[task_0 ... task_n] ... voice_m:[task_0 ... task_n]
	s_static_array<std::vector<void *>, enum_count<e_instrument_stage>()> m_task_memory_pointers;

	// Task count for voice graph, used for voice offset
	uint32 m_voice_graph_task_count;
};

