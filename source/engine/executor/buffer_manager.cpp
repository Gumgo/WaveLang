#include "engine/executor/buffer_manager.h"
#include "engine/executor/channel_mixer.h"
#include "engine/buffer_operations/buffer_operations_internal.h"
#include "engine/task_graph.h"

void c_buffer_manager::initialize(const c_task_graph *task_graph, uint32 max_buffer_size, uint32 output_channels) {
	wl_assert(task_graph);
	wl_assert(max_buffer_size > 0);
	wl_assert(output_channels > 0);

	m_task_graph = task_graph;

	s_buffer_allocator_settings buffer_allocator_settings;
	buffer_allocator_settings.buffer_type = k_buffer_type_real;
	buffer_allocator_settings.buffer_size = max_buffer_size;
	buffer_allocator_settings.buffer_count = task_graph->get_max_buffer_concurrency();

	c_task_graph_data_array outputs = task_graph->get_outputs();

	// Allocate an additional buffer for each constant output. The task graph doesn't require buffers for these, but at
	// this point we do.
	for (uint32 output = 0; output < outputs.get_count(); output++) {
		if (outputs[output].data.is_constant) {
			buffer_allocator_settings.buffer_count++;
		}
	}

	// Allocate an additional buffer for each "duplicated" output buffer. This is because we need to keep them separate
	// for accumulation.
	{
		std::vector<uint32> buffer_deduplicator;
		buffer_deduplicator.reserve(outputs.get_count());
		uint32 total_buffer_outputs = 0;
		uint32 unique_buffer_outputs = 0;
		for (uint32 output = 0; output < outputs.get_count(); output++) {

			if (!outputs[output].data.is_constant) {
				uint32 output_buffer_index = outputs[output].get_real_buffer_in();
				if (std::find(buffer_deduplicator.begin(), buffer_deduplicator.end(), output_buffer_index) ==
					buffer_deduplicator.end()) {
					unique_buffer_outputs++;
					buffer_deduplicator.push_back(output_buffer_index);
				}

				total_buffer_outputs++;
			}
		}

		uint32 duplicated_buffer_outputs = total_buffer_outputs - unique_buffer_outputs;
		buffer_allocator_settings.buffer_count += duplicated_buffer_outputs;
	}

	// If we support multiple voices, we need buffers to accumulate the final result into
	if (task_graph->get_globals().max_voices > 1) {
		buffer_allocator_settings.buffer_count += outputs.get_count();
	}

	// If the output count differs from the channel count, we'll need additional buffers to perform mixing
	if (output_channels != outputs.get_count()) {
		buffer_allocator_settings.buffer_count += output_channels;
	}

	m_buffer_allocator.initialize(buffer_allocator_settings);

	m_buffer_contexts.free();
	m_buffer_contexts.allocate(task_graph->get_buffer_count());

	m_voice_accumulation_buffers.resize(outputs.get_count());
	m_channel_mix_buffers.resize(output_channels);

	m_chunk_size = 0;
	m_voices_processed = 0;
}

void c_buffer_manager::begin_chunk(uint32 chunk_size) {
	wl_assert(chunk_size <= m_buffer_allocator.get_settings().buffer_size);
	m_chunk_size = chunk_size;

	m_voices_processed = 0;

	std::fill(m_voice_accumulation_buffers.begin(), m_voice_accumulation_buffers.end(),
		k_lock_free_invalid_handle);
	std::fill(m_channel_mix_buffers.begin(), m_channel_mix_buffers.end(),
		k_lock_free_invalid_handle);
}

void c_buffer_manager::initialize_buffers_for_voice() {
	// Initially all buffers are unassigned
	for (uint32 buffer = 0; buffer < m_task_graph->get_buffer_count(); buffer++) {
		s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer];
		buffer_context.handle = k_lock_free_invalid_handle;
		buffer_context.usages_remaining.initialize(m_task_graph->get_buffer_usages(buffer));
	}
}

void c_buffer_manager::accumulate_voice_output(uint32 voice_sample_offset) {
	if (voice_sample_offset > 0) {
		// If our voice output is offset, shift it forward in the buffer by the appropriate amount so that it starts at
		// the right time.
		shift_output_buffers(voice_sample_offset);
	}

	if (m_voices_processed == 0) {
		// Swap the output buffers directly into the accumulation buffers for the first voice we process
		swap_output_buffers_with_voice_accumulation_buffers(voice_sample_offset);
	} else {
		add_output_buffers_to_voice_accumulation_buffers(voice_sample_offset);
	}

	assert_all_output_buffers_free();
	m_voices_processed++;
}

void c_buffer_manager::mix_voice_accumulation_buffers_to_output_buffer(
	e_sample_format sample_format, c_wrapped_array<uint8> output_buffer) {
	mix_voice_accumulation_buffers_to_channel_buffers();

	c_wrapped_array_const<uint32> channel_buffers(
		&m_channel_mix_buffers.front(), m_channel_mix_buffers.size());
	convert_and_interleave_to_output_buffer(
		m_chunk_size,
		m_buffer_allocator,
		channel_buffers,
		sample_format,
		output_buffer);

	free_channel_buffers();
}

