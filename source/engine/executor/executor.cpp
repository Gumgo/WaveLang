#include "common/utility/memory_debugger.h"

#include "engine/controller_interface/controller_interface.h"
#include "engine/executor/channel_mixer.h"
#include "engine/executor/executor.h"
#include "engine/runtime_instrument.h"
#include "engine/task_function_registry.h"
#include "engine/task_graph.h"

#include "instrument/instrument_globals.h"

#include <algorithm>

c_executor::c_executor() {
	m_state = enum_index(e_state::k_uninitialized);

	m_controller_interface.initialize(&m_controller_event_manager);
}

void c_executor::initialize(
	const s_executor_settings &settings,
	c_wrapped_array<void *> task_function_library_contexts) {
	wl_assert(m_state == enum_index(e_state::k_uninitialized));

	initialize_internal(settings, task_function_library_contexts);

	IF_ASSERTS_ENABLED(int32 old_state = ) m_state.exchange(enum_index(e_state::k_initialized));
	wl_assert(old_state == enum_index(e_state::k_uninitialized));
}

void c_executor::shutdown() {
	// If we are only initialized, we can shutdown immediately
	int32 expected = enum_index(e_state::k_initialized);
	int32 old_state = expected;
	if (!m_state.compare_exchange_strong(expected, enum_index(e_state::k_uninitialized))) {
		old_state = expected;
	}

	if (old_state == enum_index(e_state::k_uninitialized)) {
		// Nothing to do
		return;
	} else if (old_state != enum_index(e_state::k_initialized)) {
		// If we're not initialized, then we must be running. We must wait for the stream to flush before shutting down.
		wl_assert(old_state == enum_index(e_state::k_running));

		IF_ASSERTS_ENABLED(int32 old_state_verify = ) m_state.exchange(enum_index(e_state::k_terminating));
		wl_assert(old_state_verify == enum_index(e_state::k_running));

		// Now wait until we get a notification from the stream
		m_shutdown_signal.wait();
		wl_assert(m_state == enum_index(e_state::k_uninitialized));
	}

	shutdown_internal();
}

void c_executor::execute(const s_executor_chunk_context &chunk_context) {
	SET_MEMORY_ALLOCATIONS_ALLOWED_FOR_SCOPE(false);

	// Try flipping from initialized to running. Nothing will happen if we're not in the initialized state.
	int32 expected = enum_index(e_state::k_initialized);
	m_state.compare_exchange_strong(expected, enum_index(e_state::k_running));

	// Try flipping from terminating to uninitialized
	expected = enum_index(e_state::k_terminating);
	int32 old_state = expected;
	if (!m_state.compare_exchange_strong(expected, enum_index(e_state::k_uninitialized))) {
		old_state = expected;
	}

	if (old_state == enum_index(e_state::k_running)) {
		execute_internal(chunk_context);
	} else {
		// Zero buffer if disabled
		zero_output_buffers(
			chunk_context.frames,
			chunk_context.output_channel_count,
			chunk_context.output_sample_format,
			chunk_context.output_buffer);

		if (old_state == enum_index(e_state::k_terminating)) {
			// We successfully terminated, so signal the main thread
			m_shutdown_signal.notify();
		}
	}
}

void c_executor::initialize_internal(
	const s_executor_settings &settings,
	c_wrapped_array<void *> task_function_library_contexts) {
	m_settings = settings;

	if (task_function_library_contexts.get_count() > 0) {
		m_task_function_library_contexts.resize(task_function_library_contexts.get_count());
		copy_type(
			m_task_function_library_contexts.data(),
			task_function_library_contexts.get_pointer(),
			task_function_library_contexts.get_count());
	}

	wl_assert(settings.runtime_instrument->get_voice_task_graph() || settings.runtime_instrument->get_fx_task_graph());

	initialize_events();
	initialize_thread_pool();
	initialize_buffer_manager();
	pre_initialize_task_function_libraries();
	initialize_task_memory();
	initialize_tasks();
	post_initialize_task_function_libraries();
	initialize_voice_allocator();
	initialize_controller_event_manager();
	initialize_task_contexts();
	initialize_profiler();

	m_event_interface.submit(EVENT_MESSAGE << "Synth started");
	if (m_settings.profiling_enabled) {
		m_event_interface.submit(EVENT_MESSAGE << "Profiling enabled");
	}
}

