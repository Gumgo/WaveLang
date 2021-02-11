#include "engine/executor/task_memory_manager.h"
#include "engine/task_function_registry.h"
#include "engine/task_graph.h"

#include "task_function/task_function.h"

static void assign_task_function_library_contexts(
	const c_task_graph *task_graph,
	c_wrapped_array<void *> task_function_library_contexts,
	std::vector<void *> &assigned_contexts);

static void reserve_allocation(s_size_alignment &total_size_alignment, const s_size_alignment &allocation);
static c_wrapped_array<uint8> add_allocation(c_wrapped_array<uint8> &buffer, const s_size_alignment &allocation);

void c_task_memory_manager::initialize(
	c_wrapped_array<void *> task_function_library_contexts,
	const c_runtime_instrument *runtime_instrument,
	f_task_memory_query_callback task_memory_query_callback,
	void *task_memory_query_callback_context,
	size_t thread_context_count) {
	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		m_task_library_contexts[enum_index(instrument_stage)].clear();
		m_shared_allocations[enum_index(instrument_stage)].clear();
		m_voice_allocations[enum_index(instrument_stage)].clear();
	}

	m_scratch_allocations.clear();

	const c_task_graph *voice_task_graph = runtime_instrument->get_voice_task_graph();
	const c_task_graph *fx_task_graph = runtime_instrument->get_fx_task_graph();
	const c_task_graph *task_graphs[] = { voice_task_graph, fx_task_graph };
	STATIC_ASSERT(array_count(task_graphs) == enum_count<e_instrument_stage>());

	// Max voices for each stage - fx stage only has 1
	uint32 max_voices[] = { runtime_instrument->get_instrument_globals().max_voices, 1 };
	STATIC_ASSERT(array_count(max_voices) == enum_count<e_instrument_stage>());

	m_voice_graph_task_count = voice_task_graph ? voice_task_graph->get_task_count() : 0;

	// Assign library contexts first since these must be available when querying task memory
	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		if (task_graphs[enum_index(instrument_stage)]) {
			assign_task_function_library_contexts(
				task_graphs[enum_index(instrument_stage)],
				task_function_library_contexts,
				m_task_library_contexts[enum_index(instrument_stage)]);
		}
	}

	std::vector<s_task_memory_query_result> task_memory_query_results[enum_count<e_instrument_stage>()];
	s_size_alignment total_size_alignment{ 0, k_lock_free_alignment };
	s_size_alignment scratch_size_alignment{ 0, 1 };

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		size_t stage_index = enum_index(instrument_stage);

		if (task_graphs[stage_index]) {
			// Query memory for all tasks
			task_memory_query_results[stage_index] = query_required_task_graph_memory(
				instrument_stage,
				task_graphs[stage_index],
				task_memory_query_callback,
				task_memory_query_callback_context);

			// Calculate total memory required
			const std::vector<s_task_memory_query_result> &results =
				task_memory_query_results[stage_index];
			for (const s_task_memory_query_result &task_memory_query_result : results) {
				// Reserve shared memory
				reserve_allocation(total_size_alignment, task_memory_query_result.shared_size_alignment);

				// Reserve per-voice memory
				for (uint32 voice = 0; voice < max_voices[stage_index]; voice++) {
					reserve_allocation(total_size_alignment, task_memory_query_result.voice_size_alignment);
				}

				// Update scratch memory
				scratch_size_alignment.size = std::max(
					scratch_size_alignment.size,
					task_memory_query_result.scratch_size_alignment.size);
				scratch_size_alignment.alignment = std::max(
					scratch_size_alignment.alignment,
					task_memory_query_result.scratch_size_alignment.alignment);
			}
		}
	}

	// Add scratch memory once per thread
	for (size_t thread = 0; thread < thread_context_count; thread++) {
		reserve_allocation(total_size_alignment, scratch_size_alignment);
	}

	// Allocate memory
	wl_assert(total_size_alignment.alignment <= k_lock_free_alignment); // $TODO remove this requirement
	m_task_memory_allocator.free();

	m_scratch_allocations.resize(thread_context_count);

	c_wrapped_array<uint8> buffer;
	if (total_size_alignment.size > 0) {
		m_task_memory_allocator.allocate(total_size_alignment.size);
		buffer = m_task_memory_allocator.get_array();
		zero_type(buffer.get_pointer(), buffer.get_count());
	}

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		size_t stage_index = enum_index(instrument_stage);

		if (task_graphs[stage_index]) {
			uint32 task_count = task_graphs[stage_index]->get_task_count();
			uint32 total_voice_count = task_graphs[stage_index]->get_task_count() * max_voices[stage_index];
			m_shared_allocations[stage_index].resize(task_count);
			m_voice_allocations[stage_index].resize(total_voice_count);

			// Create sub-allocations
			const std::vector<s_task_memory_query_result> &results =
				task_memory_query_results[enum_index(instrument_stage)];
			for (size_t task_index = 0; task_index < results.size(); task_index++) {
				const s_task_memory_query_result &task_memory_query_result = results[task_index];

				// Allocate shared memory
				m_shared_allocations[stage_index][task_index] =
					add_allocation(buffer, task_memory_query_result.shared_size_alignment);

				// Allocate per-voice memory
				for (uint32 voice = 0; voice < max_voices[stage_index]; voice++) {
					m_voice_allocations[stage_index][task_count * voice + task_index] =
						add_allocation(buffer, task_memory_query_result.voice_size_alignment);
				}
			}
		}
	}

	// Allocate scratch memory
	for (size_t thread = 0; thread < thread_context_count; thread++) {
		add_allocation(buffer, scratch_size_alignment);
	}
}