void c_buffer_manager::allocate_buffer(uint32 buffer_index) {
	wl_assert(!is_buffer_allocated(buffer_index));
	m_buffer_contexts.get_array()[buffer_index].handle = m_buffer_allocator.allocate_buffer();
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

void c_buffer_manager::shift_output_buffers(uint32 voice_sample_offset) {
	wl_assert(voice_sample_offset < m_chunk_size);

	c_task_graph_data_array outputs = m_task_graph->get_outputs();

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
			memmove(buffer->get_data<real32>() + voice_sample_offset, buffer->get_data(),
				sizeof(real32) * sample_count);
		}
		memset(buffer->get_data(), 0, sizeof(real32) * voice_sample_offset);
		buffer_context.shifted_samples = true;
	}
}

void c_buffer_manager::swap_output_buffers_with_voice_accumulation_buffers(uint32 voice_sample_offset) {
	c_task_graph_data_array outputs = m_task_graph->get_outputs();

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
		wl_assert(m_voice_accumulation_buffers[output] == k_lock_free_invalid_handle);
		wl_assert(outputs[output].get_type() == k_task_data_type_real_in);
		if (outputs[output].is_constant()) {
			uint32 buffer_handle = m_buffer_allocator.allocate_buffer();
			m_voice_accumulation_buffers[output] = buffer_handle;
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
				m_voice_accumulation_buffers[output] = buffer_context.handle;
				buffer_context.swapped_into_voice_accumulation_buffer = true;
			} else {
				// This case occurs when a single buffer is used for multiple outputs, and the buffer was already
				// swapped to an accumulation slot. In this case, we must actually allocate an additional buffer and
				// copy to it, because we need to keep the data separate both accumulations.
				uint32 buffer_handle = m_buffer_allocator.allocate_buffer();
				m_voice_accumulation_buffers[output] = buffer_handle;
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

void c_buffer_manager::add_output_buffers_to_voice_accumulation_buffers(uint32 voice_sample_offset) {
	c_task_graph_data_array outputs = m_task_graph->get_outputs();

	// Add to the accumulation buffers, which should already exist from the first voice
	for (uint32 output = 0; output < outputs.get_count(); output++) {
		uint32 accumulation_buffer_handle = m_voice_accumulation_buffers[output];
		wl_assert(accumulation_buffer_handle != k_lock_free_invalid_handle);
		wl_assert(outputs[output].get_type() == k_task_data_type_real_in);
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

void c_buffer_manager::mix_voice_accumulation_buffers_to_channel_buffers() {
	if (m_voices_processed == 0) {
		// We have no data, simply allocate zero'd buffers for the channel buffers
		for (uint32 channel = 0; channel < m_channel_mix_buffers.size(); channel++) {
			wl_assert(m_channel_mix_buffers[channel] == k_lock_free_invalid_handle);
			uint32 buffer_handle = m_buffer_allocator.allocate_buffer();
			m_channel_mix_buffers[channel] = buffer_handle;
			s_buffer_operation_assignment::in_out(
				m_chunk_size,
				c_real_buffer_or_constant_in(0.0f),
				m_buffer_allocator.get_buffer(buffer_handle));
		}
	} else if (m_voice_accumulation_buffers.size() == m_channel_mix_buffers.size()) {
		// Simply swap the accumulation buffers with the channel buffers
		std::swap(m_voice_accumulation_buffers, m_channel_mix_buffers);
	} else {
		// Mix the accumulation buffers into the channel buffers and free the accumulation buffers

		// Allocate channel buffers
		for (uint32 channel = 0; channel < m_channel_mix_buffers.size(); channel++) {
			wl_assert(m_channel_mix_buffers[channel] == k_lock_free_invalid_handle);
			m_channel_mix_buffers[channel] = m_buffer_allocator.allocate_buffer();
		}

		c_wrapped_array_const<uint32> accumulation_buffers(
			&m_voice_accumulation_buffers.front(), m_voice_accumulation_buffers.size());
		c_wrapped_array_const<uint32> channel_buffers(&m_channel_mix_buffers.front(), m_channel_mix_buffers.size());

		mix_output_buffers_to_channel_buffers(m_chunk_size, m_buffer_allocator, accumulation_buffers, channel_buffers);

		// Free the accumulation buffers
		for (uint32 accum = 0; accum < m_voice_accumulation_buffers.size(); accum++) {
			m_buffer_allocator.free_buffer(m_voice_accumulation_buffers[accum]);
		}
	}
}

void c_buffer_manager::free_channel_buffers() {
	// Free channel buffers
	for (uint32 channel = 0; channel < m_channel_mix_buffers.size(); channel++) {
		m_buffer_allocator.free_buffer(m_channel_mix_buffers[channel]);
	}
}

#if PREDEFINED(ASSERTS_ENABLED)
void c_buffer_manager::assert_all_output_buffers_free() const {
	// All buffers should have been freed
	for (size_t buffer = 0; buffer < m_buffer_contexts.get_array().get_count(); buffer++) {
		const s_buffer_context &buffer_context = m_buffer_contexts.get_array()[buffer];
		wl_assert(buffer_context.handle == k_lock_free_invalid_handle);
		wl_assert(buffer_context.usages_remaining.get_unsafe() == 0);
	}
}
#endif // PREDEFINED(ASSERTS_ENABLED)