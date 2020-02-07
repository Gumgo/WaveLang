#include "engine/buffer_operations/buffer_operations_internal.h"
#include "engine/executor/buffer_manager.h"
#include "engine/executor/channel_mixer.h"

#include <algorithm>

// $TODO $UPSAMPLE this has (mostly) been build to support the eventual addition of upsampled data being passed from the
// voice graph to the FX graph. should probably double check everything though.

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

	if (runtime_instrument->get_voice_task_graph()) {
		// During the voice processing phase, start with the buffers from the voice task graph
		c_wrapped_array<const c_task_graph::s_buffer_usage_info> voice_graph_buffer_usage_info =
			runtime_instrument->get_voice_task_graph()->get_buffer_usage_info();

		voice_buffer_usage_info.assign(
			voice_graph_buffer_usage_info.get_pointer(),
			voice_graph_buffer_usage_info.get_pointer() + voice_graph_buffer_usage_info.get_count());

		// Voice accumulation buffers exist in parallel with voice processing, so we need an additional buffer for each
		// voice output
		// Note: if max_voices is 1, then we don't actually need the accumulation buffers to be stored separately and
		// could instead only reserve additional accumulation buffers for each constant output and each duplicated
		// output buffer (meaning cases where the same buffer is routed to more than one output). However, for
		// simplicity, we will just always reserve 1 buffer per output instead.
		c_task_graph_data_array voice_outputs = runtime_instrument->get_voice_task_graph()->get_outputs();
		m_voice_output_pool_indices.resize(voice_outputs.get_count());
		for (size_t output_index = 0; output_index < voice_outputs.get_count(); output_index++) {
			uint32 buffer_pool_index = get_or_add_buffer_pool(
				voice_outputs[output_index].get_type().get_data_type(), voice_buffer_usage_info);
			m_voice_output_pool_indices[output_index] = buffer_pool_index;
			voice_buffer_usage_info[buffer_pool_index].max_concurrency++;
		}

		m_voice_accumulation_buffers.resize(voice_outputs.get_count());
	}

	if (runtime_instrument->get_fx_task_graph()) {
		// During the FX processing phase, start with the buffers from the FX task graph
		c_wrapped_array<const c_task_graph::s_buffer_usage_info> fx_graph_buffer_usage_info =
			runtime_instrument->get_fx_task_graph()->get_buffer_usage_info();

		fx_buffer_usage_info.assign(
			fx_graph_buffer_usage_info.get_pointer(),
			fx_graph_buffer_usage_info.get_pointer() + fx_graph_buffer_usage_info.get_count());

		// Note: the note above for voice processing also applies to FX processing, and in fact it always applies, even
		// if max_voices is not 1, because FX processing is always only a "single voice". Again, for simplicity, just
		// reserve a buffer for each output.
		c_task_graph_data_array fx_outputs = runtime_instrument->get_fx_task_graph()->get_outputs();
		m_fx_output_pool_indices.resize(fx_outputs.get_count());
		for (size_t output_index = 0; output_index < fx_outputs.get_count(); output_index++) {
			uint32 buffer_pool_index = get_or_add_buffer_pool(
				fx_outputs[output_index].get_type().get_data_type(), fx_buffer_usage_info);
			m_fx_output_pool_indices[output_index] = buffer_pool_index;
			fx_buffer_usage_info[buffer_pool_index].max_concurrency++;
		}

		m_fx_output_buffers.resize(fx_outputs.get_count());
	}

	{
		// Reserve additional buffers to perform channel mixing
		const c_task_graph *channel_output_task_graph = runtime_instrument->get_fx_task_graph() ?
			runtime_instrument->get_fx_task_graph() :
			runtime_instrument->get_voice_task_graph();

		// During the channel mixing phase, buffers from the final processing graph are still allocated
		c_task_graph_data_array channel_outputs = channel_output_task_graph->get_outputs();
		for (size_t output_index = 0; output_index < channel_outputs.get_count(); output_index++) {
			uint32 buffer_pool_index = get_or_add_buffer_pool(
				channel_outputs[output_index].get_type().get_data_type(), channel_mix_buffer_usage_info);
			channel_mix_buffer_usage_info[buffer_pool_index].max_concurrency++;
		}

		// Reserve buffers for channel mixing; all final output channels are real values
		// Note: again, this isn't quite a perfect optimization for memory usage, but it's good enough
		uint32 channel_mix_buffer_pool_index = get_or_add_buffer_pool(
			c_task_data_type(k_task_primitive_type_real), channel_mix_buffer_usage_info);
		m_real_buffer_pool_index = channel_mix_buffer_pool_index;
		channel_mix_buffer_usage_info[channel_mix_buffer_pool_index].max_concurrency += output_channels;

		m_channel_mix_buffers.resize(output_channels);
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

	initialize_buffer_contexts(buffer_usage_info);
	initialize_buffer_allocator(max_buffer_size, buffer_usage_info);

	m_chunk_size = 0;
	m_voices_processed = 0;
	m_fx_processed = false;
}

