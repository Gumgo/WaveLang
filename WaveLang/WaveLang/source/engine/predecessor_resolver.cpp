#include "engine/predecessor_resolver.h"

#define OUTPUT_PREDECESSOR_RESOLUTION 1

#if PREDEFINED(OUTPUT_PREDECESSOR_RESOLUTION)
#include <fstream>
#endif // PREDEFINED(OUTPUT_PREDECESSOR_RESOLUTION)

void c_predecessor_resolver::resolve() {
	if (m_node_count == 0) {
		return;
	}

#if PREDEFINED(OUTPUT_PREDECESSOR_RESOLUTION)
	std::ofstream out("predecessor_resolution.txt");
	for (uint32 b = 0; b < m_node_count; b++) {
		for (uint32 a = 0; a < m_node_count; a++) {
			out << (does_a_precede_b(a, b) ? "x " : ". ");
		}

		out << "\n";
	}
	out << "\n";
#endif // PREDEFINED(OUTPUT_PREDECESSOR_RESOLUTION)

	// Keep looping until nothing changes
	bool done;
	std::vector<uint32> last_columns_updated(m_row_size, 0xffffffff);
	std::vector<uint32> next_columns_to_update(m_row_size, 0);
	do {
		done = true;

		// Initialize the next update set
		memset(&next_columns_to_update.front(), 0, m_row_size * sizeof(uint32));

		// Loop over each possible successor
		for (uint32 b = 0; b < m_node_count; b++) {

			// Loop over each possible predecessor - since predecessor lists are stored as rows and this is the inner
			// loop, we can batch 32 at once and skip groups of 0
			for (uint32 col_group_index = 0; col_group_index < m_row_size; col_group_index++) {
				uint32 col_group = last_columns_updated[col_group_index];
				if (col_group == 0) {
					// Skip 32 columns at once!
					continue;
				}

				for (uint32 bit = 0; bit < 32; bit++) {
					if ((col_group & (1 << bit)) == 0) {
						continue;
					}

					uint32 a = (col_group_index * 32) + bit;

					if (!does_a_precede_b(a, b)) {
						continue;
					}

					// We have determined that a precedes b, meaning that everything that precedes a also precedes b.
					// Therefore, we should OR a's row into b's row.

					size_t a_predecessor_start = a * m_row_size;
					size_t b_predecessor_start = b * m_row_size;

					for (size_t index = 0; index < m_row_size; index++) {
						size_t a_index = a_predecessor_start + index;
						size_t b_index = b_predecessor_start + index;

						uint32 value_before = m_data[b_index];
						uint32 value_after = value_before | m_data[a_index];
						m_data[b_index] |= value_after;

						// Find which bits changed so we know which columns to look at next iteration
						uint32 new_bits = value_before ^ value_after;
						next_columns_to_update[index] |= new_bits;

						// If we ever are setting new bits, we need to do another iteration
						done |= (new_bits != 0);
					}
				}
			}
		}

		// Swap over to the next update set
		last_columns_updated.swap(next_columns_to_update);
	} while (!done);

#if PREDEFINED(OUTPUT_PREDECESSOR_RESOLUTION)
	for (uint32 b = 0; b < m_node_count; b++) {
		for (uint32 a = 0; a < m_node_count; a++) {
			out << (does_a_precede_b(a, b) ? "x " : ". ");
		}

		out << "\n";
	}
#endif PREDEFINED(OUTPUT_PREDECESSOR_RESOLUTION)
}