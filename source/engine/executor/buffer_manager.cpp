#include "engine/buffer_operations/buffer_iterator.h"
#include "engine/executor/buffer_manager.h"
#include "engine/executor/channel_mixer.h"

#include <algorithm>

// $TODO $UPSAMPLE this has (mostly) been build to support the eventual addition of upsampled data being passed from the
// voice graph to the FX graph. should probably double check everything though.

static constexpr uint32 k_invalid_buffer_pool_index = -1;

void c_buffer_manager::initialize(
	const c_runtime_instrument *runtime_instrument,
	uint32 max_buffer_size,
	uint32 output_channels) {
	wl_assert(runtime_instrument);
	wl_assert(max_buffer_size > 0);
	wl_assert(output_channels > 0);

	m_runtime_instrument = runtime_instrument;

	// Build data to allocate our buffer pools

	// Represents buffer concurrency during the voice processing phase
	std::vector<c_task_graph::s_buffer_usage_info> voice_buffer_usage_info;

	// Represents buffer concurrency during the FX processing phase
	std::vector<c_task_graph::s_buffer_usage_info> fx_buffer_usage_info;

	// Represents buffer concurrency during the channel mixing phase
	std::vector<c_task_graph::s_buffer_usage_info> channel_mix_buffer_usage_info;

	// All final output channels are real values
	c_task_data_type channel_mix_data_type(e_task_primitive_type::k_real);

	if (runtime_instrument->get_voice_task_graph()) {
		// During the voice processing phase, start with the buffers from the voice task graph
		c_wrapped_array<const c_task_graph::s_buffer_usage_info> voice_graph_buffer_usage_info =
			runtime_instrument->get_voice_task_graph()->get_buffer_usage_info();

		voice_buffer_usage_info.assign(
			voice_graph_buffer_usage_info.get_pointer(),
			voice_graph_buffer_usage_info.get_pointer() + voice_graph_buffer_usage_info.get_count());

		// Voice accumulation buffers and voice shift buffers exist in parallel with voice processing, so we need an
		// additional two buffers for each voice output
		c_buffer_array voice_outputs = runtime_instrument->get_voice_task_graph()->get_outputs();
		m_voice_shift_buffers.reserve(voice_outputs.get_count());
		m_voice_accumulation_buffers.reserve(voice_outputs.get_count());
		for (size_t output_index = 0; output_index < voice_outputs.get_count(); output_index++) {
			c_task_data_type data_type = voice_outputs[output_index]->get_data_type();
			uint32 buffer_pool_index = get_or_add_buffer_pool(data_type, voice_buffer_usage_info);
			voice_buffer_usage_info[buffer_pool_index].max_concurrency += 2;

			m_voice_shift_buffers.push_back(c_buffer::construct(data_type));
			m_voice_accumulation_buffers.push_back(c_buffer::construct(data_type));
		}
	}

	if (runtime_instrument->get_fx_task_graph()) {
		// During the FX processing phase, start with the buffers from the FX task graph
		c_wrapped_array<const c_task_graph::s_buffer_usage_info> fx_graph_buffer_usage_info =
			runtime_instrument->get_fx_task_graph()->get_buffer_usage_info();

		fx_buffer_usage_info.assign(
			fx_graph_buffer_usage_info.get_pointer(),
			fx_graph_buffer_usage_info.get_pointer() + fx_graph_buffer_usage_info.get_count());

		// Reserve a buffer for each FX output
		c_buffer_array fx_outputs = runtime_instrument->get_fx_task_graph()->get_outputs();
		m_fx_output_buffers.reserve(fx_outputs.get_count());
		for (size_t output_index = 0; output_index < fx_outputs.get_count(); output_index++) {
			c_task_data_type data_type = fx_outputs[output_index]->get_data_type();
			uint32 buffer_pool_index = get_or_add_buffer_pool(data_type, fx_buffer_usage_info);
			fx_buffer_usage_info[buffer_pool_index].max_concurrency++;

			m_fx_output_buffers.push_back(c_buffer::construct(data_type));
		}
	}

	{
		// Reserve additional buffers to perform channel mixing
		const c_task_graph *channel_output_task_graph = runtime_instrument->get_fx_task_graph()
			? runtime_instrument->get_fx_task_graph()
			: runtime_instrument->get_voice_task_graph();

		// During the channel mixing phase, buffers from the final processing graph are still allocated
		c_buffer_array channel_outputs = channel_output_task_graph->get_outputs();
		for (size_t output_index = 0; output_index < channel_outputs.get_count(); output_index++) {
			uint32 buffer_pool_index = get_or_add_buffer_pool(
				channel_outputs[output_index]->get_data_type(),
				channel_mix_buffer_usage_info);
			channel_mix_buffer_usage_info[buffer_pool_index].max_concurrency++;
		}

		// Reserve buffers for channel mixing
		uint32 channel_mix_buffer_pool_index = get_or_add_buffer_pool(
			channel_mix_data_type,
			channel_mix_buffer_usage_info);
		channel_mix_buffer_usage_info[channel_mix_buffer_pool_index].max_concurrency += output_channels;

		m_channel_mix_buffers.reserve(output_channels);
		for (uint32 channel = 0; channel < output_channels; channel++) {
			m_channel_mix_buffers.push_back(c_buffer::construct(channel_mix_data_type));
		}
	}

	const std::vector<c_task_graph::s_buffer_usage_info> *buffer_usage_info_array[] = {
		&voice_buffer_usage_info,
		&fx_buffer_usage_info,
		&channel_mix_buffer_usage_info
	};

	// Combine to get worst-case concurrency requirements across all phases
	std::vector<c_task_graph::s_buffer_usage_info> buffer_usage_info = combine_buffer_usage_info(
		c_wrapped_array<const std::vector<c_task_graph::s_buffer_usage_info> *const>::construct(
			buffer_usage_info_array));

	// Assign pool indices now that we've reserved pools

	if (runtime_instrument->get_voice_task_graph()) {
		c_buffer_array voice_outputs = runtime_instrument->get_voice_task_graph()->get_outputs();
		m_voice_output_pool_indices.resize(voice_outputs.get_count());
		for (size_t output_index = 0; output_index < voice_outputs.get_count(); output_index++) {
			c_task_data_type data_type = voice_outputs[output_index]->get_data_type();
			uint32 buffer_pool_index = get_buffer_pool(data_type, buffer_usage_info);
			m_voice_output_pool_indices[output_index] = buffer_pool_index;
		}
	}

	if (runtime_instrument->get_fx_task_graph()) {
		c_buffer_array fx_outputs = runtime_instrument->get_fx_task_graph()->get_outputs();
		m_fx_output_pool_indices.resize(fx_outputs.get_count());
		for (size_t output_index = 0; output_index < fx_outputs.get_count(); output_index++) {
			c_task_data_type data_type = fx_outputs[output_index]->get_data_type();
			uint32 buffer_pool_index = get_buffer_pool(data_type, buffer_usage_info);
			m_fx_output_pool_indices[output_index] = buffer_pool_index;
		}
	}

	{
		uint32 channel_mix_buffer_pool_index = get_buffer_pool(channel_mix_data_type, buffer_usage_info);
		m_real_buffer_pool_index = channel_mix_buffer_pool_index;
	}

	initialize_task_buffer_contexts(buffer_usage_info);
	initialize_buffer_allocator(max_buffer_size, buffer_usage_info);

	m_chunk_size = 0;
	m_voices_processed = 0;
	m_fx_processed = false;
}

