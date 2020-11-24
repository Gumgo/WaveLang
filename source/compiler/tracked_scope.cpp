#include "compiler/tracked_scope.h"

#include <unordered_set>

static e_tracked_event_state event_state_min(e_tracked_event_state event_state_a, e_tracked_event_state event_state_b);
static e_tracked_event_state event_state_max(e_tracked_event_state event_state_a, e_tracked_event_state event_state_b);

c_AST_node_declaration *c_tracked_declaration::get_declaration() const {
	return m_declaration;
}

c_tracked_declaration::c_tracked_declaration(c_AST_node_declaration *declaration) {
	m_declaration = declaration;
	m_next_name_lookup = nullptr;
}

c_tracked_scope::c_tracked_scope(c_tracked_scope *parent, e_tracked_scope_type scope_type) {
	m_parent = parent;
	m_scope_type = scope_type;
}

c_tracked_scope *c_tracked_scope::get_parent() {
	return m_parent;
}

const c_tracked_scope *c_tracked_scope::get_parent() const {
	return m_parent;
}

e_tracked_scope_type c_tracked_scope::get_scope_type() const {
	return m_scope_type;
}

c_tracked_declaration *c_tracked_scope::add_declaration(c_AST_node_declaration *declaration) {
	wl_assert(m_tracked_declaration_lookup.find(declaration) == m_tracked_declaration_lookup.end());

	c_tracked_declaration *tracked_declaration = new c_tracked_declaration(declaration);
	m_declarations.emplace_back(tracked_declaration);

	m_tracked_declaration_lookup.insert(std::make_pair(declaration, tracked_declaration));

	if (declaration->get_name()) {
		std::string name = declaration->get_name();
		auto iter = m_name_lookup_map.find(name);
		if (iter == m_name_lookup_map.end()) {
			m_name_lookup_map.insert(std::make_pair(name, tracked_declaration));
		} else {
			tracked_declaration->m_next_name_lookup = iter->second;
			iter->second = tracked_declaration;
		}
	}

	return tracked_declaration;
 }

c_tracked_declaration *c_tracked_scope::get_tracked_declaration(c_AST_node_declaration *declaration) {
	auto iter = m_tracked_declaration_lookup.find(declaration);
	if (iter == m_tracked_declaration_lookup.end()) {
		return m_parent ? m_parent->get_tracked_declaration(declaration) : nullptr;
	} else {
		return iter->second;
	}
}

e_tracked_event_state c_tracked_scope::get_declaration_assignment_state(
	c_AST_node_declaration *declaration) const {
	if (!declaration->get_as<c_AST_node_value_declaration>()->is_modifiable()) {
		// If this value is unmodifiable, it must have been assigned at initialization (the only case of this currently
		// is constants declared at global scope).
		return e_tracked_event_state::k_occurred;
	}

	// Search down the scopes to determine if the assignment occurred
	e_tracked_event_state event_state = e_tracked_event_state::k_did_not_occur;
	const c_tracked_scope *current_scope = this;
	while (true) {
		// We should never go off the end of the scope stack
		wl_assert(current_scope);

		event_state = event_state_max(
			event_state,
			current_scope->get_this_scope_declaration_assignment_state(declaration));
		if (event_state == e_tracked_event_state::k_occurred) {
			return event_state;
		}

		if (current_scope->m_tracked_declaration_lookup.find(declaration)
			== current_scope->m_tracked_declaration_lookup.end()) {
			// This is the scope that contains the declaration so we don't find it lower down
			return event_state;
		} else {
			// We couldn't find an assignment state in this scope so look in the parent scope
			current_scope = current_scope->m_parent;
		}
	}
}

e_tracked_event_state c_tracked_scope::get_return_state() const {
	// Search down the scopes to determine if a return statement
	e_tracked_event_state event_state = e_tracked_event_state::k_did_not_occur;
	const c_tracked_scope *current_scope = this;
	while (true) {
		// We should never go off the end of the scope stack
		wl_assert(current_scope);

		event_state = event_state_max(event_state, current_scope->m_return_state);
		if (event_state == e_tracked_event_state::k_occurred) {
			return event_state;
		}

		if (current_scope->m_scope_type == e_tracked_scope_type::k_module) {
			return event_state;
		} else {
			// We couldn't find a return state in this scope so look in the parent scope
			current_scope = current_scope->m_parent;
		}
	}
}

void c_tracked_scope::issue_declaration_assignment(c_AST_node_declaration *declaration) {
	e_tracked_event_state current_state = get_this_scope_declaration_assignment_state(declaration);
	e_tracked_event_state new_state = update_event_state(current_state);

	if (new_state != e_tracked_event_state::k_did_not_occur) {
		m_declaration_assignment_states.insert_or_assign(declaration, new_state);
	} else {
		wl_assert(current_state == e_tracked_event_state::k_did_not_occur);
	}
}

void c_tracked_scope::issue_return_statement() {
	m_return_state = update_event_state(m_return_state);
}

void c_tracked_scope::issue_break_statement() {
	m_break_state = update_event_state(m_break_state);
}

void c_tracked_scope::issue_continue_statement() {
	m_continue_state = update_event_state(m_continue_state);
}

void c_tracked_scope::merge_child_scope(c_tracked_scope *child_scope) {
	// Everything that happened in the child scope gets inherited back down to this scope
	for (auto iter : child_scope->m_declaration_assignment_states) {
		e_tracked_event_state new_event_state = event_state_max(
			get_this_scope_declaration_assignment_state(iter.first),
			iter.second);
		m_declaration_assignment_states.insert_or_assign(iter.first, new_event_state);
	}

	m_return_state = event_state_max(m_return_state, child_scope->m_return_state);
	m_break_state = event_state_max(m_break_state, child_scope->m_break_state);
	m_continue_state = event_state_max(m_continue_state, child_scope->m_continue_state);
}