void c_executor::initialize_events() {
	if (m_settings.event_console_enabled) {
		m_event_console.start();

		if (!m_async_event_handler.is_initialized()) {
			s_async_event_handler_settings event_handler_settings;
			event_handler_settings.set_default();
			event_handler_settings.event_handler = handle_event_wrapper;
			event_handler_settings.event_handler_context = this;
			m_async_event_handler.initialize(event_handler_settings);
		}

		m_async_event_handler.begin_event_handling();
	}

	m_event_interface.initialize(m_settings.event_console_enabled ? &m_async_event_handler : nullptr);
}

void c_executor::initialize_thread_pool() {
	s_thread_pool_settings thread_pool_settings;
	thread_pool_settings.thread_count = m_settings.thread_count;
	thread_pool_settings.max_tasks = 1;
	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		if (m_settings.runtime_instrument->get_task_graph(instrument_stage)) {
			thread_pool_settings.max_tasks = std::max(
				thread_pool_settings.max_tasks,
				m_settings.runtime_instrument->get_task_graph(instrument_stage)->get_max_task_concurrency());
		}
	}
	thread_pool_settings.start_paused = true;

	// We should not be allocating anything inside of tasks
	thread_pool_settings.memory_allocations_allowed = false;

	// Set up thread contexts
	size_t thread_context_count = std::max(m_settings.thread_count, 1u); // We always need at least 1 context
	m_thread_contexts.allocate(thread_context_count);
	for (size_t thread_index = 0; thread_index < thread_context_count; thread_index++) {
		s_task_function_context &task_function_context =
			m_thread_contexts.get_array()[thread_index].task_function_context;
		zero_type(&task_function_context);
		task_function_context.event_interface = &m_event_interface;
		task_function_context.voice_interface = &m_voice_interface;
		task_function_context.controller_interface = &m_controller_interface;
	}

	zero_type(&m_voice_activator_task_function_context);
	m_voice_activator_task_function_context.event_interface = &m_event_interface;

	m_thread_pool.start(thread_pool_settings);
}

void c_executor::initialize_buffer_manager() {
	m_buffer_manager.initialize(
		m_settings.runtime_instrument,
		m_settings.max_buffer_size,
		m_settings.input_channel_count,
		m_settings.output_channel_count);
}

void c_executor::pre_initialize_task_function_libraries() {
	for (h_task_function_library library_handle : c_task_function_registry::iterate_task_function_libraries()) {
		const s_task_function_library &library = c_task_function_registry::get_task_function_library(library_handle);
		if (library.tasks_pre_initializer) {
			library.tasks_pre_initializer(m_task_function_library_contexts[library_handle.get_data()]);
		}
	}
}

void c_executor::initialize_task_memory() {
	m_task_memory_manager.initialize(
		c_wrapped_array<void *>(m_task_function_library_contexts),
		m_settings.runtime_instrument,
		task_memory_query_wrapper,
		this,
		m_thread_contexts.get_array().get_count());
}