void c_buffer_manager::shutdown() {
	m_buffer_allocator.shutdown();

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		m_task_buffer_contexts[enum_index(instrument_stage)].clear();
		m_dynamic_buffers[enum_index(instrument_stage)].clear();
		m_task_buffers_to_decrement[enum_index(instrument_stage)].clear();
		m_task_buffers_to_allocate[enum_index(instrument_stage)].clear();
		m_output_buffers_to_decrement[enum_index(instrument_stage)].clear();
	}

	m_voice_output_pool_indices.clear();
	m_fx_output_pool_indices.clear();
	m_voice_shift_buffers.clear();
	m_voice_accumulation_buffers.clear();
	m_fx_output_buffers.clear();
	m_channel_mix_buffers.clear();
}

void c_buffer_manager::begin_chunk(uint32 chunk_size) {
	wl_assert(chunk_size <= m_buffer_allocator.get_buffer_pool_description(m_real_buffer_pool_index).size);
	m_chunk_size = chunk_size;

	m_voices_processed = 0;
	m_fx_processed = false;
}

void c_buffer_manager::initialize_buffers_for_graph(e_instrument_stage instrument_stage) {
	const c_task_graph *task_graph = m_runtime_instrument->get_task_graph(instrument_stage);
	wl_assert(task_graph);

	for (size_t buffer_index : m_dynamic_buffers[enum_index(instrument_stage)]) {
		s_task_buffer_context &context = m_task_buffer_contexts[enum_index(instrument_stage)][buffer_index];
		wl_assert(context.pool_index >= 0);
		wl_assert(context.buffer);
		context.usages_remaining = context.initial_usages;
	}

	if (instrument_stage == e_instrument_stage::k_voice) {
		// Allocate voice shift and accumulation buffers
		for (size_t index = 0; index < m_voice_accumulation_buffers.size(); index++) {
			c_buffer &voice_shift_buffer = m_voice_shift_buffers[index];
			voice_shift_buffer.set_memory(
				m_buffer_allocator.allocate_buffer_memory(m_voice_output_pool_indices[index]));
			// Shift buffer doesn't need to be initialized

			c_buffer &voice_accumulation_buffer = m_voice_accumulation_buffers[index];
			voice_accumulation_buffer.set_memory(
				m_buffer_allocator.allocate_buffer_memory(m_voice_output_pool_indices[index]));
			// Clear to 0
			voice_accumulation_buffer.get_as<c_real_buffer>().assign_constant(0.0f);
		}
	} else {
		wl_assert(instrument_stage == e_instrument_stage::k_fx);

		// Free voice shift buffers and assign voice accumulation outputs to FX inputs
		c_buffer_array inputs = task_graph->get_inputs();
		wl_assert(inputs.get_count() == m_voice_shift_buffers.size());
		wl_assert(inputs.get_count() == m_voice_accumulation_buffers.size());
		for (size_t index = 0; index < inputs.get_count(); index++) {
			c_buffer *voice_shift_buffer = &m_voice_shift_buffers[index];
			m_buffer_allocator.free_buffer_memory(voice_shift_buffer->get_data_untyped());
			voice_shift_buffer->set_memory(nullptr);

			c_buffer *input_buffer = inputs[index];
			c_buffer *voice_accumulation_buffer = &m_voice_accumulation_buffers[index];

			wl_assert(!input_buffer->is_compile_time_constant());
			input_buffer->set_memory(voice_accumulation_buffer->get_data_untyped());
			input_buffer->set_is_constant(voice_accumulation_buffer->is_constant());
			voice_accumulation_buffer->set_memory(nullptr);

			// If any input is completely unused, it should be deallocated immediately
			size_t buffer_index = task_graph->get_buffer_index(input_buffer);
			s_task_buffer_context &context = m_task_buffer_contexts[enum_index(instrument_stage)][buffer_index];
			if (context.usages_remaining == 0) {
				m_buffer_allocator.free_buffer_memory(input_buffer->get_data_untyped());
				input_buffer->set_memory(nullptr);
			}
		}
	}
}