void c_tracked_scope::merge_child_if_statement_scopes(c_tracked_scope *true_scope, c_tracked_scope *false_scope) {
	// There won't be a "false" scope if there wasn't an "else"
	if (!false_scope) {
		merge_optional_child_scope(true_scope);
	} else {
		// We take the min of events in each branch and inherit that. This causes "maybe" to be inherited as expected,
		// but we'll inherit "definitely" if the event "definitely" happened in both branches.
		for (auto iter : true_scope->m_declaration_assignment_states) {
			e_tracked_event_state inherited_event_state = event_state_min(
				iter.second,
				false_scope->get_this_scope_declaration_assignment_state(iter.first));
			e_tracked_event_state new_event_state = event_state_max(
				get_this_scope_declaration_assignment_state(iter.first),
				inherited_event_state);
			m_declaration_assignment_states.insert_or_assign(iter.first, new_event_state);
		}

		for (auto iter : false_scope->m_declaration_assignment_states) {
			e_tracked_event_state inherited_event_state = event_state_min(
				iter.second,
				true_scope->get_this_scope_declaration_assignment_state(iter.first));
			e_tracked_event_state new_event_state = event_state_max(
				get_this_scope_declaration_assignment_state(iter.first),
				inherited_event_state);
			m_declaration_assignment_states.insert_or_assign(iter.first, new_event_state);
		}

		m_return_state = event_state_max(
			m_return_state,
			event_state_min(true_scope->m_return_state, false_scope->m_return_state));
		m_break_state = event_state_max(
			m_break_state,
			event_state_min(true_scope->m_break_state, false_scope->m_break_state));
		m_continue_state = event_state_max(
			m_continue_state,
			event_state_min(true_scope->m_continue_state, false_scope->m_continue_state));
	}
}

void c_tracked_scope::merge_child_for_loop_scope(c_tracked_scope *for_loop_scope) {
	// The for loop executed 0 or more times, so it is optional
	merge_optional_child_scope(for_loop_scope);
}

void c_tracked_scope::lookup_declarations_by_name(
	const char *name,
	std::vector<c_tracked_declaration *> &results_out) {
	results_out.clear();

	auto iter = m_name_lookup_map.find(std::string(name));
	c_tracked_declaration *next_result = (iter == m_name_lookup_map.end()) ? nullptr : iter->second;
	while (next_result) {
		results_out.push_back(next_result);
		next_result = next_result->m_next_name_lookup;
	}

	// Note: if overloaded modules are declared at different scopes, this will only find the ones declared at the
	// innermost scope. We currently only support declaring modules at global scope so this isn't an issue, but in the
	// future it may be worth revisiting.
	if (m_scope_type != e_tracked_scope_type::k_namespace && results_out.empty() && m_parent) {
		m_parent->lookup_declarations_by_name(name, results_out);
	}
}

void c_tracked_scope::merge_optional_child_scope(c_tracked_scope *child_scope) {
	// This scope may or may not run, so anything that definitely happened in it needs to be downgraded to a "maybe"
	for (auto iter : child_scope->m_declaration_assignment_states) {
		e_tracked_event_state new_event_state = event_state_max(
			get_this_scope_declaration_assignment_state(iter.first),
			event_state_min(iter.second, e_tracked_event_state::k_maybe_occurred));
		m_declaration_assignment_states.insert_or_assign(iter.first, new_event_state);
	}

	m_return_state = event_state_max(
		m_return_state,
		event_state_min(child_scope->m_return_state, e_tracked_event_state::k_maybe_occurred));
	m_break_state = event_state_max(
		m_break_state,
		event_state_min(child_scope->m_break_state, e_tracked_event_state::k_maybe_occurred));
	m_continue_state = event_state_max(
		m_continue_state,
		event_state_min(child_scope->m_continue_state, e_tracked_event_state::k_maybe_occurred));
}

e_tracked_event_state c_tracked_scope::get_this_scope_declaration_assignment_state(
	c_AST_node_declaration *declaration) const {
	auto iter = m_declaration_assignment_states.find(declaration);
	return iter == m_declaration_assignment_states.end() ? e_tracked_event_state::k_did_not_occur : iter->second;
}

e_tracked_event_state c_tracked_scope::update_event_state(e_tracked_event_state current_state) const {
	if (m_return_state == e_tracked_event_state::k_occurred
		|| m_break_state == e_tracked_event_state::k_occurred
		|| m_continue_state == e_tracked_event_state::k_occurred) {
		// This statement is not reachable because we either issued a return, break, or continue, so no change occurs
		return current_state;
	} else if (m_return_state == e_tracked_event_state::k_maybe_occurred
		|| m_break_state == e_tracked_event_state::k_maybe_occurred
		|| m_continue_state == e_tracked_event_state::k_maybe_occurred) {
		// We may have issued a return, break, or continue, so upgrade the event state to "maybe"
		return (current_state == e_tracked_event_state::k_did_not_occur)
			? current_state = e_tracked_event_state::k_maybe_occurred
			: current_state;
	} else {
		// We have not issued any return, break, or continue statements, so this event definitely occurred
		return e_tracked_event_state::k_occurred;
	}
}

static e_tracked_event_state event_state_min(e_tracked_event_state event_state_a, e_tracked_event_state event_state_b) {
	return static_cast<e_tracked_event_state>(std::max(enum_index(event_state_a), enum_index(event_state_b)));
}

static e_tracked_event_state event_state_max(e_tracked_event_state event_state_a, e_tracked_event_state event_state_b) {
	return static_cast<e_tracked_event_state>(std::max(enum_index(event_state_a), enum_index(event_state_b)));
}
