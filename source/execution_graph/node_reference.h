#pragma once

#include "common/common.h"

#ifdef _DEBUG
#define EXECUTION_GRAPH_NODE_SALT_ENABLED 1
#else // _DEBUG
#define EXECUTION_GRAPH_NODE_SALT_ENABLED 0
#endif // _DEBUG

// $TODO change this to a regular c_handle with a special data type that includes salt. Note that ordinary handles don't
// get initialized in the constructor.
class c_node_reference {
public:
	c_node_reference() {
		m_node_index = k_invalid_node_index;
#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
		m_salt = k_invalid_salt;
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	}

	c_node_reference(const c_node_reference &) = default;
	c_node_reference &operator=(const c_node_reference &) = default;

	bool is_valid() const {
		return m_node_index != k_invalid_node_index;
	}

	bool operator==(const c_node_reference &other) const {
		bool result = m_node_index == other.m_node_index;
#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
		wl_assertf(!result || m_salt == other.m_salt, "Salt mismatch");
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
		return result;
	}

	bool operator!=(const c_node_reference &other) const {
		return !(*this == other);
	}

	// For use in sets:
	bool operator<(const c_node_reference &other) const {
#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
		wl_assertf(m_node_index != other.m_node_index || m_salt == other.m_salt, "Salt mismatch");
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
		return m_node_index < other.m_node_index;
	}

private:
	friend class c_execution_graph;
	friend struct std::hash<c_node_reference>;

	template<typename t_argument_reference>
	friend c_node_reference node_reference_from_argument_reference(t_argument_reference argument_reference);

	static constexpr uint32 k_invalid_node_index = static_cast<uint32>(-1);
	static constexpr uint32 k_invalid_salt = static_cast<uint32>(-1);

#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	c_node_reference(uint32 node_index, uint32 salt) {
		m_node_index = node_index;
		m_salt = salt;
	}
#else // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	c_node_reference(uint32 node_index) {
		m_node_index = node_index;
	}
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)

	uint32 get_node_index() const {
		return m_node_index;
	}

#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	uint32 get_salt() const {
		return m_salt;
	}
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)

	uint32 m_node_index;
#if IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
	uint32 m_salt;
#endif // IS_TRUE(EXECUTION_GRAPH_NODE_SALT_ENABLED)
};

namespace std {
	template<>
	struct hash<c_node_reference> {
		size_t operator()(const c_node_reference &key) const {
			return hash<uint32>()(key.get_node_index());
		}
	};
}