void c_buffer_manager::accumulate_voice_output(uint32 voice_sample_offset) {
	const c_task_graph *task_graph = m_runtime_instrument->get_voice_task_graph();
	wl_assert(task_graph);

	c_buffer_array outputs = task_graph->get_outputs();
	for (size_t output_index = 0; output_index < outputs.get_count(); output_index++) {
		c_real_buffer *output_buffer = &outputs[output_index]->get_as<c_real_buffer>();
		c_real_buffer *voice_shift_buffer = &m_voice_shift_buffers[output_index].get_as<c_real_buffer>();
		c_real_buffer *voice_accumulation_buffer = &m_voice_accumulation_buffers[output_index].get_as<c_real_buffer>();

		wl_assert(output_buffer->get_data_type() == voice_shift_buffer->get_data_type());
		wl_assert(output_buffer->get_data_type() == voice_accumulation_buffer->get_data_type());

		// First, copy and shift the data into the shift buffer
		zero_type(voice_shift_buffer->get_data(), voice_sample_offset);
		if (output_buffer->is_constant()) {
			real32 constant = output_buffer->get_constant();
			if (voice_sample_offset == 0 || constant == 0.0f) {
				voice_shift_buffer->assign_constant(constant);
			} else {
				voice_shift_buffer->set_is_constant(false);
				for (uint32 index = voice_sample_offset; index < m_chunk_size; index++) {
					voice_shift_buffer->get_data()[index] = constant;
				}
			}
		} else {
			voice_shift_buffer->set_is_constant(false);
			copy_type(
				voice_shift_buffer->get_data() + voice_sample_offset,
				output_buffer->get_data(),
				m_chunk_size - voice_sample_offset);
		}

		// Next, add the shift buffer to the voice accumulation buffer
		iterate_buffers<k_simd_32_lanes, true>(
			m_chunk_size,
			static_cast<const c_real_buffer *>(voice_shift_buffer),
			static_cast<const c_real_buffer *>(voice_accumulation_buffer),
			voice_accumulation_buffer,
			[](
				size_t i,
				const real32xN &voice,
				const real32xN &voice_accumulation_in,
				real32xN &voice_accumulation_out) {
				voice_accumulation_out = voice_accumulation_in + voice;
			});
	}

	// Free the voice shift buffers
	for (c_buffer &buffer : m_voice_shift_buffers) {
		m_buffer_allocator.free_buffer_memory(buffer.get_data_untyped());
		buffer.set_memory(nullptr);
	}

	// Free the output buffers
	decrement_buffer_usages(
		e_instrument_stage::k_voice,
		m_output_buffers_to_decrement[enum_index(e_instrument_stage::k_voice)]);

	assert_all_task_buffers_free(e_instrument_stage::k_voice);
	m_voices_processed++;
}