void c_task_memory_manager::deinitialize() {
	m_task_memory_allocator.free();

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		m_shared_allocations[enum_index(instrument_stage)].clear();
		m_voice_allocations[enum_index(instrument_stage)].clear();
		m_scratch_allocations.clear();
	}
}

std::vector<s_task_memory_query_result> c_task_memory_manager::query_required_task_graph_memory(
	e_instrument_stage instrument_stage,
	const c_task_graph *task_graph,
	f_task_memory_query_callback task_memory_query_callback,
	void *task_memory_query_callback_context) {
	std::vector<s_task_memory_query_result> memory_query_results(task_graph->get_task_count());
	for (uint32 task = 0; task < task_graph->get_task_count(); task++) {
		memory_query_results[task] =
			task_memory_query_callback(task_memory_query_callback_context, instrument_stage, task_graph, task);
	}

	return memory_query_results;
}

static void reserve_allocation(s_size_alignment &total_size_alignment, const s_size_alignment &allocation) {
	if (allocation.size == 0) {
		return;
	}

	// Make sure all allocations are aligned on lock-free boundaries to prevent false sharing
	size_t alignment = std::max(allocation.alignment, k_lock_free_alignment);
	total_size_alignment.size = align_size(total_size_alignment.size, alignment);
	total_size_alignment.size += allocation.size;
	total_size_alignment.alignment = std::max(total_size_alignment.alignment, alignment);
}

static c_wrapped_array<uint8> add_allocation(c_wrapped_array<uint8> &buffer, const s_size_alignment &allocation) {
	if (allocation.size == 0) {
		return {};
	}

	// Make sure all allocations are aligned on lock-free boundaries to prevent false sharing
	size_t alignment = std::max(allocation.alignment, k_lock_free_alignment);
	uint8 *aligned_pointer = align_pointer(buffer.get_pointer(), alignment);
	size_t alignment_offset = aligned_pointer - buffer.get_pointer();
	wl_assert(alignment_offset <= buffer.get_count());

	buffer = buffer.get_range(alignment_offset, buffer.get_count() - alignment_offset);
	c_wrapped_array<uint8> result = buffer.get_range(0, allocation.size);
	buffer = buffer.get_range(allocation.size, buffer.get_count() - allocation.size);

	return result;
}

void assign_task_function_library_contexts(
	const c_task_graph *task_graph,
	c_wrapped_array<void *> task_function_library_contexts,
	std::vector<void *> &assigned_contexts) {
	assigned_contexts.resize(task_graph->get_task_count());
	for (uint32 task_index = 0; task_index < task_graph->get_task_count(); task_index++) {
		h_task_function task_function_handle = task_graph->get_task_function_handle(task_index);
		const s_task_function &task_function = c_task_function_registry::get_task_function(task_function_handle);
		h_task_function_library library_handle =
			c_task_function_registry::get_task_function_library_handle(task_function.uid.get_library_id());
		assigned_contexts[task_index] = task_function_library_contexts[library_handle.get_data()];
	}
}