void c_executor::initialize_tasks() {
	uint32 max_voices = m_settings.runtime_instrument->get_instrument_globals().max_voices;

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		const c_task_graph *task_graph = m_settings.runtime_instrument->get_task_graph(instrument_stage);
		if (!task_graph) {
			continue;
		}

		uint32 max_voices_for_this_graph = (instrument_stage == e_instrument_stage::k_voice) ? max_voices : 1;

		for (uint32 task = 0; task < task_graph->get_task_count(); task++) {
			const s_task_function &task_function =
				c_task_function_registry::get_task_function(task_graph->get_task_function_handle(task));

			if (task_function.initializer || task_function.voice_initializer) {
				s_task_function_context task_function_context;
				zero_type(&task_function_context);
				task_function_context.event_interface = &m_event_interface;
				task_function_context.upsample_factor = task_graph->get_task_upsample_factor(task);
				task_function_context.sample_rate = m_settings.sample_rate * task_function_context.upsample_factor;
				task_function_context.arguments = task_graph->get_task_arguments(task);

				task_function_context.library_context =
					m_task_memory_manager.get_task_library_context(instrument_stage, task);
				task_function_context.shared_memory =
					m_task_memory_manager.get_task_shared_memory(instrument_stage, task);

				if (task_function.initializer) {
					task_function.initializer(task_function_context);
				}

				if (task_function.voice_initializer) {
					for (uint32 voice = 0; voice < max_voices_for_this_graph; voice++) {
						task_function_context.voice_memory =
							m_task_memory_manager.get_task_voice_memory(instrument_stage, task, voice);
						task_function.voice_initializer(task_function_context);
					}
				}
			}
		}
	}
}

void c_executor::post_initialize_task_function_libraries() {
	for (h_task_function_library library_handle : c_task_function_registry::iterate_task_function_libraries()) {
		const s_task_function_library &library = c_task_function_registry::get_task_function_library(library_handle);
		if (library.tasks_post_initializer) {
			library.tasks_post_initializer(m_task_function_library_contexts[library_handle.get_data()]);
		}
	}
}

void c_executor::initialize_voice_allocator() {
	const s_instrument_globals &globals = m_settings.runtime_instrument->get_instrument_globals();
	m_voice_allocator.initialize(
		globals.max_voices,
		m_settings.runtime_instrument->get_fx_task_graph() != nullptr,
		globals.activate_fx_immediately);
}

void c_executor::initialize_controller_event_manager() {
	m_controller_event_manager.initialize(m_settings.controller_event_queue_size, m_settings.max_controller_parameters);
}

void c_executor::initialize_task_contexts() {
	m_task_contexts.free();
	uint32 max_task_count = 0;

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		const c_task_graph *task_graph = m_settings.runtime_instrument->get_task_graph(instrument_stage);

		if (task_graph) {
			if (task_graph) {
				max_task_count = std::max(max_task_count, task_graph->get_task_count());
			}
		}
	}

	m_task_contexts.allocate(max_task_count);
}

void c_executor::initialize_profiler() {
	if (m_settings.profiling_enabled) {
		s_profiler_settings profiler_settings;
		profiler_settings.worker_thread_count = std::max(1u, m_settings.thread_count);
		profiler_settings.voice_task_count = m_settings.runtime_instrument->get_voice_task_graph()
			? m_settings.runtime_instrument->get_voice_task_graph()->get_task_count()
			: 0;
		profiler_settings.fx_task_count = m_settings.runtime_instrument->get_fx_task_graph()
			? m_settings.runtime_instrument->get_fx_task_graph()->get_task_count()
			: 0;
		m_profiler.initialize(profiler_settings);
		m_profiler.start();
	}
}

void c_executor::shutdown_internal() {
	IF_ASSERTS_ENABLED(uint32 unexecuted_tasks = ) m_thread_pool.stop();
	wl_assert(unexecuted_tasks == 0);

	m_thread_contexts.free();
	m_task_contexts.free();
	m_controller_event_manager.shutdown();
	m_voice_allocator.shutdown();
	deinitialize_tasks();
	m_task_memory_manager.deinitialize();
	m_buffer_manager.shutdown();

	if (m_settings.event_console_enabled) {
		m_async_event_handler.end_event_handling();
		m_event_console.stop();
	}

	if (m_settings.profiling_enabled) {
		m_profiler.stop();
		s_profiler_report report;
		m_profiler.get_report(report);
		output_profiler_report("profiler_report.csv", report);
	}

	m_task_function_library_contexts.clear();
}