void c_buffer_manager::store_fx_output() {
	const c_task_graph *task_graph = m_runtime_instrument->get_fx_task_graph();
	wl_assert(task_graph);

	c_buffer_array outputs = task_graph->get_outputs();
	for (uint32 output_index = 0; output_index < outputs.get_count(); output_index++) {
		c_real_buffer *output_buffer = &outputs[output_index]->get_as<c_real_buffer>();
		c_real_buffer *fx_output_buffer = &m_fx_output_buffers[output_index].get_as<c_real_buffer>();

		wl_assert(output_buffer->get_data_type() == fx_output_buffer->get_data_type());

		if (output_buffer->is_constant()) {
			fx_output_buffer->assign_constant(output_buffer->get_constant());
		} else {
			fx_output_buffer->set_is_constant(false);
			copy_type(fx_output_buffer->get_data(), output_buffer->get_data(), m_chunk_size);
		}
	}

	// Free the output buffers
	decrement_buffer_usages(
		e_instrument_stage::k_fx,
		m_output_buffers_to_decrement[enum_index(e_instrument_stage::k_fx)]);

	assert_all_task_buffers_free(e_instrument_stage::k_fx);
	m_fx_processed = true;
}

bool c_buffer_manager::process_remain_active_output(e_instrument_stage instrument_stage, uint32 voice_sample_offset) {
	const c_task_graph *task_graph = m_runtime_instrument->get_task_graph(instrument_stage);
	wl_assert(task_graph);

	bool remain_active;

	const c_bool_buffer *remain_active_buffer = &task_graph->get_remain_active_output()->get_as<c_bool_buffer>();
	if (remain_active_buffer->is_constant()) {
		// The remain-active output is just a constant, so return it directly
		remain_active = remain_active_buffer->get_constant();
	} else {
		// Scan the buffer for any "false" values. Don't bother shifting the buffer, just scan up to the number of
		// samples written.
		remain_active = false;

		uint32 chunk_size = m_chunk_size;
		iterate_buffers<k_simd_size_bits, true>(m_chunk_size, static_cast<const c_bool_buffer *>(remain_active_buffer),
			[&remain_active](size_t i, const int32xN &value) {
				if (any_false(value == int32xN(0))) {
					// At least one bit was true
					remain_active = true;
				}

				// Stop iterating once we've discovered that we shouldn't remain active
				return !remain_active;
			},
			[&remain_active, &chunk_size](size_t i, const int32xN &value) {
				// This only runs on the final iteration
#if IS_TRUE(SIMD_256_ENABLED)
				const int32xN bit_offsets(0, 32, 64, 96, 128, 160, 192, 224);
#elif IS_TRUE(SIMD_128_ENABLED)
				const int32xN bit_offsets(0, 32, 64, 96);
#else // SIMD
#error Single element SIMD type not supported // $TODO $SIMD int32x1 fallback
#endif // SIMD
				int32 samples_remaining = cast_integer_verify<int32>(chunk_size - i);
				int32xN elements_remaining = int32xN(samples_remaining) - bit_offsets;
				int32xN clamped_elements_remaining = min(max(elements_remaining, int32xN(0)), int32xN(32));
				int32xN zeros_to_shift = int32xN(32) - clamped_elements_remaining;
				int32xN overflow_mask = int32xN(0xffffffff) << zeros_to_shift;

				// We now have zeros in the positions of all bits past the end of the buffer
				// Mask the value being tested so we don't get false positives
				int32xN padded_value = value & overflow_mask;
				if (any_false(padded_value == int32xN(0))) {
					// At least one bit was true, so stop scanning
					remain_active = true;
				}
			});
	}

	return remain_active;
}

