#pragma once

#include "common/common.h"

#include "compiler/ast/node.h"
#include "compiler/ast/node_declaration.h"
#include "compiler/ast/node_scope_item.h"

#include <memory>
#include <vector>

class c_AST_node_scope : public c_AST_node_scope_item {
public:
	AST_NODE_TYPE_DESCRIPTION(c_AST_node_scope, k_scope, "scope");
	c_AST_node_scope();

	// If a scope item is being imported, this node doesn't take ownership
	void add_scope_item(c_AST_node_scope_item *scope_item, bool take_ownership = true);
	size_t get_scope_item_count() const;
	c_AST_node_scope_item *get_scope_item(size_t index) const;

	void lookup_declarations_by_name(const char *name, std::vector<c_AST_node_declaration *> &declarations_out) const;

protected:
	c_AST_node *copy_internal() const override;

private:
	struct s_scope_item_entry {
		s_scope_item_entry(c_AST_node_scope_item *item, bool take_ownership)
			: owned_scope_item(take_ownership ? nullptr : item)
			, scope_item(item) {}
		UNCOPYABLE_MOVABLE(s_scope_item_entry);

		std::unique_ptr<c_AST_node_scope_item> owned_scope_item;
		c_AST_node_scope_item *scope_item;
	};

	std::vector<s_scope_item_entry> m_scope_items;
};