void c_executor::deinitialize_tasks() {
	uint32 max_voices = m_settings.runtime_instrument->get_instrument_globals().max_voices;

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		const c_task_graph *task_graph = m_settings.runtime_instrument->get_task_graph(instrument_stage);
		if (!task_graph) {
			continue;
		}

		uint32 max_voices_for_this_graph = (instrument_stage == e_instrument_stage::k_voice) ? max_voices : 1;

		for (uint32 task = 0; task < task_graph->get_task_count(); task++) {
			const s_task_function &task_function =
				c_task_function_registry::get_task_function(task_graph->get_task_function_handle(task));

			if (task_function.initializer || task_function.voice_initializer) {
				s_task_function_context task_function_context;
				zero_type(&task_function_context);
				task_function_context.event_interface = &m_event_interface;
				task_function_context.upsample_factor = task_graph->get_task_upsample_factor(task);
				task_function_context.sample_rate = m_settings.sample_rate * task_function_context.upsample_factor;
				task_function_context.arguments = task_graph->get_task_arguments(task);

				task_function_context.library_context =
					m_task_memory_manager.get_task_library_context(instrument_stage, task);
				task_function_context.shared_memory =
					m_task_memory_manager.get_task_shared_memory(instrument_stage, task);

				if (task_function.voice_deinitializer) {
					for (uint32 voice = 0; voice < max_voices_for_this_graph; voice++) {
						task_function_context.voice_memory =
							m_task_memory_manager.get_task_voice_memory(instrument_stage, task, voice);
						task_function.voice_deinitializer(task_function_context);
					}
				}

				if (task_function.deinitializer) {
					task_function_context.voice_memory = {};
					task_function.deinitializer(task_function_context);
				}
			}
		}
	}
}

s_task_memory_query_result c_executor::task_memory_query_wrapper(
	void *context,
	e_instrument_stage instrument_stage,
	const c_task_graph *task_graph,
	uint32 task_index) {
	return static_cast<c_executor *>(context)->task_memory_query(instrument_stage, task_graph, task_index);
}

s_task_memory_query_result c_executor::task_memory_query(
	e_instrument_stage instrument_stage,
	const c_task_graph *task_graph,
	uint32 task_index) {
	const s_task_function &task_function =
		c_task_function_registry::get_task_function(task_graph->get_task_function_handle(task_index));

	s_task_memory_query_result memory_query_result;
	if (task_function.memory_query) {
		// The memory query function will be able to read from the constant inputs.
		// We don't (currently) support dynamic task memory usage.

		s_task_function_context task_function_context;
		zero_type(&task_function_context);
		task_function_context.event_interface = &m_event_interface;
		task_function_context.upsample_factor = task_graph->get_task_upsample_factor(task_index);
		task_function_context.sample_rate = m_settings.sample_rate * task_function_context.upsample_factor;

		// This function is being called by c_task_memory_manager, but c_task_memory manager initializes all library
		// contexts before querying task memory, so it's okay to call get_task_library_context() at this point in time.
		task_function_context.library_context =
			m_task_memory_manager.get_task_library_context(instrument_stage, task_index);

		task_function_context.arguments = task_graph->get_task_arguments(task_index);

		memory_query_result = task_function.memory_query(task_function_context);
	}

	return memory_query_result;
}