void c_buffer_manager::mix_voice_accumulation_buffers_to_channel_buffers() {
	mix_to_channel_buffers(m_voice_accumulation_buffers);
}

void c_buffer_manager::mix_fx_output_to_channel_buffers() {
	mix_to_channel_buffers(m_fx_output_buffers);
}

void c_buffer_manager::mix_channel_buffers_to_output_buffer(
	e_sample_format sample_format,
	c_wrapped_array<uint8> output_buffer) {
	convert_and_interleave_to_output_buffer(
		m_chunk_size,
		c_wrapped_array<c_buffer>(m_channel_mix_buffers),
		sample_format,
		output_buffer);

	free_channel_buffers();
	assert_all_buffers_free();
}

void c_buffer_manager::allocate_output_buffers(e_instrument_stage instrument_stage, uint32 task_index) {
	const std::vector<size_t> &buffer_indices = m_task_buffers_to_allocate[enum_index(instrument_stage)][task_index];
	for (size_t buffer_index : buffer_indices) {
		s_task_buffer_context &context = m_task_buffer_contexts[enum_index(instrument_stage)][buffer_index];

		// Make sure the buffer isn't already allocated
		wl_assert(!context.buffer->is_constant() && !context.buffer->get_data_untyped());
		context.buffer->set_memory(m_buffer_allocator.allocate_buffer_memory(context.pool_index));
	}

#if IS_TRUE(ASSERTS_ENABLED)
	// Make sure all buffers are allocated
	const c_task_graph *task_graph = m_runtime_instrument->get_task_graph(instrument_stage);
	wl_assert(task_graph);
	for (c_task_buffer_iterator iter(task_graph->get_task_arguments(task_index)); iter.is_valid(); iter.next()) {
		wl_assert(iter.get_buffer()->get_data_untyped());
	}
#endif // IS_TRUE(ASSERTS_ENABLED)
}

