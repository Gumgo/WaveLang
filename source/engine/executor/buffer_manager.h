#pragma once

#include "common/common.h"
#include "common/threading/lock_free.h"

#include "driver/sample_format.h"

#include "engine/runtime_instrument.h"
#include "engine/task_graph.h"
#include "engine/executor/buffer_allocator.h"

#include <atomic>
#include <vector>

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
		e_sample_format sample_format,
		c_wrapped_array<uint8> output_buffer);

	void allocate_output_buffers(e_instrument_stage instrument_stage, uint32 task_index);
	void decrement_buffer_usages(e_instrument_stage instrument_stage, uint32 task_index);

private:
	// Context for a buffer used by a task. These are indexed by total buffer count for the task graph, which includes
	// both constant and dynamic buffers, but only dynamic buffers are used, so the c_buffer instance on the constant
	// entries will be unallocated.
	struct ALIGNAS_LOCK_FREE s_task_buffer_context {
		s_task_buffer_context() = default;
		UNCOPYABLE(s_task_buffer_context);

		s_task_buffer_context(s_task_buffer_context &&other)
			: pool_index(other.pool_index)
			, initial_usages(other.initial_usages)
			, usages_remaining(other.usages_remaining.load())
			, buffer(other.buffer) {}

		s_task_buffer_context &operator=(s_task_buffer_context &&other) {
			pool_index = other.pool_index;
			initial_usages = other.initial_usages;
			usages_remaining = other.usages_remaining.load();
			buffer = other.buffer;
		}

		uint32 pool_index;
		int32 initial_usages;
		std::atomic<uint32> usages_remaining;
		c_buffer *buffer;
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

	void decrement_buffer_usages(e_instrument_stage instrument_stage, const std::vector<size_t> &buffer_indices);

	void initialize_buffer_allocator(
		size_t max_buffer_size,
		const std::vector<c_task_graph::s_buffer_usage_info> &buffer_usage_info);
	void initialize_task_buffer_contexts(
		const std::vector<c_task_graph::s_buffer_usage_info> &buffer_usage_info);

	void mix_to_channel_buffers(std::vector<c_buffer> &source_buffers);
	void free_channel_buffers();

#if IS_TRUE(ASSERTS_ENABLED)
	void assert_all_task_buffers_free(e_instrument_stage instrument_stage) const;
	void assert_all_buffers_free() const;
#else // IS_TRUE(ASSERTS_ENABLED)
	void assert_all_task_buffers_free(e_instrument_stage instrument_stage) const {}
	void assert_all_buffers_free() const {}
#endif // IS_TRUE(ASSERTS_ENABLED)

	const c_runtime_instrument *m_runtime_instrument;

	// Allocator for buffers used during processing
	c_buffer_allocator m_buffer_allocator;

	// Contexts for task buffers for each stage
	s_static_array<std::vector<s_task_buffer_context>, enum_count<e_instrument_stage>()> m_task_buffer_contexts;

	// List of all dynamic buffers indices for each stage
	s_static_array<std::vector<size_t>, enum_count<e_instrument_stage>()> m_dynamic_buffers;

	// Cached lists of dynamic input buffers for each task for each stage
	s_static_array<std::vector<std::vector<size_t>>, enum_count<e_instrument_stage>()> m_task_buffers_to_decrement;

	// Cached lists of dynamic output buffers for each task for each stage (de-duplicated)
	s_static_array<std::vector<std::vector<size_t>>, enum_count<e_instrument_stage>()> m_task_buffers_to_allocate;

	// Cached list of dynamic output buffers for each stage
	s_static_array<std::vector<size_t>, enum_count<e_instrument_stage>()> m_output_buffers_to_decrement;

	// Pool index of real buffers
	uint32 m_real_buffer_pool_index;

	// Pool indices for each voice output
	std::vector<uint32> m_voice_output_pool_indices;

	// Pool indices for each FX output
	std::vector<uint32> m_fx_output_pool_indices;

	// Buffers used to shift voice samples
	std::vector<c_buffer> m_voice_shift_buffers;

	// Buffers to accumulate the final multi-voice result into
	std::vector<c_buffer> m_voice_accumulation_buffers;

	// Buffers to store the FX processing result in
	std::vector<c_buffer> m_fx_output_buffers;

	// Buffers to mix to output channels
	std::vector<c_buffer> m_channel_mix_buffers;

	// Size of the current chunk
	uint32 m_chunk_size;

	// Number of voices processed so far this chunk
	uint32 m_voices_processed;

	// Whether FX has been processed
	bool m_fx_processed;
};

