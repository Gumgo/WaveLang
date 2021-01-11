#pragma once

#include "common/common.h"

#include "compiler/ast/node.h"
#include "compiler/ast/node_declaration.h"
#include "compiler/ast/node_scope_item.h"

#include <memory>
#include <vector>

class c_ast_node_scope : public c_ast_node_scope_item {
public:
	AST_NODE_TYPE_DESCRIPTION(c_ast_node_scope, k_scope, "scope");
	c_ast_node_scope();

	void add_scope_item(c_ast_node_scope_item *scope_item);

	// In some cases, if a scope item is being imported, this node doesn't take ownership
	void add_imported_scope_item(c_ast_node_scope_item *scope_item, bool take_ownership);

	size_t get_scope_item_count() const;
	c_ast_node_scope_item *get_scope_item(size_t index) const;

	size_t get_non_imported_scope_item_count() const;
	c_ast_node_scope_item *get_non_imported_scope_item(size_t index) const;

	void lookup_declarations_by_name(const char *name, std::vector<c_ast_node_declaration *> &declarations_out) const;

protected:
	c_ast_node *copy_internal() const override;

private:
	struct s_scope_item_entry {
		s_scope_item_entry(c_ast_node_scope_item *item, bool take_ownership)
			: owned_scope_item(take_ownership ? item : nullptr)
			, scope_item(item) {}
		UNCOPYABLE_MOVABLE(s_scope_item_entry);

		std::unique_ptr<c_ast_node_scope_item> owned_scope_item;
		c_ast_node_scope_item *scope_item;
	};

	void add_scope_item_internal(c_ast_node_scope_item *scope_item, bool is_imported, bool take_ownership);

	std::vector<s_scope_item_entry> m_scope_items;
	std::vector<size_t> m_non_imported_scope_item_indices;
};
