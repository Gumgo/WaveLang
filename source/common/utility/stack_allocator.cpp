#include "common/utility/stack_allocator.h"

s_size_alignment c_stack_allocator::c_memory_calculator::get_size_alignment() const {
	s_size_alignment result = m_size_alignment;
	if (m_destructor_count > 0) {
		result.size = align_size(result.size, alignof(s_destructor_entry));
		result.size += sizeof(s_destructor_entry) * m_destructor_count;
		result.alignment = std::max(result.alignment, alignof(s_destructor_entry));
	}

	return result;
}

size_t c_stack_allocator::c_memory_calculator::get_destructor_count() const {
	return m_destructor_count;
}

c_stack_allocator::c_stack_allocator(c_wrapped_array<uint8> buffer) {
	m_context.buffer = buffer;
}

c_stack_allocator::~c_stack_allocator() {
	free();
}

c_stack_allocator::s_context c_stack_allocator::release() {
	s_context context = m_context;
	m_context = {};
	m_offset = 0;
	return context;
}

c_stack_allocator::s_context c_stack_allocator::release_no_destructors() {
	wl_assert(m_context.destructor_count == 0);
	return release();
}

void c_stack_allocator::free() {
	free_context(m_context);
	m_context = {};
	m_offset = 0;
}

void c_stack_allocator::free_context(const s_context &context) {
	// Call all destructors in reverse order
	for (size_t destructor_index = 0; destructor_index < context.destructor_count; destructor_index++) {
		size_t reverse_destructor_index = context.destructor_count - destructor_index;
		size_t destructor_entry_offset =
			context.buffer.get_count() - sizeof(s_destructor_entry) * reverse_destructor_index;
		const s_destructor_entry *destructor_entry =
			reinterpret_cast<s_destructor_entry *>(context.buffer[destructor_entry_offset]);

		// Destroy elements in reverse order
		for (size_t index = 0; index < destructor_entry->count; index++) {
			size_t reverse_index = destructor_entry->count - index - 1;
			void *pointer = &context.buffer[destructor_entry->offset + destructor_entry->stride * reverse_index];
			destructor_entry->call_destructor(pointer);
		}
	}
}