void c_buffer_manager::decrement_buffer_usages(e_instrument_stage instrument_stage, uint32 task_index) {
	const std::vector<size_t> &buffer_indices = m_task_buffers_to_decrement[enum_index(instrument_stage)][task_index];
	decrement_buffer_usages(instrument_stage, buffer_indices);
}

uint32 c_buffer_manager::get_buffer_pool(
	c_task_data_type buffer_type,
	const std::vector<c_task_graph::s_buffer_usage_info> &buffer_pools) const {
	for (uint32 index = 0; index < buffer_pools.size(); index++) {
		if (buffer_pools[index].type == buffer_type) {
			return index;
		}
	}

	return k_invalid_buffer_pool_index;
}

uint32 c_buffer_manager::get_or_add_buffer_pool(
	c_task_data_type buffer_type,
	std::vector<c_task_graph::s_buffer_usage_info> &buffer_pools) const {
	uint32 pool_index = get_buffer_pool(buffer_type, buffer_pools);
	if (pool_index != k_invalid_buffer_pool_index) {
		return pool_index;
	}

	uint32 new_index = cast_integer_verify<uint32>(buffer_pools.size());
	c_task_graph::s_buffer_usage_info buffer_usage_info;
	buffer_usage_info.type = buffer_type;
	buffer_usage_info.max_concurrency = 0;
	buffer_pools.push_back(buffer_usage_info);
	return new_index;
}

std::vector<c_task_graph::s_buffer_usage_info> c_buffer_manager::combine_buffer_usage_info(
	c_wrapped_array<const std::vector<c_task_graph::s_buffer_usage_info> *const> buffer_usage_info_array) const {
	std::vector<c_task_graph::s_buffer_usage_info> result;

	for (size_t array_index = 0; array_index < buffer_usage_info_array.get_count(); array_index++) {
		const std::vector<c_task_graph::s_buffer_usage_info> &buffer_usage_info = *buffer_usage_info_array[array_index];

		for (size_t index = 0; index < buffer_usage_info.size(); index++) {
			uint32 result_pool_index = get_or_add_buffer_pool(buffer_usage_info[index].type, result);
			result[result_pool_index].max_concurrency = std::max(
				result[result_pool_index].max_concurrency,
				buffer_usage_info[index].max_concurrency);
		}
	}

	return result;
}

void c_buffer_manager::decrement_buffer_usages(
	e_instrument_stage instrument_stage,
	const std::vector<size_t> &buffer_indices) {
	for (size_t buffer_index : buffer_indices) {
		s_task_buffer_context &context = m_task_buffer_contexts[enum_index(instrument_stage)][buffer_index];

		// Free the buffer if usage reaches 0
		uint32 prev_usage = context.usages_remaining--;
		wl_assert(prev_usage > 0);
		if (prev_usage == 1) {
			m_buffer_allocator.free_buffer_memory(context.buffer->get_data_untyped());
			context.buffer->set_memory(nullptr);
		}
	}
}

void c_buffer_manager::initialize_buffer_allocator(
	size_t max_buffer_size,
	const std::vector<c_task_graph::s_buffer_usage_info> &buffer_usage_info) {
	std::vector<s_buffer_pool_description> buffer_pool_descriptions;

	for (uint32 index = 0; index < buffer_usage_info.size(); index++) {
		const c_task_graph::s_buffer_usage_info &info = buffer_usage_info[index];
		s_buffer_pool_description desc;

		wl_assert(!info.type.is_array());
		wl_assert(!info.type.get_primitive_type_traits().constant_only);
		switch (info.type.get_primitive_type()) {
		case e_task_primitive_type::k_real:
			desc.type = e_buffer_type::k_real;
			break;

		case e_task_primitive_type::k_bool:
			desc.type = e_buffer_type::k_bool;
			break;

		case e_task_primitive_type::k_string:
			wl_unreachable();
			break;

		default:
			wl_unreachable();
		}

		desc.size = max_buffer_size;
		desc.count = info.max_concurrency;
		buffer_pool_descriptions.push_back(desc);
	}

	s_buffer_allocator_settings buffer_allocator_settings;
	buffer_allocator_settings.buffer_pool_descriptions =
		c_wrapped_array<const s_buffer_pool_description>(buffer_pool_descriptions);

	m_buffer_allocator.initialize(buffer_allocator_settings);
}

