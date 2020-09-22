#include "engine/task_function.h"
#include "engine/task_function_registry.h"
#include "engine/task_graph.h"
#include "engine/executor/task_memory_manager.h"

// Since we initially store offsets when computing memory requirements, this value cast to a pointer represents null
static constexpr size_t k_invalid_memory_pointer = static_cast<size_t>(-1);

void c_task_memory_manager::initialize(
	const c_runtime_instrument *runtime_instrument,
	f_task_memory_query_callback task_memory_query_callback,
	void *task_memory_query_callback_context) {
	m_task_memory_pointers[enum_index(e_instrument_stage::k_voice)].clear();
	m_task_memory_pointers[enum_index(e_instrument_stage::k_fx)].clear();

	const c_task_graph *voice_task_graph = runtime_instrument->get_voice_task_graph();
	const c_task_graph *fx_task_graph = runtime_instrument->get_fx_task_graph();
	uint32 max_voices = runtime_instrument->get_instrument_globals().max_voices;

	m_voice_graph_task_count = 0;
	size_t total_voice_task_memory_pointers = 0;
	if (voice_task_graph) {
		m_voice_graph_task_count = voice_task_graph->get_task_count();
		total_voice_task_memory_pointers = m_voice_graph_task_count * max_voices;
		m_task_memory_pointers[enum_index(e_instrument_stage::k_voice)].reserve(total_voice_task_memory_pointers);
	}

	size_t total_fx_task_memory_pointers = 0;
	if (fx_task_graph) {
		total_fx_task_memory_pointers = fx_task_graph->get_task_count();
		m_task_memory_pointers[enum_index(e_instrument_stage::k_fx)].reserve(total_fx_task_memory_pointers);
	}

	size_t required_voice_task_memory_per_voice = 0;
	if (voice_task_graph) {
		required_voice_task_memory_per_voice = calculate_required_memory_for_task_graph(
			voice_task_graph,
			task_memory_query_callback,
			task_memory_query_callback_context,
			m_task_memory_pointers[enum_index(e_instrument_stage::k_voice)]);
	}

	size_t required_fx_task_memory = 0;
	if (fx_task_graph) {
		required_fx_task_memory = calculate_required_memory_for_task_graph(
			fx_task_graph,
			task_memory_query_callback,
			task_memory_query_callback_context,
			m_task_memory_pointers[enum_index(e_instrument_stage::k_fx)]);
	}

	wl_assert(is_size_aligned(required_voice_task_memory_per_voice, k_lock_free_alignment));
	wl_assert(is_size_aligned(required_fx_task_memory, k_lock_free_alignment));

	size_t required_voice_task_memory = required_voice_task_memory_per_voice * max_voices;

	// Allocate the memory so we can calculate the true pointers
	size_t total_required_memory = required_voice_task_memory + required_fx_task_memory;
	m_task_memory_allocator.free();

	if (total_required_memory > 0) {
		m_task_memory_allocator.allocate(total_required_memory);

		void *voice_task_memory_base = m_task_memory_allocator.get_array().get_pointer();
		void *fx_task_memory_base = reinterpret_cast<void *>(
			reinterpret_cast<size_t>(voice_task_memory_base) + required_voice_task_memory);

		resolve_task_memory_pointers(
			voice_task_memory_base,
			m_task_memory_pointers[enum_index(e_instrument_stage::k_voice)],
			max_voices,
			required_voice_task_memory_per_voice);
		resolve_task_memory_pointers(
			fx_task_memory_base,
			m_task_memory_pointers[enum_index(e_instrument_stage::k_fx)],
			1,
			required_fx_task_memory);

		// Zero out all the memory - tasks can detect whether it is initialized by providing an "initialized" field
		zero_type(m_task_memory_allocator.get_array().get_pointer(), m_task_memory_allocator.get_array().get_count());
	} else {
#if IS_TRUE(ASSERTS_ENABLED)
		// All pointers should be invalid

		size_t voice_memory_pointers_count = m_task_memory_pointers[enum_index(e_instrument_stage::k_voice)].size();
		for (size_t index = 0; index < voice_memory_pointers_count; index++) {
			wl_assert(m_task_memory_pointers[enum_index(e_instrument_stage::k_voice)][index] ==
				reinterpret_cast<void *>(k_invalid_memory_pointer));
		}

		size_t fx_memory_pointers_count = m_task_memory_pointers[enum_index(e_instrument_stage::k_fx)].size();
		for (size_t index = 0; index < fx_memory_pointers_count; index++) {
			wl_assert(m_task_memory_pointers[enum_index(e_instrument_stage::k_fx)][index] ==
				reinterpret_cast<void *>(k_invalid_memory_pointer));
		}
#endif // IS_TRUE(ASSERTS_ENABLED)

		// All point to null
		m_task_memory_pointers[enum_index(e_instrument_stage::k_voice)].clear();
		m_task_memory_pointers[enum_index(e_instrument_stage::k_voice)].resize(
			total_voice_task_memory_pointers, nullptr);

		m_task_memory_pointers[enum_index(e_instrument_stage::k_fx)].clear();
		m_task_memory_pointers[enum_index(e_instrument_stage::k_fx)].resize(
			total_fx_task_memory_pointers, nullptr);
	}
}

void c_task_memory_manager::shutdown() {
	m_task_memory_allocator.free();

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		m_task_memory_pointers[enum_index(instrument_stage)].clear();
	}
}

size_t c_task_memory_manager::calculate_required_memory_for_task_graph(
	const c_task_graph *task_graph,
	f_task_memory_query_callback task_memory_query_callback,
	void *task_memory_query_callback_context,
	std::vector<void *> &task_memory_pointers) {
	size_t required_memory_per_voice = 0;

	for (uint32 task = 0; task < task_graph->get_task_count(); task++) {
		size_t required_memory_for_task =
			task_memory_query_callback(task_memory_query_callback_context, task_graph, task);

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
