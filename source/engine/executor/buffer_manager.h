#pragma once

#include "common/common.h"
#include "common/threading/lock_free.h"

#include "driver/sample_format.h"

#include "engine/buffer_handle.h"
#include "engine/runtime_instrument.h"
#include "engine/task_graph.h"
#include "engine/executor/buffer_allocator.h"

#include <vector>

class c_buffer;

class c_buffer_manager {
public:
	void initialize(const c_runtime_instrument *runtime_instrument, uint32 max_buffer_size, uint32 output_channels);

	void begin_chunk(uint32 chunk_size);
	void initialize_buffers_for_graph(e_instrument_stage instrument_stage);
	void accumulate_voice_output(uint32 voice_sample_offset);
	void store_fx_output();
	bool process_remain_active_output(e_instrument_stage instrument_stage, uint32 voice_sample_offset);
	void mix_voice_accumulation_buffers_to_channel_buffers();
	void mix_fx_output_to_channel_buffers();
	void mix_channel_buffers_to_output_buffer(
		e_sample_format sample_format, c_wrapped_array<uint8> output_buffer);

	void allocate_buffer(e_instrument_stage instrument_stage, h_buffer buffer_handle);
	void decrement_buffer_usage(h_buffer buffer_handle);
	bool is_buffer_allocated(h_buffer buffer_handle) const;
	c_buffer *get_buffer(h_buffer buffer_handle);
	const c_buffer *get_buffer(h_buffer buffer_handle) const;

private:
	// nocheckin Need to update more of this
	struct ALIGNAS_LOCK_FREE s_buffer_context {
		c_atomic_int32 usages_remaining;
		s_static_array<uint32, k_instrument_stage_count> pool_indices;
		h_allocated_buffer handle;
		bool shifted_samples;
		bool swapped_into_voice_accumulation_buffer;
	};

	uint32 get_buffer_pool(
		c_task_data_type buffer_type,
		const std::vector<c_task_graph::s_buffer_usage_info> &buffer_pools) const;
	uint32 get_or_add_buffer_pool(
		c_task_data_type buffer_type,
		std::vector<c_task_graph::s_buffer_usage_info> &buffer_pools) const;

	// Produces a buffer usage info list which accounts for the concurrency requirements from several lists
	std::vector<c_task_graph::s_buffer_usage_info> combine_buffer_usage_info(
		c_wrapped_array<const std::vector<c_task_graph::s_buffer_usage_info> *const> buffer_usage_info_array) const;

	void initialize_buffer_allocator(
		size_t max_buffer_size,
		const std::vector<c_task_graph::s_buffer_usage_info> &buffer_usage_info);
	void initialize_buffer_contexts(
		const std::vector<c_task_graph::s_buffer_usage_info> &buffer_usage_info);

	void shift_voice_output_buffers(uint32 voice_sample_offset);
	void swap_and_deduplicate_output_buffers(
		c_task_graph_data_array outputs,
		const std::vector<uint32> &buffer_pool_indices,
		std::vector<h_allocated_buffer> &destination,
		uint32 voice_sample_offset);
	void swap_output_buffers_with_voice_accumulation_buffers(uint32 voice_sample_offset);
	void add_output_buffers_to_voice_accumulation_buffers(uint32 voice_sample_offset);
	void mix_to_channel_buffers(std::vector<h_allocated_buffer> &source_buffers);
	bool scan_remain_active_buffer(const c_buffer *remain_active_buffer, uint32 voice_sample_offset) const;
	void free_channel_buffers();

#if IS_TRUE(ASSERTS_ENABLED)
	void assert_all_output_buffers_free(e_instrument_stage instrument_stage) const;
	void assert_all_buffers_free() const;
#else // IS_TRUE(ASSERTS_ENABLED)
	void assert_all_output_buffers_free(e_instrument_stage instrument_stage) const {}
	void assert_all_buffers_free() const {}
#endif // IS_TRUE(ASSERTS_ENABLED)

	const c_runtime_instrument *m_runtime_instrument;

	// Allocator for buffers used during processing
	c_buffer_allocator m_buffer_allocator;

	// Pool index of real buffers
	uint32 m_real_buffer_pool_index;

	// Pool indices for each voice output
	std::vector<uint32> m_voice_output_pool_indices;

	// Pool indices for each FX output
	std::vector<uint32> m_fx_output_pool_indices;

	// Context for each buffer for the currently processing voice
	c_lock_free_aligned_allocator<s_buffer_context> m_buffer_contexts;

	// Buffers to accumulate the final multi-voice result into
	std::vector<h_allocated_buffer> m_voice_accumulation_buffers;

	// Buffers to store the FX processing result in
	std::vector<h_allocated_buffer> m_fx_output_buffers;

	// Buffers to mix to output channels
	std::vector<h_allocated_buffer> m_channel_mix_buffers;

	// Size of the current chunk
	uint32 m_chunk_size;

	// Number of voices processed so far this chunk
	uint32 m_voices_processed;

	// Whether FX has been processed
	bool m_fx_processed;
};

