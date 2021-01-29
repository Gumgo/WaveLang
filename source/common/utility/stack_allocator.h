#pragma once

#include "common/common.h"

class c_stack_allocator {
public:
	class c_memory_calculator {
	public:
		c_memory_calculator() = default;

		template<typename t_type>
		void add(size_t alignment = alignof(t_type)) {
			add_array<t_type>(1, alignment);
		}

		template<typename t_type>
		void add_array(size_t count, size_t alignment = alignof(t_type)) {
			wl_assert(alignment >= alignof(t_type));
			if (count == 0) {
				return;
			}

			m_size_alignment.size = align_size(m_size_alignment.size, alignment);
			m_size_alignment.size += sizeof(t_type) * count;
			m_size_alignment.alignment = std::max(m_size_alignment.alignment, alignment);

			if constexpr (std::is_destructible_v<t_type> && !std::is_trivially_destructible_v<t_type>) {
				m_destructor_count++;
			}
		}

		s_size_alignment get_size_alignment() const;

	private:
		s_size_alignment m_size_alignment{ 0, 1 };
		size_t m_destructor_count = 0;
	};

	struct s_context {
		c_wrapped_array<uint8> buffer;
		size_t destructor_count = 0;
	};

	c_stack_allocator(c_wrapped_array<uint8> buffer);
	~c_stack_allocator();

	UNCOPYABLE(c_stack_allocator);

	// Call this to release the context of the allocator so memory doesn't get freed on destruction
	s_context release();

	void free();
	static void free_context(const s_context &context);

	template<typename t_type>
	void allocate(t_type *&result, size_t alignment = alignof(t_type)) {
		c_wrapped_array<t_type> array_result;
		allocate_array<t_type>(array_result, 1, alignment);
	}

	template<typename t_type>
	void allocate_array(c_wrapped_array<t_type> &result, size_t count, size_t alignment = alignof(t_type)) {
		wl_assert(alignment >= alignof(t_type));
		if (count == 0) {
			result = c_wrapped_array<t_type>();
			return;
		}

		size_t start_offset = align_size(m_offset, alignment);
		size_t end_offset = start_offset + sizeof(t_type) * count;
		static constexpr bool k_has_destructor =
			std::is_destructible_v<t_type> && !std::is_trivially_destructible_v<t_type>;

		IF_ASSERTS_ENABLED(size_t destructor_count = m_context.destructor_count + k_has_destructor;)
		wl_assert(end_offset <= m_context.buffer.get_count() - sizeof(s_destructor_entry) * destructor_count);

		wl_assert(is_pointer_aligned(&m_context.buffer[start_offset], alignment));
		for (size_t index = 0; index < count; index++) {
			new (&m_context.buffer[start_offset + sizeof(t_type) * index]) t_type();
		}
		m_offset = end_offset;

		if constexpr (k_has_destructor) {
			m_context.destructor_count++;
			size_t destructor_entry_offset =
				m_context.buffer.get_count() - sizeof(s_destructor_entry) * m_context.destructor_count;
			new (&m_context.buffer[destructor_entry_offset]) s_destructor_entry{
				start_offset,
				sizeof(t_type),
				count,
				call_destructor<t_type>
			};
		}
	}

public:
	friend class c_memory_calculator; // To access sizeof(s_destructor_entry)

	using f_call_destructor = void (*)(void *pointer);

	struct s_destructor_entry {
		size_t offset;
		size_t stride;
		size_t count;
		f_call_destructor call_destructor;
	};

	template<typename t_type>
	static void call_destructor(void *pointer) {
		static_cast<t_type *>(pointer)->~t_type();
	}

	s_context m_context{};
	size_t m_offset = 0;
};