void c_buffer_manager::initialize_task_buffer_contexts(
	const std::vector<c_task_graph::s_buffer_usage_info> &buffer_usage_info) {
	const c_task_graph *voice_task_graph = m_runtime_instrument->get_voice_task_graph();
	const c_task_graph *fx_task_graph = m_runtime_instrument->get_fx_task_graph();

	size_t task_buffer_context_count = 0;
	const c_task_graph *task_graphs[] = { voice_task_graph, fx_task_graph };
	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		const c_task_graph *task_graph = task_graphs[enum_index(instrument_stage)];
		if (!task_graph) {
			continue;
		}

		m_task_buffer_contexts[enum_index(instrument_stage)].resize(task_graph->get_buffer_count());
		for (size_t buffer_index = 0; buffer_index < task_graph->get_buffer_count(); buffer_index++) {
			c_buffer *buffer = task_graph->get_buffer_by_index(buffer_index);
			if (buffer->is_compile_time_constant()) {
				// Don't bother initializing contexts for compile-time constant buffers
				continue;
			}

			s_task_buffer_context &context = m_task_buffer_contexts[enum_index(instrument_stage)][buffer_index];

			context.pool_index = get_buffer_pool(buffer->get_data_type(), buffer_usage_info);
			wl_assert(valid_index(context.pool_index, buffer_usage_info.size()));

			context.initial_usages = 0;
			context.usages_remaining = 0;
			context.buffer = buffer;

			m_dynamic_buffers[enum_index(instrument_stage)].push_back(buffer_index);
		}

		m_task_buffers_to_decrement[enum_index(instrument_stage)].resize(task_graph->get_task_count());
		m_task_buffers_to_allocate[enum_index(instrument_stage)].resize(task_graph->get_task_count());

		for (uint32 task_index = 0; task_index < task_graph->get_task_count(); task_index++) {
			c_task_function_runtime_arguments arguments = task_graph->get_task_arguments(task_index);

			for (c_task_buffer_iterator it(arguments); it.is_valid(); it.next()) {
				wl_assert(!it.get_buffer()->is_compile_time_constant());
				size_t buffer_index = task_graph->get_buffer_index(it.get_buffer());

				// Each time a buffer is used in a task, increment its usage count
				m_task_buffer_contexts[enum_index(instrument_stage)][buffer_index].initial_usages++;
				m_task_buffers_to_decrement[enum_index(instrument_stage)][task_index].push_back(buffer_index);

				if (it.get_argument_direction() == e_task_argument_direction::k_out) {
					std::vector<size_t> &buffers_to_allocate =
						m_task_buffers_to_allocate[enum_index(instrument_stage)][task_index];

					// Don't allocate if the output buffer is shared with an input
					bool is_shared = false;
					for (c_task_buffer_iterator it_inner(arguments); it_inner.is_valid(); it_inner.next()) {
						if (it_inner.get_buffer() == it.get_buffer()
							&& it_inner.get_argument_direction() == e_task_argument_direction::k_in) {
							is_shared = true;
							break;
						}
					}

					if (!is_shared) {
						buffers_to_allocate.push_back(buffer_index);
					}
				}
			}
		}

		// Each output buffer contributes to usage to prevent the buffer from deallocating at the end of the last task
		for (c_buffer *output_buffer : task_graph->get_outputs()) {
			if (!output_buffer->is_compile_time_constant()) {
				size_t buffer_index = task_graph->get_buffer_index(output_buffer);
				m_task_buffer_contexts[enum_index(instrument_stage)][buffer_index].initial_usages++;
				m_output_buffers_to_decrement[enum_index(instrument_stage)].push_back(buffer_index);
			}
		}

		// Remain-active buffer contributes as well
		if (!task_graph->get_remain_active_output()->is_compile_time_constant()) {
			size_t buffer_index = task_graph->get_buffer_index(task_graph->get_remain_active_output());
			m_task_buffer_contexts[enum_index(instrument_stage)][buffer_index].initial_usages++;
			m_output_buffers_to_decrement[enum_index(instrument_stage)].push_back(buffer_index);
		}

#if IS_TRUE(ASSERTS_ENABLED)
		for (size_t buffer_index = 0; buffer_index < task_graph->get_buffer_count(); buffer_index++) {
			c_buffer *buffer = task_graph->get_buffer_by_index(buffer_index);
			if (buffer->is_compile_time_constant()) {
				continue;
			}

			const s_task_buffer_context &context = m_task_buffer_contexts[enum_index(instrument_stage)][buffer_index];
			if (context.initial_usages == 0) {
				// If we have a completely unused buffer, it must be an input, since inputs aren't optimized away
				bool is_input = false;
				for (c_buffer *input_buffer : task_graph->get_inputs()) {
					if (buffer == input_buffer) {
						is_input = true;
						break;
					}
				}
				wl_assert(is_input);
			}
		}
#endif // IS_TRUE(ASSERTS_ENABLED)
	}
}