void c_buffer_manager::begin_chunk(uint32 chunk_size) {
	wl_assert(chunk_size <= m_buffer_allocator.get_buffer_pool_description(m_real_buffer_pool_index).size);
	m_chunk_size = chunk_size;

	m_voices_processed = 0;
	m_fx_processed = false;

	std::fill(
		m_voice_accumulation_buffers.begin(), m_voice_accumulation_buffers.end(), k_lock_free_invalid_handle);
	std::fill(
		m_fx_output_buffers.begin(), m_fx_output_buffers.end(), k_lock_free_invalid_handle);
	std::fill(
		m_channel_mix_buffers.begin(), m_channel_mix_buffers.end(), k_lock_free_invalid_handle);
}

void c_buffer_manager::initialize_buffers_for_graph(e_instrument_stage instrument_stage) {
	const c_task_graph *task_graph = m_runtime_instrument->get_task_graph(instrument_stage);
	wl_assert(task_graph);

	c_task_graph_data_array inputs = task_graph->get_inputs();

	// Initially all buffers are unassigned
	for (uint32 buffer = 0; buffer < task_graph->get_buffer_count(); buffer++) {
		s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer];
		buffer_context.handle = k_lock_free_invalid_handle;
		buffer_context.usages_remaining.initialize(task_graph->get_buffer_usages(buffer));

#if IS_TRUE(ASSERTS_ENABLED)
		if (buffer_context.usages_remaining.get_unsafe() == 0) {
			// If we have a completely unused buffer, it must be an input, since inputs aren't optimized away
			bool is_input = false;
			for (size_t index = 0; !is_input && index < inputs.get_count(); index++) {
				wl_assert(!inputs[index].is_constant());
				is_input = (buffer == inputs[index].data.value.buffer);
			}
			wl_assert(is_input);
		}
#endif // IS_TRUE(ASSERTS_ENABLED)
	}

	// Assign voice accumulation outputs to FX inputs
	// If any input is completely unused, it should be deallocated immediately
	wl_assert(inputs.get_count() == 0 || instrument_stage == k_instrument_stage_fx);
	for (size_t index = 0; index < inputs.get_count(); index++) {
		wl_assert(!inputs[index].is_constant());
		uint32 buffer_index = inputs[index].data.value.buffer;

		s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer_index];

		if (m_voices_processed == 0) {
			wl_assert(m_voice_accumulation_buffers[index] == k_lock_free_invalid_handle);
			// No voices were active, so allocate an empty buffer
			buffer_context.handle = m_buffer_allocator.allocate_buffer(m_voice_output_pool_indices[index]);
			c_buffer *buffer = m_buffer_allocator.get_buffer(buffer_context.handle);
			*buffer->get_data<real32>() = 0.0f;
			buffer->set_constant(true);
		} else {
			wl_assert(m_voice_accumulation_buffers[index] != k_lock_free_invalid_handle);
			std::swap(buffer_context.handle, m_voice_accumulation_buffers[index]);
		}

		if (buffer_context.usages_remaining.get_unsafe() == 0) {
			m_buffer_allocator.free_buffer(buffer_context.handle);
			buffer_context.handle = k_lock_free_invalid_handle;
		}
	}
}

void c_buffer_manager::accumulate_voice_output(uint32 voice_sample_offset) {
	if (voice_sample_offset > 0) {
		// If our voice output is offset, shift it forward in the buffer by the appropriate amount so that it starts at
		// the right time.
		shift_voice_output_buffers(voice_sample_offset);
	}

	if (m_voices_processed == 0) {
		// Swap the output buffers directly into the accumulation buffers for the first voice we process
		swap_output_buffers_with_voice_accumulation_buffers(voice_sample_offset);
	} else {
		add_output_buffers_to_voice_accumulation_buffers(voice_sample_offset);
	}

	assert_all_output_buffers_free(k_instrument_stage_voice);
	m_voices_processed++;
}

