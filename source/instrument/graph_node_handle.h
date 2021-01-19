#pragma once

#include "common/common.h"
#include "common/utility/handle.h"

#if IS_TRUE(ASSERTS_ENABLED)
#define NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED 1
#else // IS_TRUE(ASSERTS_ENABLED)
#define NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED 0
#endif // IS_TRUE(ASSERTS_ENABLED)

struct s_graph_node_handle_data {
	uint32 index;
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
	uint32 salt;
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)

	static constexpr s_graph_node_handle_data invalid() {
		return {
			static_cast<uint32>(-1),
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
			static_cast<uint32>(-1)
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
		};
	}

	bool operator==(const s_graph_node_handle_data &other) const {
		bool result = index == other.index;
#if IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
		wl_assertf(!result || salt == other.salt, "Salt mismatch");
#endif // IS_TRUE(NATIVE_MODULE_GRAPH_NODE_SALT_ENABLED)
		return result;
	}

	bool operator!=(const s_graph_node_handle_data &other) const {
		return !(*this == other);
	}
};

namespace std {
	template<>
	struct hash<s_graph_node_handle_data> {
		size_t operator()(const s_graph_node_handle_data &key) const {
			return hash<uint32>()(key.index);
		}
	};
}

struct s_make_invalid_graph_node_handle_data {
	constexpr s_graph_node_handle_data operator()() const {
		return s_graph_node_handle_data::invalid();
	}
};

struct s_graph_node_handle_identifier {};
using h_graph_node = c_handle<
	s_graph_node_handle_identifier,
	s_graph_node_handle_data,
	s_make_invalid_graph_node_handle_data>;