void c_executor::execute_internal(const s_executor_chunk_context &chunk_context) {
	if (m_settings.profiling_enabled) {
		m_profiler.begin_execution();
	}

	wl_assert(chunk_context.frames <= m_settings.max_buffer_size); // $TODO we could do multiple passes to handle this

	// Clear accumulation buffers
	m_buffer_manager.begin_chunk(chunk_context.frames);

	// Deinterleave and mix the input buffers (if they're used)
	m_buffer_manager.mix_input_channel_buffer_to_input_buffers(
		chunk_context.input_sample_format,
		chunk_context.input_buffer);

	size_t controller_event_count = m_settings.process_controller_events(
		m_settings.process_controller_events_context,
		m_controller_event_manager.get_writable_controller_events(),
		chunk_context.buffer_time_sec,
		static_cast<real64>(chunk_context.frames) / static_cast<real64>(chunk_context.sample_rate));
	m_controller_event_manager.process_controller_events(controller_event_count);

	m_voice_allocator.allocate_voices_for_chunk(
		m_controller_event_manager.get_note_events(),
		chunk_context.sample_rate,
		chunk_context.frames);

	m_buffer_manager.allocate_voice_accumulation_buffers();

	if (m_settings.runtime_instrument->get_voice_task_graph()) {
		if (m_settings.profiling_enabled) {
			m_profiler.begin_voices();
		}

		// Process each active voice
		for (uint32 voice_index = 0; voice_index < m_voice_allocator.get_voice_count(); voice_index++) {
			const c_voice_allocator::s_voice &voice = m_voice_allocator.get_voice(voice_index);

			if (!voice.active) {
				continue;
			}

			m_buffer_manager.allocate_and_initialize_voice_input_buffers(voice.chunk_offset_samples);
			m_buffer_manager.allocate_voice_shift_buffers();

			if (m_settings.profiling_enabled) {
				m_profiler.begin_voice();
			}

			process_instrument_stage(e_instrument_stage::k_voice, chunk_context, voice_index);

			if (m_settings.profiling_enabled) {
				m_profiler.end_voice();
			}

			m_buffer_manager.accumulate_voice_output(voice.chunk_offset_samples);
		}

		if (m_settings.profiling_enabled) {
			m_profiler.end_voices();
		}
	}

	if (m_settings.runtime_instrument->get_fx_task_graph()) {
		m_buffer_manager.allocate_fx_output_buffers();
		if (m_voice_allocator.get_fx_voice().active) {
			m_buffer_manager.transfer_input_buffers_and_voice_accumulation_buffers_to_fx_inputs();

			if (m_settings.profiling_enabled) {
				m_profiler.begin_fx();
			}

			process_instrument_stage(e_instrument_stage::k_fx, chunk_context, 0);

			if (m_settings.profiling_enabled) {
				m_profiler.end_fx();
			}

			m_buffer_manager.store_fx_output();
		} else {
			m_buffer_manager.free_voice_accumulation_buffers();
		}

		m_buffer_manager.mix_fx_output_to_channel_buffers();
	} else {
		m_buffer_manager.mix_voice_accumulation_buffers_to_channel_buffers();
	}

	m_buffer_manager.mix_output_channel_buffers_to_output_buffer(
		chunk_context.output_sample_format,
		chunk_context.output_buffer);

	if (m_settings.profiling_enabled) {
		int64 min_total_time_threshold = static_cast<int64>(
			(static_cast<real64>(chunk_context.frames) / static_cast<real64>(chunk_context.sample_rate)) *
			m_settings.profiling_threshold *
			static_cast<real64>(k_nanoseconds_per_second));
		m_profiler.end_execution(min_total_time_threshold);
	}
}

