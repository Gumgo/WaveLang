#pragma once

#include "common/common.h"
#include "common/threading/lock_free.h"

#include "engine/runtime_instrument.h"

#include "instrument/instrument_stage.h"

#include "task_function/task_function.h"

#include <vector>

class c_task_memory_manager {
public:
	using f_task_memory_query_callback = s_task_memory_query_result (*)(
		void *context,
		e_instrument_stage instrument_stage,
		const c_task_graph *task_graph,
		uint32 task_index);

	void initialize(
		c_wrapped_array<void *> task_function_library_contexts,
		const c_runtime_instrument *runtime_instrument,
		f_task_memory_query_callback task_memory_query_callback,
		void *task_memory_query_callback_context,
		size_t thread_context_count);
	void deinitialize();

	inline void *get_task_library_context(e_instrument_stage instrument_stage, uint32 task_index) const {
		return m_task_library_contexts[enum_index(instrument_stage)][task_index];
	}

	inline c_wrapped_array<uint8> get_task_shared_memory(
		e_instrument_stage instrument_stage,
		uint32 task_index) const {
		return m_shared_allocations[enum_index(instrument_stage)][task_index];
	}

	inline c_wrapped_array<uint8> get_task_voice_memory(
		e_instrument_stage instrument_stage,
		uint32 task_index,
		uint32 voice_index) const {
		wl_assert(instrument_stage == e_instrument_stage::k_voice || voice_index == 0);
		uint32 allocation_index = m_voice_graph_task_count * voice_index + task_index;
		return m_voice_allocations[enum_index(instrument_stage)][allocation_index];
	}

	inline c_wrapped_array<uint8> get_scratch_memory(size_t thread_context_index) const {
		return m_scratch_allocations[thread_context_index];
	}

private:
	std::vector<s_task_memory_query_result> query_required_task_graph_memory(
		e_instrument_stage instrument_stage,
		const c_task_graph *task_graph,
		f_task_memory_query_callback task_memory_query_callback,
		void *task_memory_query_callback_context);

	// Pointers to each task's library context
	s_static_array<std::vector<void *>, enum_count<e_instrument_stage>()> m_task_library_contexts;

	// Allocator for task-persistent memory
	c_lock_free_aligned_allocator<uint8> m_task_memory_allocator;

	// Shared allocations for each task, indexed by task
	s_static_array<std::vector<c_wrapped_array<uint8>>, enum_count<e_instrument_stage>()> m_shared_allocations;

	// Per-voice allocations for each task, indexed by (task_count * voice + task_index)
	s_static_array<std::vector<c_wrapped_array<uint8>>, enum_count<e_instrument_stage>()> m_voice_allocations;

	// Scratch allocations for each thread context
	std::vector<c_wrapped_array<uint8>> m_scratch_allocations;

	// Task count for voice graph, used for voice offset
	uint32 m_voice_graph_task_count;
};

