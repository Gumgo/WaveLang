#include "compiler/ast/node_scope.h"

c_ast_node_scope::c_ast_node_scope()
	: c_ast_node_scope_item(k_ast_node_type) {}

void c_ast_node_scope::add_scope_item(c_ast_node_scope_item *scope_item) {
	add_scope_item_internal(scope_item, false, false);
}

void c_ast_node_scope::add_imported_scope_item(c_ast_node_scope_item *scope_item, bool take_ownership) {
	add_scope_item_internal(scope_item, true, take_ownership);
}

size_t c_ast_node_scope::get_scope_item_count() const {
	return m_scope_items.size();
}

c_ast_node_scope_item *c_ast_node_scope::get_scope_item(size_t index) const {
	return m_scope_items[index].scope_item;
}

size_t c_ast_node_scope::get_non_imported_scope_item_count() const {
	return m_non_imported_scope_item_indices.size();
}

c_ast_node_scope_item *c_ast_node_scope::get_non_imported_scope_item(size_t index) const {
	return m_scope_items[m_non_imported_scope_item_indices[index]].scope_item;
}

void c_ast_node_scope::lookup_declarations_by_name(
	const char *name,
	std::vector<c_ast_node_declaration *> &declarations_out) const {
	declarations_out.clear();

	for (const s_scope_item_entry &scope_item_entry : m_scope_items) {
		c_ast_node_declaration *declaration = scope_item_entry.scope_item->try_get_as<c_ast_node_declaration>();
		if (declaration && declaration->get_name() == name) {
			declarations_out.push_back(declaration);
		}
	}
}

c_ast_node *c_ast_node_scope::copy_internal() const {
	c_ast_node_scope *node_copy = new c_ast_node_scope();
	node_copy->m_scope_items.reserve(m_scope_items.size());
	for (const s_scope_item_entry &scope_item_entry : m_scope_items) {
		if (scope_item_entry.owned_scope_item) {
			node_copy->m_scope_items.emplace_back(s_scope_item_entry(scope_item_entry.owned_scope_item->copy(), true));
		} else {
			node_copy->m_scope_items.emplace_back(s_scope_item_entry(scope_item_entry.scope_item, false));
		}
	}

	node_copy->m_non_imported_scope_item_indices.assign(
		m_non_imported_scope_item_indices.begin(),
		m_non_imported_scope_item_indices.end());
	return node_copy;
}

void c_ast_node_scope::add_scope_item_internal(
	c_ast_node_scope_item *scope_item,
	bool is_imported,
	bool take_ownership) {
	if (!is_imported) {
		m_non_imported_scope_item_indices.push_back(m_scope_items.size());
	}
	m_scope_items.emplace_back(s_scope_item_entry(scope_item, take_ownership));
}
