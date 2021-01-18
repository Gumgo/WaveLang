#pragma once

#include "common/common.h"

#include "compiler/ast/nodes.h"
#include "compiler/graph_trimmer.h"

#include "execution_graph/graph_node_handle.h"

#include <memory>
#include <unordered_map>
#include <vector>

class c_tracked_scope;

enum class e_tracked_scope_type {
	k_namespace,
	k_module,
	k_local,
	k_if_statement,
	k_for_loop,

	k_count
};

enum class e_tracked_event_state {
	k_did_not_occur,
	k_maybe_occurred,
	k_occurred
};

class c_tracked_declaration {
public:
	UNCOPYABLE_MOVABLE(c_tracked_declaration);
	~c_tracked_declaration();

	c_ast_node_declaration *get_declaration() const;

	// Keeps track of the last value assigned to this declaration when building an instrument variant
	h_graph_node get_node_handle() const;
	void set_node_handle(h_graph_node node_handle);

private:
	friend class c_tracked_scope;
	c_tracked_declaration(c_ast_node_declaration *declaration, c_graph_trimmer *graph_trimmer);

	c_ast_node_declaration *m_declaration = nullptr;
	c_graph_trimmer *m_graph_trimmer = nullptr;
	h_graph_node m_node_handle = h_graph_node::invalid();

	// Forms a linked list of scope items with the same name for quick lookups
	c_tracked_declaration *m_next_name_lookup = nullptr;
};

// Tracks progress through a scope
class c_tracked_scope {
public:
	c_tracked_scope(c_tracked_scope *parent, e_tracked_scope_type scope_type, c_graph_trimmer *graph_trimmer);
	UNCOPYABLE(c_tracked_scope);

	c_tracked_scope *get_parent();
	const c_tracked_scope *get_parent() const;
	e_tracked_scope_type get_scope_type() const;

	c_tracked_declaration *add_declaration(c_ast_node_declaration *declaration);
	c_tracked_declaration *get_tracked_declaration(c_ast_node_declaration *declaration);

	e_tracked_event_state get_declaration_assignment_state(c_ast_node_declaration *declaration) const;
	e_tracked_event_state get_return_state() const;

	void issue_declaration_assignment(c_ast_node_declaration *declaration);
	void issue_return_statement();
	void issue_break_statement();
	void issue_continue_statement();

	void merge_child_scope(c_tracked_scope *child_scope);
	void merge_child_if_statement_scopes(c_tracked_scope *true_scope, c_tracked_scope *false_scope);
	void merge_child_for_loop_scope(c_tracked_scope *for_loop_scope);

	// If this is not a namespace scope, we recurse down to the parent scope. If this is a namespace scope, we've
	// specifically followed access operators to get here, so we don't want to recurse back down, the intent is to
	// specifically limit to just this scope.
	void lookup_declarations_by_name(const char *name, std::vector<c_tracked_declaration *> &results_out);

private:
	void merge_optional_child_scope(c_tracked_scope *child_scope);
	e_tracked_event_state get_this_scope_declaration_assignment_state(c_ast_node_declaration *declaration) const;
	e_tracked_event_state update_event_state(e_tracked_event_state current_state) const;

	c_tracked_scope *m_parent = nullptr;
	e_tracked_scope_type m_scope_type;
	c_graph_trimmer *m_graph_trimmer = nullptr;
	std::vector<std::unique_ptr<c_tracked_declaration>> m_declarations;

	// Allows lookup of a tracked declaration from an AST declaration
	std::unordered_map<c_ast_node_declaration *, c_tracked_declaration *> m_tracked_declaration_lookup;

	// Points to a linked list of scope items with the same name
	std::unordered_map<std::string, c_tracked_declaration *> m_name_lookup_map;

	// Assignment state for each declaration that got assigned in this scope
	std::unordered_map<c_ast_node_declaration *, e_tracked_event_state> m_declaration_assignment_states;

	// Whether this scope executes a return statement
	e_tracked_event_state m_return_state = e_tracked_event_state::k_did_not_occur;

	// Whether this scope executes a break statement
	e_tracked_event_state m_break_state = e_tracked_event_state::k_did_not_occur;

	// Whether this scope executes a continue statement
	e_tracked_event_state m_continue_state = e_tracked_event_state::k_did_not_occur;
};