void c_buffer_manager::store_fx_output() {
	wl_assert(m_runtime_instrument->get_fx_task_graph());
	swap_and_deduplicate_output_buffers(
		m_runtime_instrument->get_fx_task_graph()->get_outputs(), m_fx_output_pool_indices, m_fx_output_buffers, 0);

	assert_all_output_buffers_free(k_instrument_stage_fx);
	m_fx_processed = true;
}

bool c_buffer_manager::process_remain_active_output(e_instrument_stage instrument_stage, uint32 voice_sample_offset) {
	const c_task_graph *task_graph = m_runtime_instrument->get_task_graph(instrument_stage);
	wl_assert(task_graph);

	bool remain_active;

	if (task_graph->get_remain_active_output().is_constant()) {
		// The remain-active output is just a constant, so return it directly
		remain_active = task_graph->get_remain_active_output().get_bool_constant_in();
	} else {
		wl_assert(task_graph->get_remain_active_output().get_type() ==
			c_task_qualified_data_type(k_task_primitive_type_bool, k_task_qualifier_in));
		uint32 remain_active_buffer_index = task_graph->get_remain_active_output().get_bool_buffer_in();
		uint32 remain_active_buffer_handle = m_buffer_contexts.get_array()[remain_active_buffer_index].handle;
		wl_assert(remain_active_buffer_handle != k_lock_free_invalid_handle);

		// Scan the buffer for any "false" values. Don't bother shifting the buffer, just scan up to the number of
		// samples written.
		const c_buffer *remain_active_buffer = m_buffer_allocator.get_buffer(remain_active_buffer_handle);
		remain_active = scan_remain_active_buffer(remain_active_buffer, voice_sample_offset);

		decrement_buffer_usage(remain_active_buffer_index);
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
	e_sample_format sample_format, c_wrapped_array<uint8> output_buffer) {
	c_wrapped_array<const uint32> channel_buffers(
		&m_channel_mix_buffers.front(), m_channel_mix_buffers.size());
	convert_and_interleave_to_output_buffer(
		m_chunk_size,
		m_buffer_allocator,
		channel_buffers,
		sample_format,
		output_buffer);

	free_channel_buffers();
	assert_all_buffers_free();
}

void c_buffer_manager::allocate_buffer(e_instrument_stage instrument_stage, uint32 buffer_index) {
	wl_assert(!is_buffer_allocated(buffer_index));
	s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer_index];
	buffer_context.handle = m_buffer_allocator.allocate_buffer(buffer_context.pool_indices[instrument_stage]);
}

void c_buffer_manager::decrement_buffer_usage(uint32 buffer_index) {
	// Free the buffer if usage reaches 0
	s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer_index];
	int32 prev_usage = buffer_context.usages_remaining.decrement();
	wl_assert(prev_usage > 0);
	if (prev_usage == 1) {
		m_buffer_allocator.free_buffer(buffer_context.handle);
		buffer_context.handle = k_lock_free_invalid_handle;
	}
}

bool c_buffer_manager::is_buffer_allocated(uint32 buffer_index) const {
	const s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer_index];
	return buffer_context.handle != k_lock_free_invalid_handle;
}

c_buffer *c_buffer_manager::get_buffer(uint32 buffer_index) {
	const s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer_index];
	return m_buffer_allocator.get_buffer(buffer_context.handle);
}

const c_buffer *c_buffer_manager::get_buffer(uint32 buffer_index) const {
	const s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer_index];
	return m_buffer_allocator.get_buffer(buffer_context.handle);
}

uint32 c_buffer_manager::get_buffer_pool(
	c_task_data_type buffer_type,
	const std::vector<c_task_graph::s_buffer_usage_info> &buffer_pools) const {
	for (uint32 index = 0; index < buffer_pools.size(); index++) {
		if (buffer_pools[index].type == buffer_type) {
			return index;
		}
	}

	return static_cast<uint32>(-1);
}