void c_buffer_manager::mix_to_channel_buffers(std::vector<c_buffer> &source_buffers) {
	// Allocate and zero channel buffers
	for (c_buffer &buffer : m_channel_mix_buffers) {
		buffer.set_memory(m_buffer_allocator.allocate_buffer_memory(m_real_buffer_pool_index));
		buffer.get_as<c_real_buffer>().assign_constant(0.0f);
	}

	if (m_voices_processed == 0 && !m_fx_processed) {
		// We have no data, buffers are already zero
	} else {
		// Mix the source buffers into the channel buffers and free the source buffers
		mix_output_buffers_to_channel_buffers(
			m_chunk_size,
			c_wrapped_array<c_buffer>(source_buffers),
			c_wrapped_array<c_buffer>(m_channel_mix_buffers));

		// Free the source buffers
		for (uint32 index = 0; index < source_buffers.size(); index++) {
			m_buffer_allocator.free_buffer_memory(source_buffers[index].get_data_untyped());
			source_buffers[index].set_memory(nullptr);
		}
	}
}

void c_buffer_manager::free_channel_buffers() {
	// Free channel buffers
	for (c_buffer &buffer : m_channel_mix_buffers) {
		m_buffer_allocator.free_buffer_memory(buffer.get_data_untyped());
		buffer.set_memory(nullptr);
	}
}

#if IS_TRUE(ASSERTS_ENABLED)
void c_buffer_manager::assert_all_task_buffers_free(e_instrument_stage instrument_stage) const {
	// All buffers should have been freed
	for (const s_task_buffer_context &context : m_task_buffer_contexts[enum_index(instrument_stage)]) {
		wl_assert(context.usages_remaining == 0);
		// For constant buffers, the buffer itself won't even be allocated
		wl_assert(!context.buffer || context.buffer->get_data_untyped() == nullptr);
	}
}

void c_buffer_manager::assert_all_buffers_free() const {
	m_buffer_allocator.assert_no_allocations();

	for (e_instrument_stage instrument_stage : iterate_enum<e_instrument_stage>()) {
		assert_all_task_buffers_free(instrument_stage);
	}

	for (const c_buffer &buffer : m_voice_shift_buffers) {
		wl_assert(buffer.get_data_untyped() == nullptr);
	}

	for (const c_buffer &buffer : m_voice_accumulation_buffers) {
		wl_assert(buffer.get_data_untyped() == nullptr);
	}

	for (const c_buffer &buffer : m_fx_output_buffers) {
		wl_assert(buffer.get_data_untyped() == nullptr);
	}

	for (const c_buffer &buffer : m_channel_mix_buffers) {
		wl_assert(buffer.get_data_untyped() == nullptr);
	}
}
#endif // IS_TRUE(ASSERTS_ENABLED)
