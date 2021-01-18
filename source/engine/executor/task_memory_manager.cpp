#include "engine/executor/task_memory_manager.h"
#include "engine/task_function_registry.h"
#include "engine/task_graph.h"

#include "task_function/task_function.h"

// Since we initially store offsets when computing memory requirements, this value cast to a pointer represents null
static constexpr size_t k_invalid_memory_pointer = static_cast<size_t>(-1);

static void assign_task_function_library_contexts(
	const c_task_graph *task_graph,
	c_wrapped_array<void *> task_function_library_contexts,
	std::vector<void *> &assigned_contexts);

void c_task_memory_manager::initialize(
	c_wrapped_array<void *> task_function_library_contexts,
	const c_runtime_instrument *runtime_instrument,
	f_task_memory_query_callback task_memory_query_callback,
	void *task_memory_query_callback_context) {

	
	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		m_task_library_contexts[enum_index(instrument_stage)].clear();
		m_task_memory_pointers[enum_index(instrument_stage)].clear();
	}

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

	// Query task memory
	s_static_array<size_t, enum_count<e_instrument_stage>()> required_task_memory_per_voice;
	size_t total_required_memory = 0;
	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		if (task_graphs[enum_index(instrument_stage)]) {
			size_t total_task_memory_pointers =
				task_graphs[enum_index(instrument_stage)]->get_task_count() * max_voices[enum_index(instrument_stage)];
			m_task_memory_pointers[enum_index(instrument_stage)].reserve(total_task_memory_pointers);

			required_task_memory_per_voice[enum_index(instrument_stage)] = calculate_required_memory_for_task_graph(
				instrument_stage,
				task_graphs[enum_index(instrument_stage)],
				task_memory_query_callback,
				task_memory_query_callback_context,
				m_task_memory_pointers[enum_index(instrument_stage)]);
		} else {
			required_task_memory_per_voice[enum_index(instrument_stage)] = 0;
		}

		wl_assert(is_size_aligned(required_task_memory_per_voice[enum_index(instrument_stage)], k_lock_free_alignment));
		total_required_memory +=
			required_task_memory_per_voice[enum_index(instrument_stage)] * max_voices[enum_index(instrument_stage)];
	}

	// Allocate the memory so we can calculate the true pointers
	m_task_memory_allocator.free();

	if (total_required_memory > 0) {
		m_task_memory_allocator.allocate(total_required_memory);
		void *task_memory_base = m_task_memory_allocator.get_array().get_pointer();
		size_t task_memory_offset = 0;

		for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
			resolve_task_memory_pointers(
				static_cast<uint8 *>(task_memory_base) + task_memory_offset,
				m_task_memory_pointers[enum_index(instrument_stage)],
				max_voices[enum_index(instrument_stage)],
				required_task_memory_per_voice[enum_index(instrument_stage)]);

			task_memory_offset +=
				required_task_memory_per_voice[enum_index(instrument_stage)] * max_voices[enum_index(instrument_stage)];
		}

		// Zero out all the memory - tasks can detect whether it is initialized by providing an "initialized" field
		zero_type(m_task_memory_allocator.get_array().get_pointer(), m_task_memory_allocator.get_array().get_count());
	} else {
#if IS_TRUE(ASSERTS_ENABLED)
		// All pointers should be invalid
		for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
			for (void *task_memory_pointer : m_task_memory_pointers[enum_index(instrument_stage)]) {
				wl_assert(task_memory_pointer == reinterpret_cast<void *>(k_invalid_memory_pointer));
			}

			// All point to null
			size_t count =
				m_task_memory_pointers[enum_index(instrument_stage)].size() * max_voices[enum_index(instrument_stage)];
			m_task_memory_pointers[enum_index(instrument_stage)].clear();
			m_task_memory_pointers[enum_index(instrument_stage)].resize(count, nullptr);
		}
#endif // IS_TRUE(ASSERTS_ENABLED)
	}
}

void c_task_memory_manager::shutdown() {
	m_task_memory_allocator.free();

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		m_task_memory_pointers[enum_index(instrument_stage)].clear();
	}
}

size_t c_task_memory_manager::calculate_required_memory_for_task_graph(
	e_instrument_stage instrument_stage,
	const c_task_graph *task_graph,
	f_task_memory_query_callback task_memory_query_callback,
	void *task_memory_query_callback_context,
	std::vector<void *> &task_memory_pointers) {
	size_t required_memory_per_voice = 0;

	for (uint32 task = 0; task < task_graph->get_task_count(); task++) {
		size_t required_memory_for_task =
			task_memory_query_callback(task_memory_query_callback_context, instrument_stage, task_graph, task);

		if (required_memory_for_task == 0) {
			task_memory_pointers.push_back(reinterpret_cast<void *>(k_invalid_memory_pointer));
		} else {
			task_memory_pointers.push_back(reinterpret_cast<void *>(required_memory_per_voice));

			required_memory_per_voice += required_memory_for_task;
			required_memory_per_voice = align_size(required_memory_per_voice, k_lock_free_alignment);
		}
	}

	return required_memory_per_voice;
}

void c_task_memory_manager::resolve_task_memory_pointers(
	void *task_memory_base,
	std::vector<void *> &task_memory_pointers,
	uint32 max_voices,
	size_t required_memory_per_voice) {
	size_t task_memory_base_uint = reinterpret_cast<size_t>(task_memory_base);

	// Compute offset pointers
	for (size_t index = 0; index < task_memory_pointers.size(); index++) {
		size_t offset = reinterpret_cast<size_t>(task_memory_pointers[index]);
		void *offset_pointer = (offset == k_invalid_memory_pointer)
			? nullptr
			: reinterpret_cast<void *>(task_memory_base_uint + offset);
		task_memory_pointers[index] = offset_pointer;
	}

	// Now offset for each additional voice
	size_t task_count = task_memory_pointers.size();
	task_memory_pointers.resize(task_count * max_voices);
	for (uint32 voice = 1; voice < max_voices; voice++) {
		size_t voice_offset = voice * task_count;
		size_t voice_memory_offset = voice * required_memory_per_voice;
		for (size_t index = 0; index < task_count; index++) {
			size_t base_pointer = reinterpret_cast<size_t>(task_memory_pointers[index]);
			void *offset_pointer = (base_pointer == 0)
				? nullptr
				: reinterpret_cast<void *>(base_pointer + voice_memory_offset);
			task_memory_pointers[voice_offset + index] = offset_pointer;
		}
	}
}

void assign_task_function_library_contexts(
	const c_task_graph *task_graph,
	c_wrapped_array<void *> task_function_library_contexts,
	std::vector<void *> &assigned_contexts) {
	assigned_contexts.resize(task_graph->get_task_count());
	for (uint32 task_index = 0; task_index < task_graph->get_task_count(); task_index++) {
		uint32 task_function_index = task_graph->get_task_function_index(task_index);
		const s_task_function &task_function = c_task_function_registry::get_task_function(task_function_index);
		uint32 library_index =
			c_task_function_registry::get_task_function_library_index(task_function.uid.get_library_id());
		assigned_contexts[task_index] = task_function_library_contexts[library_index];
	}
}