void c_executor::process_instrument_stage(
	e_instrument_stage instrument_stage,
	const s_executor_chunk_context &chunk_context,
	uint32 voice_index) {
	const c_task_graph *task_graph = m_settings.runtime_instrument->get_task_graph(instrument_stage);
	const c_voice_allocator::s_voice &voice = (instrument_stage == e_instrument_stage::k_voice)
		? m_voice_allocator.get_voice(voice_index)
		: m_voice_allocator.get_fx_voice();
	wl_assert(voice.active);

	// Voice index should be 0 if we're performing FX proessing
	wl_assert(instrument_stage == e_instrument_stage::k_voice || voice_index == 0);

	if (voice.activated_this_chunk) {
		for (uint32 task = 0; task < task_graph->get_task_count(); task++) {
			const s_task_function &task_function =
				c_task_function_registry::get_task_function(task_graph->get_task_function_handle(task));

			if (task_function.voice_activator) {
				m_voice_activator_task_function_context.shared_memory =
					m_task_memory_manager.get_task_shared_memory(instrument_stage, task);
				m_voice_activator_task_function_context.voice_memory =
					m_task_memory_manager.get_task_voice_memory(instrument_stage, task, voice_index);
				m_voice_activator_task_function_context.scratch_memory =
					m_task_memory_manager.get_scratch_memory(0);
				m_voice_activator_task_function_context.arguments = task_graph->get_task_arguments(task);

				task_function.voice_activator(m_voice_activator_task_function_context);
			}
		}
	}

	// Setup each initial task predecessor count
	for (uint32 task = 0; task < task_graph->get_task_count(); task++) {
		m_task_contexts.get_array()[task].predecessors_remaining =
			cast_integer_verify<int32>(task_graph->get_task_predecessor_count(task));
	}

	m_buffer_manager.initialize_buffers_for_graph_processing(instrument_stage);

	m_tasks_remaining = cast_integer_verify<int32>(task_graph->get_task_count());

	wl_assert(chunk_context.frames > voice.chunk_offset_samples);
	uint32 frame_count = chunk_context.frames - voice.chunk_offset_samples;

	m_voice_interface = c_voice_interface(
		voice.note_id,
		voice.note_velocity,
		voice.note_release_sample - voice.chunk_offset_samples);

	// Add the initial tasks
	c_task_graph_task_array initial_tasks = task_graph->get_initial_tasks();
	if (initial_tasks.get_count() > 0) {
		for (size_t initial_task = 0; initial_task < initial_tasks.get_count(); initial_task++) {
			add_task(
				instrument_stage,
				voice_index,
				initial_tasks[initial_task],
				chunk_context.sample_rate,
				frame_count);
		}

		// Start the threads processing
		m_thread_pool.resume();
		m_all_tasks_complete_signal.wait();
		m_thread_pool.pause();
	}

	bool remain_active = m_buffer_manager.process_remain_active_output(
		instrument_stage,
		voice.sample_index,
		voice.chunk_offset_samples);

	if (instrument_stage == e_instrument_stage::k_voice) {
		m_voice_allocator.voice_samples_processed(voice_index, frame_count);
	} else {
		m_voice_allocator.fx_samples_processed(frame_count);
	}

	if (!remain_active) {
		if (instrument_stage == e_instrument_stage::k_voice) {
			m_voice_allocator.disable_voice(voice_index);
		} else {
			m_voice_allocator.disable_fx();
		}
	}
}

void c_executor::add_task(
	e_instrument_stage instrument_stage,
	uint32 voice_index,
	uint32 task_index,
	uint32 sample_rate,
	uint32 frames) {
	wl_assert(m_task_contexts.get_array()[task_index].predecessors_remaining == 0);

	s_thread_pool_task task;
	task.task_entry_point = process_task_wrapper;
	s_task_parameters *task_params = task.parameter_block.get_memory_typed<s_task_parameters>();
	task_params->this_ptr = this;
	task_params->instrument_stage = instrument_stage;
	task_params->voice_index = voice_index;
	task_params->task_index = task_index;
	task_params->sample_rate = sample_rate;
	task_params->frames = frames;

	m_thread_pool.add_task(task);
}

void c_executor::process_task_wrapper(uint32 thread_index, const s_thread_parameter_block *params) {
	const s_task_parameters *task_params = params->get_memory_typed<s_task_parameters>();
	task_params->this_ptr->process_task(thread_index, task_params);
}

