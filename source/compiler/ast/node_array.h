#pragma once

#include "common/common.h"

#include "compiler/ast/node_expression.h"

#include <memory>
#include <vector>

class c_AST_node_array : public c_AST_node_expression {
public:
	AST_NODE_TYPE_DESCRIPTION(k_array, "array");
	c_AST_node_array();

	void set_data_type(const c_AST_qualified_data_type &data_type);

	void add_element(const c_AST_node_expression *element);
	size_t get_element_count() const;
	c_AST_node_expression *get_element(size_t index) const;

private:
	std::vector<std::unique_ptr<c_AST_node_expression>> m_elements;
};
