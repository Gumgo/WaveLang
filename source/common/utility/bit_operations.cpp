#include "common/utility/bit_operations.h"

class c_bit_reader {
public:
	c_bit_reader(const uint32 *source, size_t offset, size_t count) {
		wl_assert(count > 0);

		m_source = source + offset / 32;
#if IS_TRUE(ASSERTS_ENABLED)
		m_source_end = source + (offset + count + 31) / 32;
#endif // IS_TRUE(ASSERTS_ENABLED)

		m_shift = offset % 32;
		m_count = count;
		if (m_shift == 0) {
			m_prev_value = 0;
			m_shift = 32;
		} else {
			m_prev_value = *m_source++;
		}

		m_index = 32 - m_shift;
	}

	uint32 read() {
		// We expect to over-read by up to 31 due to the initial read
		wl_assert(m_index < m_count + 32);

		// Cast to uint64 so that shifting by 32 works
		uint32 result = static_cast<uint32>(static_cast<uint64>(m_prev_value) >> m_shift);
		if (m_index < m_count) {
			wl_assert(m_source < m_source_end);
			uint32 value = *m_source++;
			result |= static_cast<uint32>(value << (32ull - m_shift));
			m_prev_value = value;
		}

		m_index += 32;
		return result;
	}

private:
	const uint32 *m_source;		// Source pointer to read from
#if IS_TRUE(ASSERTS_ENABLED)
	const uint32 *m_source_end;	// Make sure we don't read past the end
#endif // IS_TRUE(ASSERTS_ENABLED)
	size_t m_shift;				// Bit offset of the start of the range from aligned int32 boundary
	size_t m_count;				// Total number of bits to read
	size_t m_index;				// Number of bits read so far - this is expected to slightly exceed m_count
	uint32 m_prev_value;		// Bits of the current word read so far
};

class c_bit_writer {
public:
	c_bit_writer(uint32 *destination, size_t offset, size_t count) {
		wl_assert(count > 0);

		m_destination = destination + offset / 32;
#if IS_TRUE(ASSERTS_ENABLED)
		m_destination_end = destination + (offset + count + 31) / 32;
#endif // IS_TRUE(ASSERTS_ENABLED)

		m_shift = offset % 32;
		m_count = count;
		m_index = 0;
	}

	void write(uint32 value) {
		wl_assert(m_index < m_count);

		size_t bits_remaining = m_count - m_index;
		size_t bits_to_drop = 32 - std::min(bits_remaining, 32ull);
		uint32 mask = static_cast<uint32>(0xffffffffull >> bits_to_drop);
		value &= mask;

		uint32 mask_a = mask << m_shift;
		wl_assert(m_destination < m_destination_end);
		*m_destination = (*m_destination & ~mask_a) | static_cast<uint32>(static_cast<uint64>(value) << m_shift);
		m_destination++;

		if (m_shift != 0) {
			size_t inv_shift = 32 - m_shift;
			uint32 mask_b = mask >> inv_shift;

			// If mask_b is empty, don't perform the write because we might be writing off the end
			if (mask_b != 0) {
				wl_assert(m_destination < m_destination_end);
				*m_destination = (*m_destination & ~mask_b) | (value >> inv_shift);
			}
		}

		m_index += 32;
	}

private:
	uint32 *m_destination;		// Destination pointer to write to
#if IS_TRUE(ASSERTS_ENABLED)
	uint32 *m_destination_end;	// Make sure we don't write past the end
#endif // IS_TRUE(ASSERTS_ENABLED)
	size_t m_shift;				// Bit offset of the start of the range from aligned int32 boundary
	size_t m_count;				// Total number of bits to write
	size_t m_index;				// Number of bits written so far
};

void copy_bits(int32 *destination, size_t destination_offset, const int32 *source, size_t source_offset, size_t count) {
	if (count == 0) {
		return;
	}

	c_bit_reader reader(reinterpret_cast<const uint32 *>(source), source_offset, count);
	c_bit_writer writer(reinterpret_cast<uint32 *>(destination), destination_offset, count);

	size_t int32_count = (count + 31) / 32;
	for (size_t index = 0; index < int32_count; index++) {
		writer.write(reader.read());
	}
}

void replicate_bit(int32 *destination, size_t destination_offset, bool value, size_t count) {
	// Determine the indices of the first and last int32s that this range touches
	size_t start_index = destination_offset / 32;
	size_t end_index = (destination_offset + count + 31) / 32;

	// Determine the indices of the first and last int32s that this range fully covers
	size_t full_start_index = (destination_offset + 31) / 32;
	size_t full_end_index = (destination_offset + count) / 32;

	// Blast over the fully-covered range
	int32 full_value = value ? 0xffffffff : 0;
	for (size_t index = full_start_index; index < full_end_index; index++) {
		destination[index] = full_value;
	}

	bool partial_start = start_index < full_start_index;
	bool partial_end = end_index > full_end_index;
	if (partial_start && partial_end && start_index + 1 == end_index) {
		// The partial start and end are covering the same int32
		size_t start_skip_bits = destination_offset % 32;
		size_t end_skip_bits = 32 - ((destination_offset + count) % 32);
		wl_assert(start_skip_bits != 32);
		wl_assert(end_skip_bits != 32);
		int32 start_mask = 0xffffffff << start_skip_bits;
		int32 end_mask = static_cast<int32>(0xffffffffu >> end_skip_bits);
		int32 mask = start_mask & end_mask;
		if (value) {
			destination[start_index] |= mask;
		} else {
			destination[start_index] &= ~mask;
		}
	} else {
		if (partial_start) {
			// The int32 at the start of the range is only partially covered
			size_t skip_bits = destination_offset % 32;
			wl_assert(skip_bits != 32);
			int32 mask = 0xffffffff << skip_bits;
			if (value) {
				destination[start_index] |= mask;
			} else {
				destination[start_index] &= ~mask;
			}
		}

		if (partial_end) {
			// The int32 at the end of the range is only partially covered
			size_t skip_bits = 32 - ((destination_offset + count) % 32);
			wl_assert(skip_bits != 32);
			int32 mask = static_cast<int32>(0xffffffffu >> skip_bits);
			if (value) {
				destination[end_index - 1] |= mask;
			} else {
				destination[end_index - 1] &= ~mask;
			}
		}
	}
}