void c_executor::process_task(uint32 thread_index, const s_task_parameters *params) {
	const c_task_graph *task_graph = m_settings.runtime_instrument->get_task_graph(params->instrument_stage);

	if (m_settings.profiling_enabled) {
		m_profiler.begin_task(
			params->instrument_stage,
			thread_index,
			params->task_index,
			task_graph->get_task_function_handle(params->task_index));
	}

	const s_task_function &task_function =
		c_task_function_registry::get_task_function(task_graph->get_task_function_handle(params->task_index));

	m_buffer_manager.allocate_output_buffers(params->instrument_stage, params->task_index);

	{
		s_task_function_context &task_function_context =
			m_thread_contexts.get_array()[thread_index].task_function_context;
		task_function_context.upsample_factor = task_graph->get_task_upsample_factor(params->task_index);
		task_function_context.sample_rate = m_settings.sample_rate * task_function_context.upsample_factor;
		task_function_context.buffer_size = params->frames * task_function_context.upsample_factor;
		task_function_context.library_context =
			m_task_memory_manager.get_task_library_context(params->instrument_stage, params->task_index);
		task_function_context.shared_memory = m_task_memory_manager.get_task_shared_memory(
			params->instrument_stage,
			params->task_index);
		task_function_context.voice_memory = m_task_memory_manager.get_task_voice_memory(
			params->instrument_stage,
			params->task_index,
			params->voice_index);
		task_function_context.scratch_memory = m_task_memory_manager.get_scratch_memory(thread_index);
		task_function_context.arguments = task_graph->get_task_arguments(params->task_index);

		// Call the task function
		wl_assert(task_function.function);

		if (m_settings.profiling_enabled) {
			m_profiler.begin_task_function(params->instrument_stage, thread_index, params->task_index);
		}

		task_function.function(task_function_context);

		if (m_settings.profiling_enabled) {
			m_profiler.end_task_function(params->instrument_stage, thread_index, params->task_index);
		}
	}

	m_buffer_manager.decrement_buffer_usages(params->instrument_stage, params->task_index);

	// Decrement remaining predecessor counts for all successors to this task
	c_task_graph_task_array successors = task_graph->get_task_successors(params->task_index);
	for (size_t successor = 0; successor < successors.get_count(); successor++) {
		uint32 successor_index = successors[successor];

		int32 prev_predecessors_remaining =
			m_task_contexts.get_array()[successor_index].predecessors_remaining--;
		wl_assert(prev_predecessors_remaining > 0);
		if (prev_predecessors_remaining == 1) {
			add_task(
				params->instrument_stage,
				params->voice_index,
				successor_index,
				params->sample_rate,
				params->frames);
		}
	}

	int32 prev_tasks_remaining = m_tasks_remaining--;
	wl_assert(prev_tasks_remaining > 0);

	if (m_settings.profiling_enabled) {
		m_profiler.end_task(params->instrument_stage, thread_index, params->task_index);
	}

	if (prev_tasks_remaining == 1) {
		// Notify the calling thread that we are done
		m_all_tasks_complete_signal.notify();
	}
}

void c_executor::handle_event_wrapper(void *context, size_t event_size, const void *event_data) {
	static_cast<c_executor *>(context)->handle_event(event_size, event_data);
}

void c_executor::handle_event(size_t event_size, const void *event_data) {
	if (m_event_console.is_running()) {
		c_event_string event_string;
		e_event_level event_level = c_event_interface::build_event_string(event_size, event_data, event_string);

		event_string.truncate_to_length(event_string.get_max_length() - 1);
		event_string.append_verify("\n");

		e_console_color color = e_console_color::k_white;
		switch (event_level) {
		case e_event_level::k_verbose:
			color = e_console_color::k_gray;
			break;

		case e_event_level::k_message:
			color = e_console_color::k_white;
			break;

		case e_event_level::k_warning:
			color = e_console_color::k_yellow;
			break;

		case e_event_level::k_error:
			color = e_console_color::k_red;
			break;

		case e_event_level::k_critical:
			color = e_console_color::k_pink;
			break;

		default:
			wl_unreachable();
		}

		m_event_console.print_to_console(event_string.get_string(), color);
	}
}
