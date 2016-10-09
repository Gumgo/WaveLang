#ifndef WAVELANG_PREDECESSOR_RESOLVER_H__
#define WAVELANG_PREDECESSOR_RESOLVER_H__

#include "common/common.h"

#include <vector>

// Utility class used to fully resolve predecessors in a graph
class c_predecessor_resolver {
public:
	c_predecessor_resolver(uint32 node_count) {
		m_node_count = node_count;
		m_row_size = (node_count + 31) / 32;
		m_data.resize(m_node_count * m_row_size);
	}

	void a_precedes_b(uint32 a, uint32 b) {
		// Set bit at col a, row b
		// This way, we can look at row b and get a contiguous list of the things which precede it
		m_data[b * m_row_size + (a / 32)] |= (1 << (a % 32));
	}

	bool does_a_precede_b(uint32 a, uint32 b) const {
		return (m_data[b * m_row_size + (a / 32)] & (1 << (a % 32))) != 0;
	}

	bool are_a_b_concurrent(uint32 a, uint32 b) const {
		return !does_a_precede_b(a, b) && !does_a_precede_b(b, a);
	}

	void resolve();

private:
	uint32 m_node_count;
	size_t m_row_size;
	std::vector<uint32> m_data;
};

#endif // WAVELANG_PREDECESSOR_RESOLVER_H__