#ifndef WAVELANG_BUFFER_MANAGER_H__
#define WAVELANG_BUFFER_MANAGER_H__

#include "common/common.h"
#include "common/threading/lock_free.h"

#include "driver/sample_format.h"

#include "engine/executor/buffer_allocator.h"

#include <vector>

class c_task_graph;

class c_buffer_manager {
public:
	void initialize(const c_task_graph *task_graph, uint32 max_buffer_size, uint32 output_channels);

	void begin_chunk(uint32 chunk_size);
	void initialize_buffers_for_voice();
	void accumulate_voice_output(uint32 voice_sample_offset);
	void mix_voice_accumulation_buffers_to_output_buffer(
		e_sample_format sample_format, c_wrapped_array<uint8> output_buffer);

	void allocate_buffer(uint32 buffer_index);
	void decrement_buffer_usage(uint32 buffer_index);
	bool is_buffer_allocated(uint32 buffer_index) const;
	c_buffer *get_buffer(uint32 buffer_index);
	const c_buffer *get_buffer(uint32 buffer_index) const;

private:
	struct ALIGNAS_LOCK_FREE s_buffer_context {
		c_atomic_int32 usages_remaining;
		uint32 pool_index;
		uint32 handle;
		bool shifted_samples;
		bool swapped_into_voice_accumulation_buffer;
	};

	void shift_output_buffers(uint32 voice_sample_offset);
	void swap_output_buffers_with_voice_accumulation_buffers(uint32 voice_sample_offset);
	void add_output_buffers_to_voice_accumulation_buffers(uint32 voice_sample_offset);
	void mix_voice_accumulation_buffers_to_channel_buffers();
	void free_channel_buffers();

#if IS_TRUE(ASSERTS_ENABLED)
	void assert_all_output_buffers_free() const;
#else // IS_TRUE(ASSERTS_ENABLED)
	void assert_all_output_buffers_free() const {}
#endif // IS_TRUE(ASSERTS_ENABLED)

	const c_task_graph *m_task_graph;

	// Allocator for buffers used during processing
	c_buffer_allocator m_buffer_allocator;

	// Pool index of real buffers
	uint32 m_real_buffer_pool_index;

	// Context for each buffer for the currently processing voice
	c_lock_free_aligned_allocator<s_buffer_context> m_buffer_contexts;

	// Buffers to accumulate the final multi-voice result into
	std::vector<uint32> m_voice_accumulation_buffers;

	// Buffers to mix to output channels
	std::vector<uint32> m_channel_mix_buffers;

	// Size of the current chunk
	uint32 m_chunk_size;

	// Number of voices processed so far this chunk
	uint32 m_voices_processed;
};

#endif // WAVELANG_BUFFER_MANAGER_H__