#ifndef WAVELANG_ENGINE_EXECUTOR_TASK_MEMORY_MANAGER_H__
#define WAVELANG_ENGINE_EXECUTOR_TASK_MEMORY_MANAGER_H__

#include "common/common.h"
#include "common/threading/lock_free.h"

#include "engine/runtime_instrument.h"

#include "execution_graph/instrument_stage.h"

#include <vector>

class c_task_memory_manager {
public:
	typedef size_t (*f_task_memory_query_callback)(void *context, const c_task_graph *task_graph, uint32 task_index);

	void initialize(
		const c_runtime_instrument *runtime_instrument,
		f_task_memory_query_callback task_memory_query_callback,
		void *task_memory_query_callback_context);

	inline void *get_task_memory(e_instrument_stage instrument_stage, uint32 task_index, uint32 voice_index) const {
		wl_assert(instrument_stage == k_instrument_stage_voice || voice_index == 0);
		return m_task_memory_pointers[instrument_stage][m_voice_graph_task_count * voice_index + task_index];
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
	s_static_array<std::vector<void *>, k_instrument_stage_count> m_task_memory_pointers;

	// Task count for voice graph, used for voice offset
	uint32 m_voice_graph_task_count;
};

#endif // WAVELANG_ENGINE_EXECUTOR_TASK_MEMORY_MANAGER_H__