uint32 c_buffer_manager::get_or_add_buffer_pool(
	c_task_data_type buffer_type,
	std::vector<c_task_graph::s_buffer_usage_info> &buffer_pools) const {
	uint32 pool_index = get_buffer_pool(buffer_type, buffer_pools);
	if (pool_index != static_cast<uint32>(-1)) {
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

void c_buffer_manager::initialize_buffer_allocator(
	size_t max_buffer_size,
	const std::vector<c_task_graph::s_buffer_usage_info> &buffer_usage_info) {
	std::vector<s_buffer_pool_description> buffer_pool_descriptions;

	for (uint32 index = 0; index < buffer_usage_info.size(); index++) {
		const c_task_graph::s_buffer_usage_info &info = buffer_usage_info[index];
		s_buffer_pool_description desc;

		wl_assert(!info.type.is_array());
		wl_assert(info.type.get_primitive_type_traits().is_dynamic);
		switch (info.type.get_primitive_type()) {
		case k_task_primitive_type_real:
			desc.type = k_buffer_type_real;
			break;

		case k_task_primitive_type_bool:
			desc.type = k_buffer_type_bool;
			break;

		case k_task_primitive_type_string:
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
	buffer_allocator_settings.buffer_pool_descriptions = c_wrapped_array<const s_buffer_pool_description>(
		&buffer_pool_descriptions.front(), buffer_pool_descriptions.size());

	m_buffer_allocator.initialize(buffer_allocator_settings);
}

void c_buffer_manager::initialize_buffer_contexts(
	const std::vector<c_task_graph::s_buffer_usage_info> &buffer_usage_info) {
	const c_task_graph *voice_task_graph = m_runtime_instrument->get_voice_task_graph();
	const c_task_graph *fx_task_graph = m_runtime_instrument->get_fx_task_graph();
	size_t buffer_context_count = std::max(
		voice_task_graph ? voice_task_graph->get_buffer_count() : 0,
		fx_task_graph ? fx_task_graph->get_buffer_count() : 0);

	m_buffer_contexts.free();
	m_buffer_contexts.allocate(buffer_context_count);

	// Assign pool indices to each buffer context so we know where to allocate from when the time comes
#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t index = 0; index < m_buffer_contexts.get_array().get_count(); index++) {
		s_buffer_context &buffer_context = m_buffer_contexts.get_array()[index];
		buffer_context.usages_remaining.initialize(0);
		for (size_t instrument_stage = 0; instrument_stage < k_instrument_stage_count; instrument_stage++) {
			m_buffer_contexts.get_array()[index].pool_indices[instrument_stage] = static_cast<uint32>(-1);
		}
		buffer_context.handle = k_lock_free_invalid_handle;
		buffer_context.shifted_samples = false;
		buffer_context.swapped_into_voice_accumulation_buffer = false;
	}
#endif // IS_TRUE(ASSERTS_ENABLED)

	// Iterate through each buffer in each task and assign it a pool
	for (size_t instrument_stage = 0; instrument_stage < k_instrument_stage_count; instrument_stage++) {
		const c_task_graph *task_graph = m_runtime_instrument->get_task_graph(
			static_cast<e_instrument_stage>(instrument_stage));
		if (!task_graph) {
			continue;
		}

		c_task_graph_data_array inputs = task_graph->get_inputs();
		for (uint32 input_index = 0; input_index < inputs.get_count(); input_index++) {
			const s_task_graph_data &input_data = inputs[input_index];
			wl_assert(!input_data.is_constant());

			// Find the pool for this buffer (or buffers, in the case of arrays)
			uint32 pool_index = get_buffer_pool(input_data.get_type().get_data_type(), buffer_usage_info);
			wl_assert(VALID_INDEX(pool_index, buffer_usage_info.size()));

			m_buffer_contexts.get_array()[input_data.data.value.buffer].pool_indices[instrument_stage] = pool_index;
		}

		for (uint32 task_index = 0; task_index < task_graph->get_task_count(); task_index++) {
			c_task_graph_data_array arguments = task_graph->get_task_arguments(task_index);

			for (c_task_buffer_iterator it(arguments); it.is_valid(); it.next()) {
				c_task_data_type type_to_match = it.get_buffer_type().get_data_type();

				// Find the pool for this buffer (or buffers, in the case of arrays)
				uint32 pool_index = get_buffer_pool(type_to_match, buffer_usage_info);
				wl_assert(VALID_INDEX(pool_index, buffer_usage_info.size()));

				m_buffer_contexts.get_array()[it.get_buffer_index()].pool_indices[instrument_stage] = pool_index;
			}
		}
	}

#if IS_TRUE(ASSERTS_ENABLED)
	for (size_t instrument_stage = 0; instrument_stage < k_instrument_stage_count; instrument_stage++) {
		const c_task_graph *task_graph = m_runtime_instrument->get_task_graph(
			static_cast<e_instrument_stage>(instrument_stage));
		if (!task_graph) {
			continue;
		}

		// Make sure all buffers got pool assignments
		for (uint32 index = 0; index < task_graph->get_buffer_count(); index++) {
			wl_assert(VALID_INDEX(
				m_buffer_contexts.get_array()[index].pool_indices[instrument_stage],
				buffer_usage_info.size()));
		}
	}
#endif // IS_TRUE(ASSERTS_ENABLED)
}

void c_buffer_manager::shift_voice_output_buffers(uint32 voice_sample_offset) {
	wl_assert(voice_sample_offset < m_chunk_size);

	c_task_graph_data_array outputs = m_runtime_instrument->get_voice_task_graph()->get_outputs();

	for (uint32 output = 0; output < outputs.get_count(); output++) {
		if (outputs[output].is_constant()) {
			continue;
		}

		// Clear the shifted flag. We need this flag because buffers may be shared between multiple outputs, and we only
		// want to shift each one once.
		s_buffer_context &buffer_context =
			m_buffer_contexts.get_array()[outputs[output].get_real_buffer_in()];
		buffer_context.shifted_samples = false;
	}

	for (uint32 output = 0; output < outputs.get_count(); output++) {
		if (outputs[output].is_constant()) {
			continue;
		}

		s_buffer_context &buffer_context =
			m_buffer_contexts.get_array()[outputs[output].get_real_buffer_in()];

		if (buffer_context.shifted_samples) {
			// We already shifted this buffer
			continue;
		}

		// Shift the samples forward and fill the beginning with 0
		c_buffer *buffer = m_buffer_allocator.get_buffer(buffer_context.handle);
		if (buffer->is_constant()) {
			// Grab the first value and copy it starting at the offset sample
			real32 *buffer_data = buffer->get_data<real32>();
			real32 constant_value = *buffer_data;
			for (size_t index = voice_sample_offset; index < m_chunk_size; index++) {
				buffer_data[index] = constant_value;
			}
		} else {
			uint32 sample_count = m_chunk_size - voice_sample_offset;
			memmove(
				buffer->get_data<real32>() + voice_sample_offset, buffer->get_data(), sizeof(real32) * sample_count);
		}
		memset(buffer->get_data(), 0, sizeof(real32) * voice_sample_offset);
		buffer_context.shifted_samples = true;
	}
}

void c_buffer_manager::swap_and_deduplicate_output_buffers(
	c_task_graph_data_array outputs,
	const std::vector<uint32> &buffer_pool_indices,
	std::vector<uint32> &destination,
	uint32 voice_sample_offset) {
	wl_assert(outputs.get_count() == destination.size());

	for (uint32 output = 0; output < outputs.get_count(); output++) {
		if (outputs[output].is_constant()) {
			continue;
		}

		// Clear the swapped flag. We need this flag because buffers may be shared between multiple outputs, and we only
		// want to swap each one once.
		s_buffer_context &buffer_context =
			m_buffer_contexts.get_array()[outputs[output].get_real_buffer_in()];
		buffer_context.swapped_into_voice_accumulation_buffer = false;
	}

	for (uint32 output = 0; output < outputs.get_count(); output++) {
		wl_assert(destination[output] == k_lock_free_invalid_handle);
		wl_assert(outputs[output].get_type() ==
			c_task_qualified_data_type(k_task_primitive_type_real, k_task_qualifier_in));
		if (outputs[output].is_constant()) {
			uint32 buffer_handle = m_buffer_allocator.allocate_buffer(buffer_pool_indices[output]);
			destination[output] = buffer_handle;
			if (voice_sample_offset == 0) {
				s_buffer_operation_assignment::in_out(
					m_chunk_size,
					c_real_buffer_or_constant_in(outputs[output].get_real_constant_in()),
					m_buffer_allocator.get_buffer(buffer_handle));
			} else {
				// We need to fill the beginning of the buffer with 0s up until the offset
				c_buffer *buffer = m_buffer_allocator.get_buffer(buffer_handle);
				real32 constant = outputs[output].get_real_constant_in();
				if (constant == 0.0f) {
					*buffer->get_data<real32>() = 0.0f;
					buffer->set_constant(true);
				} else {
					real32 *buffer_data = buffer->get_data<real32>();
					memset(buffer_data, 0, sizeof(real32) * m_chunk_size);
					buffer->set_constant(false);
				}
			}
		} else {
			// Swap out the buffer
			s_buffer_context &buffer_context =
				m_buffer_contexts.get_array()[outputs[output].get_real_buffer_in()];

			if (!buffer_context.swapped_into_voice_accumulation_buffer) {
				destination[output] = buffer_context.handle;
				buffer_context.swapped_into_voice_accumulation_buffer = true;
			} else {
				// This case occurs when a single buffer is used for multiple outputs, and the buffer was already
				// swapped to an accumulation slot. In this case, we must actually allocate an additional buffer and
				// copy to it, because we need to keep the data separate both accumulations.
				uint32 buffer_handle = m_buffer_allocator.allocate_buffer(buffer_pool_indices[output]);
				destination[output] = buffer_handle;
				s_buffer_operation_assignment::in_out(
					m_chunk_size,
					c_real_buffer_or_constant_in(m_buffer_allocator.get_buffer(buffer_context.handle)),
					m_buffer_allocator.get_buffer(buffer_handle));
			}

			// Decrement usage count, but don't deallocate, because we're swapping it into the accumulation buffer slot.
			int32 prev_usage = buffer_context.usages_remaining.decrement();
			wl_assert(prev_usage > 0);
			if (prev_usage == 1) {
				buffer_context.handle = k_lock_free_invalid_handle;
			}
		}
	}
}

void c_buffer_manager::swap_output_buffers_with_voice_accumulation_buffers(uint32 voice_sample_offset) {
	swap_and_deduplicate_output_buffers(
		m_runtime_instrument->get_voice_task_graph()->get_outputs(),
		m_voice_output_pool_indices,
		m_voice_accumulation_buffers,
		voice_sample_offset);
}

void c_buffer_manager::add_output_buffers_to_voice_accumulation_buffers(uint32 voice_sample_offset) {
	const c_task_graph *task_graph = m_runtime_instrument->get_voice_task_graph();
	wl_assert(task_graph);

	c_task_graph_data_array outputs = task_graph->get_outputs();

	// Add to the accumulation buffers, which should already exist from the first voice
	for (uint32 output = 0; output < outputs.get_count(); output++) {
		uint32 accumulation_buffer_handle = m_voice_accumulation_buffers[output];
		wl_assert(accumulation_buffer_handle != k_lock_free_invalid_handle);
		wl_assert(outputs[output].get_type() ==
			c_task_qualified_data_type(k_task_primitive_type_real, k_task_qualifier_in));
		if (outputs[output].is_constant()) {
			if (voice_sample_offset == 0) {
				s_buffer_operation_addition::inout_in(
					m_chunk_size,
					m_buffer_allocator.get_buffer(accumulation_buffer_handle),
					c_real_buffer_or_constant_in(outputs[output].get_real_constant_in()));
			} else {
				// Manually add only starting at the offset
				c_buffer *buffer = m_buffer_allocator.get_buffer(accumulation_buffer_handle);
				real32 *buffer_data = buffer->get_data<real32>();
				real32 constant_value = outputs[output].get_real_constant_in();
				for (size_t index = voice_sample_offset; index < m_chunk_size; index++) {
					buffer_data[index] += constant_value;
				}
			}
		} else {
			s_buffer_context &buffer_context =
				m_buffer_contexts.get_array()[outputs[output].get_real_buffer_in()];
			s_buffer_operation_addition::inout_in(
				m_chunk_size,
				m_buffer_allocator.get_buffer(accumulation_buffer_handle),
				c_real_buffer_or_constant_in(m_buffer_allocator.get_buffer(buffer_context.handle)));
		}
	}

	// Free the output buffers
	for (size_t index = 0; index < outputs.get_count(); index++) {
		if (!outputs[index].is_constant()) {
			uint32 buffer_index = outputs[index].get_real_buffer_in();
			decrement_buffer_usage(buffer_index);
		}
	}
}

void c_buffer_manager::mix_to_channel_buffers(std::vector<uint32> &source_buffers) {
	if (m_voices_processed == 0 && !m_fx_processed) {
		// We have no data, simply allocate zero'd buffers for the channel buffers
		for (uint32 channel = 0; channel < m_channel_mix_buffers.size(); channel++) {
			wl_assert(m_channel_mix_buffers[channel] == k_lock_free_invalid_handle);
			uint32 buffer_handle = m_buffer_allocator.allocate_buffer(m_real_buffer_pool_index);
			m_channel_mix_buffers[channel] = buffer_handle;
			s_buffer_operation_assignment::in_out(
				m_chunk_size,
				c_real_buffer_or_constant_in(0.0f),
				m_buffer_allocator.get_buffer(buffer_handle));
		}
	} else if (source_buffers.size() == m_channel_mix_buffers.size()) {
		// Simply swap the source buffers with the channel buffers
		std::swap(source_buffers, m_channel_mix_buffers);
	} else {
		// Mix the source buffers into the channel buffers and free the source buffers

		// Allocate channel buffers
		for (uint32 channel = 0; channel < m_channel_mix_buffers.size(); channel++) {
			wl_assert(m_channel_mix_buffers[channel] == k_lock_free_invalid_handle);
			m_channel_mix_buffers[channel] = m_buffer_allocator.allocate_buffer(m_real_buffer_pool_index);
		}

		c_wrapped_array<const uint32> source_buffer_list(&source_buffers.front(), source_buffers.size());
		c_wrapped_array<const uint32> channel_buffers(&m_channel_mix_buffers.front(), m_channel_mix_buffers.size());

		mix_output_buffers_to_channel_buffers(m_chunk_size, m_buffer_allocator, source_buffer_list, channel_buffers);

		// Free the accumulation buffers
		for (uint32 accum = 0; accum < source_buffers.size(); accum++) {
			m_buffer_allocator.free_buffer(source_buffers[accum]);
		}
	}
}

bool c_buffer_manager::scan_remain_active_buffer(
	const c_buffer *remain_active_buffer, uint32 voice_sample_offset) const {
	bool remain_active;

	if (remain_active_buffer->is_constant()) {
		// Grab the first bit
		remain_active = ((*remain_active_buffer->get_data<int32>() & 1) != 0);
	} else {
		remain_active = false;

		// Scan the buffer for any true values
		uint32 samples_to_process = m_chunk_size - voice_sample_offset;
		int32 samples_remaining = cast_integer_verify<int32>(samples_to_process);
		c_buffer_iterator_1<c_const_bool_buffer_iterator_128> iter(remain_active_buffer, samples_to_process);
		for (; iter.is_valid(); iter.next()) {
			wl_assert(samples_remaining > 0);

			c_int32_4 elements_remaining = c_int32_4(samples_remaining) - c_int32_4(0, 32, 64, 96);
			c_int32_4 clamped_elements_remaining = min(max(elements_remaining, c_int32_4(0)), c_int32_4(32));
			c_int32_4 zeros_to_shift = c_int32_4(32) - clamped_elements_remaining;
			c_int32_4 overflow_mask = c_int32_4(0xffffffff) << zeros_to_shift;

			// We now have zeros in the positions of all bits past the end of the buffer
			// Mask the value being tested so we don't get false positives
			c_int32_4 padded_value = iter.get_iterator_a().get_value() & overflow_mask;
			c_int32_4 all_zero_test = (padded_value == c_int32_4(0));
			int32 all_zero_msb = mask_from_msb(all_zero_test);

			if (all_zero_msb != 15) {
				// At least one bit was true, so stop scanning
				remain_active = true;
				break;
			}

			samples_remaining -= 128;
		}
	}

	return remain_active;
}

void c_buffer_manager::free_channel_buffers() {
	// Free channel buffers
	for (uint32 channel = 0; channel < m_channel_mix_buffers.size(); channel++) {
		m_buffer_allocator.free_buffer(m_channel_mix_buffers[channel]);
	}
}

#if IS_TRUE(ASSERTS_ENABLED)
void c_buffer_manager::assert_all_output_buffers_free(e_instrument_stage instrument_stage) const {
	// All buffers should have been freed
	uint32 buffer_count = m_runtime_instrument->get_task_graph(instrument_stage)->get_buffer_count();
	for (uint32 buffer = 0; buffer < buffer_count; buffer++) {
		const s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer];
		wl_assert(buffer_context.handle == k_lock_free_invalid_handle);
		wl_assert(buffer_context.usages_remaining.get_unsafe() == 0);
	}
}

void c_buffer_manager::assert_all_buffers_free() const {
	m_buffer_allocator.assert_all_buffers_free();
}
#endif // IS_TRUE(ASSERTS_ENABLED)
