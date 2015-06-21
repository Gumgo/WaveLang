#include "compiler/ast_validator.h"
#include <deque>
#include <map>
#include <stack>

enum e_ast_validator_pass {
	k_ast_validator_pass_register_global_identifiers,
	k_ast_validator_pass_validate_syntax,

	k_ast_validator_pass_count
};

// $TODO let's just move this into the validator itself
struct s_ast_validator_context {
	std::vector<s_compiler_result> *errors;

	// If this is set, then the top scope is the outer-most scope for a module
	const c_ast_node_module_declaration *module_for_top_scope;

	struct s_named_value_context {
		// The statement number that this named value was assigned on, or -1 if it has not yet been assigned
		// In order to be assigned, this value must be -1
		// In order to be used, this value must be in the range [0, current_statement-1]
		int32 statement_assigned;
	};

	enum e_expression_expectation {
		k_expression_expectation_value_or_valueless,	// This expression can either be a value or have no value
		k_expression_expectation_value,					// This expression must have a value
		k_expression_expectation_assignment,			// This expression must be a single unassigned identifier

		k_expression_expectation_count
	};

	struct s_scope {
		const c_ast_node_module_declaration *module_for_scope;
		int32 return_statement;

		// List of used identifiers
		std::map<std::string, const c_ast_node *> identifiers;

		// Named value contexts
		std::map<const c_ast_node *, s_named_value_context> named_value_contexts;

		int32 current_statement;

		s_scope() {
			module_for_scope = nullptr;
			return_statement = -1;
			current_statement = 0;
		}
	};

	// Stack of scopes - global scope is at the bottom
	std::deque<s_scope> scope_stack;
	size_t scope_depth;

	// Used to validate that expressions resolve to their expected values
	// When sub-expressions are discovered, expecatations are pushed to this stack in reverse argument order
	std::stack<e_expression_expectation> expression_expectation_stack;

	struct s_module_call {
		bool visited;
		bool marked;
		const c_ast_node_module_declaration *module;
		// List of modules called by this module
		std::vector<const c_ast_node_module_declaration *> subcalls;

		s_module_call() {
			visited = false;
			marked = false;
			module = nullptr;
		}
	};

	// Keeps track of the graph of module calls to detect cycles
	std::vector<s_module_call> module_calls;

	bool entry_point_found;

	s_ast_validator_context() {
		module_for_top_scope = nullptr;
		scope_depth = 0;
		entry_point_found = false;
	}
};

static bool visit_module(s_ast_validator_context &context, size_t index) {
	s_ast_validator_context::s_module_call &module_call = context.module_calls[index];

	if (module_call.marked) {
		// Cycle
		return false;
	}

	if (!module_call.visited) {
		module_call.marked = true;
		for (size_t edge = 0; edge < module_call.subcalls.size(); edge++) {
			// Find the index for this edge
			size_t edge_index;
			for (edge_index = 0; edge_index < context.module_calls.size(); edge_index++) {
				if (context.module_calls[edge_index].module == module_call.subcalls[edge]) {
					break;
				}
			}
			wl_assert(edge_index != context.module_calls.size());

			if (!visit_module(context, edge_index)) {
				return false;
			}
		}

		module_call.visited = true;
		module_call.marked = false;
	}

	return true;
}

class c_ast_validator_visitor : public c_ast_node_const_visitor {
public:
	e_ast_validator_pass pass;
	s_ast_validator_context *context;

	// Adds an identifier to the scope at the given index if it does not exist in this scope or lower
	bool add_identifier_to_scope(size_t scope_stack_index, const std::string &identifier, const c_ast_node *node) {
		bool found = false;
		for (size_t index = scope_stack_index; !found && index < context->scope_stack.size(); index++) {
			const s_ast_validator_context::s_scope &scope = context->scope_stack[index];
			found = (scope.identifiers.find(identifier) != scope.identifiers.end());
		}

		if (!found) {
			context->scope_stack[scope_stack_index].identifiers.insert(std::make_pair(identifier, node));
		} else {
			s_compiler_result error;
			error.result = k_compiler_result_duplicate_identifier;
			error.source_location = node->get_source_location();
			error.message = "Identifier '" + identifier + "' is already in use";
			context->errors->push_back(error);
		}

		return !found;
	}

	// Searches for the identifier in the given scope and lower
	const c_ast_node *find_identifier(size_t scope_stack_index, const std::string &identifier) const {
		for (size_t index = scope_stack_index; index < context->scope_stack.size(); index++) {
			const s_ast_validator_context::s_scope &scope = context->scope_stack[index];
			auto match = scope.identifiers.find(identifier);
			if (match != scope.identifiers.end()) {
				return match->second;
			}
		}

		return nullptr;
	}

	s_ast_validator_context::s_named_value_context *find_named_value_context(
		size_t scope_stack_index, const c_ast_node *node) {
		for (size_t index = scope_stack_index; index < context->scope_stack.size(); index++) {
			s_ast_validator_context::s_scope &scope = context->scope_stack[index];
			auto match = scope.named_value_contexts.find(node);
			if (match != scope.named_value_contexts.end()) {
				return &match->second;
			}
		}

		return nullptr;
	}

	void detect_statements_after_return(s_ast_validator_context::s_scope &scope, const c_ast_node *node) {
		// If a return statement exists and this statement immediately follows, emit an error
		// The second condition is so we only emit one error for this
		if (scope.return_statement >= 0 && scope.current_statement == scope.return_statement + 1) {
			s_compiler_result error;
			error.result = k_compiler_result_statements_after_return;
			error.source_location = node->get_source_location();
			error.message = "Module '" + scope.module_for_scope->get_name() +
				"' contains statements after the return statement";
			context->errors->push_back(error);
		}
	}

	void push_scope() {
		if (context->scope_depth == 0 && !context->scope_stack.empty()) {
			// On the second pass, the global scope already exists
			wl_assert(pass == k_ast_validator_pass_validate_syntax);
		} else {
			context->scope_stack.push_front(s_ast_validator_context::s_scope());
		}
		context->scope_depth++;
	}

	virtual bool begin_visit(const c_ast_node_scope *node) {
		if (!context->module_for_top_scope) {
			// Only push a scope if a module declaration didn't already do it
			// This is so we can capture the module arguments in the new scope
			push_scope();
		}
		s_ast_validator_context::s_scope &scope = context->scope_stack.front();

		if (context->module_for_top_scope) {
			wl_assert(pass == k_ast_validator_pass_validate_syntax);
			scope.module_for_scope = context->module_for_top_scope;
			context->module_for_top_scope = nullptr;
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_scope *node) {
		s_ast_validator_context::s_scope &scope = context->scope_stack.front();

		// Process the end of modules here:
		if (scope.module_for_scope) {
			// Make sure a return statement was found if necessary
			if (scope.module_for_scope->get_has_return_value() &&
				scope.return_statement < 0) {
				s_compiler_result error;
				error.result = k_compiler_result_missing_return_statement;
				error.source_location = scope.module_for_scope->get_source_location();
				error.message = "Module '" + scope.module_for_scope->get_name() + "' is missing return statement";
				context->errors->push_back(error);
			}

			// Make sure all out arguments have been assigned
			for (size_t index = 0; index < scope.module_for_scope->get_argument_count(); index++) {
				const c_ast_node_named_value_declaration *argument = scope.module_for_scope->get_argument(index);

				if (argument->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_out) {
					// Look up the context
					s_ast_validator_context::s_named_value_context *match = find_named_value_context(0, argument);
					// Context must already exist for this argument
					wl_assert(match != nullptr);

					if (match->statement_assigned < 0) {
						s_compiler_result error;
						error.result = k_compiler_result_unassigned_output;
						error.source_location = argument->get_source_location();
						error.message = "Assignment for output argument '" + argument->get_name() + "' is missing";
						context->errors->push_back(error);
					} else {
						match->statement_assigned = scope.current_statement;
					}
				}
			}

			// Don't remove the module's name from the scope below, keep it around from the first pass
		}

		context->scope_depth--;
		if (context->scope_depth > 0) {
			// Don't pop global scope
			context->scope_stack.pop_front();
		}
	}

	virtual bool begin_visit(const c_ast_node_module_declaration *node) {
		if (pass == k_ast_validator_pass_register_global_identifiers) {
			// Add the module's name to the list of identifiers seen so far
			wl_assert(context->scope_stack.size() == 1);
			add_identifier_to_scope(0, node->get_name(), node);

			context->module_calls.push_back(s_ast_validator_context::s_module_call());
			context->module_calls.back().module = node;

			if (!context->entry_point_found &&
				node->get_name() == k_entry_point_name) {
				context->entry_point_found = true;

				// Should have no return value and only out arguments
				if (node->get_has_return_value()) {
					s_compiler_result error;
					error.result = k_compiler_result_invalid_entry_point;
					error.source_location = node->get_source_location();
					error.message = "Entry point '" + node->get_name() + "' must not return a value";
					context->errors->push_back(error);
				}

				bool all_out = true;
				for (size_t arg = 0; all_out && arg < node->get_argument_count(); arg++) {
					if (node->get_argument(arg)->get_qualifier() !=
						c_ast_node_named_value_declaration::k_qualifier_out) {
						all_out = false;
					}
				}

				if (!all_out) {
					s_compiler_result error;
					error.result = k_compiler_result_invalid_entry_point;
					error.source_location = node->get_source_location();
					error.message = "Entry point '" + node->get_name() + "' must only take out arguments";
					context->errors->push_back(error);
				}
			}

			// Don't enter the scope in this pass
			return false;
		} else if (!node->get_is_native()) {
			wl_assert(pass == k_ast_validator_pass_validate_syntax);

			// Prepare for processing in scope visitor
			wl_assert(context->module_for_top_scope == nullptr);
			context->module_for_top_scope = node;
			push_scope();
			return true;
		} else {
			// Don't visit native modules
			return false;
		}
	}

	virtual void end_visit(const c_ast_node_module_declaration *node) {}

	virtual bool begin_visit(const c_ast_node_named_value_declaration *node) {
		s_ast_validator_context::s_scope &scope = context->scope_stack.front();

		detect_statements_after_return(scope, node);

		// Add the identifier to the scope
		if (add_identifier_to_scope(0, node->get_name(), node)) {
			s_ast_validator_context::s_named_value_context named_value_context;
			if (node->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_none ||
				node->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_out) {
				// Locals and outputs start out being unassigned
				named_value_context.statement_assigned = -1;
			} else {
				// Module inputs are already assigned at the beginning of the module
				wl_assert(node->get_qualifier() == c_ast_node_named_value_declaration::k_qualifier_in);
				named_value_context.statement_assigned = context->scope_stack.front().current_statement;
			}

			context->scope_stack.front().named_value_contexts.insert(std::make_pair(node, named_value_context));
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_named_value_declaration *node) {
		wl_assert(context->expression_expectation_stack.empty());
		s_ast_validator_context::s_scope &scope = context->scope_stack.front();
		scope.current_statement++;
	}

	virtual bool begin_visit(const c_ast_node_named_value_assignment *node) {
		s_ast_validator_context::s_scope &scope = context->scope_stack.front();

		detect_statements_after_return(scope, node);

		wl_assert(context->expression_expectation_stack.empty());

		if (!node->get_named_value().empty()) {
			// This is a "direct" assignment
			// First make sure this is actually a named value
			const c_ast_node *identifier = find_identifier(0, node->get_named_value());
			if (!identifier) {
				s_compiler_result error;
				error.result = k_compiler_result_undeclared_identifier;
				error.source_location = node->get_source_location();
				error.message = "Identifier '" + node->get_named_value() + "' has not been declared";
				context->errors->push_back(error);
			} else if (identifier->get_type() != k_ast_node_type_named_value_declaration) {
				s_compiler_result error;
				error.result = k_compiler_result_type_mismatch;
				error.source_location = node->get_source_location();
				error.message = "Identifier '" + node->get_named_value() + "' is not a named value";
				context->errors->push_back(error);
			} else {
				// Look up the context
				s_ast_validator_context::s_named_value_context *match = find_named_value_context(0, identifier);
				// Since we've verified that this is a named value, it must already have a context
				wl_assert(match != nullptr);

				if (match->statement_assigned >= 0) {
					s_compiler_result error;
					error.result = k_compiler_result_type_mismatch;
					error.source_location = node->get_source_location();
					error.message = "Named value '" + node->get_named_value() + "' has already been assigned";
					context->errors->push_back(error);
				} else {
					match->statement_assigned = scope.current_statement;
				}
			}

			// We expect the assignment expression to yield a value
			context->expression_expectation_stack.push(s_ast_validator_context::k_expression_expectation_value);
		} else {
			// The expression can be either a value or a valueless module call, but should not itself be an assignment
			context->expression_expectation_stack.push(
				s_ast_validator_context::k_expression_expectation_value_or_valueless);
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_named_value_assignment *node) {
		wl_assert(context->expression_expectation_stack.empty());
		s_ast_validator_context::s_scope &scope = context->scope_stack.front();
		scope.current_statement++;
	}

	virtual bool begin_visit(const c_ast_node_return_statement *node) {
		s_ast_validator_context::s_scope &scope = context->scope_stack.front();

		// Return statements will fail at the parser stage if they are outside of a module
		wl_assert(scope.module_for_scope != nullptr);

		detect_statements_after_return(scope, node);

		if (scope.return_statement >= 0) {
			s_compiler_result error;
			error.result = k_compiler_result_duplicate_identifier;
			error.source_location = node->get_source_location();
			error.message = "Module '" + scope.module_for_scope->get_name() + "' has multiple return statements";
			context->errors->push_back(error);
		}

		if (!scope.module_for_scope->get_has_return_value()) {
			s_compiler_result error;
			error.result = k_compiler_result_extraneous_return_statement;
			error.source_location = node->get_source_location();
			error.message = "Module '" + scope.module_for_scope->get_name() + "' has extraneous return statement";
			context->errors->push_back(error);
		}

		if (scope.return_statement < 0) {
			scope.return_statement = scope.current_statement;
		}

		wl_assert(context->expression_expectation_stack.empty());
		// The return statement must return a value
		context->expression_expectation_stack.push(s_ast_validator_context::k_expression_expectation_value);

		return true;
	}

	virtual void end_visit(const c_ast_node_return_statement *node) {
		wl_assert(context->expression_expectation_stack.empty());
		s_ast_validator_context::s_scope &scope = context->scope_stack.front();
		scope.current_statement++;
	}

	virtual bool begin_visit(const c_ast_node_expression *node) {
		// Since we're about to evaluate an expression, we should have an expectation
		wl_assert(!context->expression_expectation_stack.empty());
		return true;
	}

	virtual void end_visit(const c_ast_node_expression *node) {
		// When we finish evaluating an expression, we're done with its expectation, so pop it from the stack
		context->expression_expectation_stack.pop();
	}

	virtual bool begin_visit(const c_ast_node_constant *node) {
		s_ast_validator_context::e_expression_expectation expectation = context->expression_expectation_stack.top();

		if (expectation == s_ast_validator_context::k_expression_expectation_assignment) {
			// We expected an unassigned named value but we got a constant
			s_compiler_result error;
			error.result = k_compiler_result_unassigned_named_value_expected;
			error.source_location = node->get_source_location();
			error.message =
				"Expected unassigned named value but got constant '" + std::to_string(node->get_value()) + "' instead";
			context->errors->push_back(error);
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_constant *node) {}

	virtual bool begin_visit(const c_ast_node_named_value *node) {
		s_ast_validator_context::s_scope &scope = context->scope_stack.front();
		s_ast_validator_context::e_expression_expectation expectation = context->expression_expectation_stack.top();

		// First make sure this is actually a named value
		const c_ast_node *identifier = find_identifier(0, node->get_name());
		if (!identifier) {
			s_compiler_result error;
			error.result = k_compiler_result_undeclared_identifier;
			error.source_location = node->get_source_location();
			error.message = "Identifier '" + node->get_name() + "' has not been declared";
			context->errors->push_back(error);
		} else if (identifier->get_type() != k_ast_node_type_named_value_declaration) {
			s_compiler_result error;
			error.result = k_compiler_result_type_mismatch;
			error.source_location = node->get_source_location();
			error.message = "Identifier '" + node->get_name() + "' is not a named value";
			context->errors->push_back(error);
		} else {
			// Look up the context
			s_ast_validator_context::s_named_value_context *match = find_named_value_context(0, identifier);
			// Since we've verified that this is a named value, it must already have a context
			wl_assert(match != nullptr);

			if (expectation == s_ast_validator_context::k_expression_expectation_assignment) {
				// We expect that this named value is not yet assigned
				if (match->statement_assigned >= 0) {
					s_compiler_result error;
					error.result = k_compiler_result_unassigned_named_value_expected;
					error.source_location = node->get_source_location();
					error.message = "Named value '" + node->get_name() + "' has already been assigned";
					context->errors->push_back(error);
				} else {
					match->statement_assigned = scope.current_statement;
				}
			} else {
				wl_assert(expectation == s_ast_validator_context::k_expression_expectation_value ||
					expectation == s_ast_validator_context::k_expression_expectation_value_or_valueless);

				if (match->statement_assigned >= 0 &&
					match->statement_assigned < scope.current_statement) {
					// This named value has been assigned, and its assignment was not during this statement, so it is
					// safe to use.
				} else {
					s_compiler_result error;
					error.result = k_compiler_result_unassigned_named_value_used;
					error.source_location = node->get_source_location();
					error.message = "Named value '" + node->get_name() + "' has not been assigned";
					context->errors->push_back(error);
				}
			}
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_named_value *node) {}

	virtual bool begin_visit(const c_ast_node_module_call *node) {
		s_ast_validator_context::s_scope &scope = context->scope_stack.front();
		s_ast_validator_context::e_expression_expectation expectation = context->expression_expectation_stack.top();

		bool push_default_expectations = true;

		// First make sure this is actually a module value
		const c_ast_node *identifier = find_identifier(0, node->get_name());
		if (!identifier) {
			s_compiler_result error;
			error.result = k_compiler_result_undeclared_identifier;
			error.source_location = node->get_source_location();
			error.message = "Identifier '" + node->get_name() + "' has not been declared";
			context->errors->push_back(error);
		} else if (identifier->get_type() != k_ast_node_type_module_declaration) {
			s_compiler_result error;
			error.result = k_compiler_result_type_mismatch;
			error.source_location = node->get_source_location();
			error.message = "Identifier '" + node->get_name() + "' is not a module";
			context->errors->push_back(error);
		} else {
			const c_ast_node_module_declaration *module_declaration =
				static_cast<const c_ast_node_module_declaration *>(identifier);

			// Add this module to the list of module calls made from the current module, if it has not yet appeared
			size_t module_call_index;
			for (module_call_index = 0; module_call_index < context->module_calls.size(); module_call_index++) {
				if (context->module_calls[module_call_index].module == scope.module_for_scope) {
					break;
				}
			}
			wl_assert(module_call_index < context->module_calls.size());

			s_ast_validator_context::s_module_call &module_call = context->module_calls[module_call_index];
			wl_assert(module_call.module == scope.module_for_scope);
			bool subcall_found = false;
			for (size_t subcall_index = 0;
				 !subcall_found && subcall_index < module_call.subcalls.size();
				 subcall_index++) {
				if (module_call.subcalls[subcall_index] == module_declaration) {
					subcall_found = true;
				}
			}

			if (!subcall_found) {
				module_call.subcalls.push_back(module_declaration);
			}

			if (expectation == s_ast_validator_context::k_expression_expectation_assignment) {
				s_compiler_result error;
				error.result = k_compiler_result_unassigned_named_value_expected;
				error.source_location = node->get_source_location();
				error.message =
					"Expected unassigned named value but got module '" + module_declaration->get_name() + "' instead";
				context->errors->push_back(error);
			} else if (expectation == s_ast_validator_context::k_expression_expectation_value &&
				!module_declaration->get_has_return_value()) {
				// We expected a value, but this module is valueless
				s_compiler_result error;
				error.result = k_compiler_result_unassigned_named_value_expected;
				error.source_location = node->get_source_location();
				error.message = "Module '" + module_declaration->get_name() + "' has no value";
				context->errors->push_back(error);
			} else {
				wl_assert(module_declaration->get_has_return_value() ||
					expectation == s_ast_validator_context::k_expression_expectation_value_or_valueless);
			}

			// We've identified that this call is to a valid module, but we still must verify that the number of
			// arguments provided is correct. Once we have done this, depending on the argument type (in or out), we
			// will push an "expectation" onto the stack so that the sub-expression knows what type it should be. If we
			// have too many arguments, we will just push a default expectation for those ones (see comment below).
			// Note that we push in reverse since we are using a stack, meaning the first argument's expectation is on
			// the top of the stack.
			size_t required_expectations = node->get_argument_count();
			if (node->get_argument_count() < module_declaration->get_argument_count()) {
				s_compiler_result error;
				error.result = k_compiler_result_incorrect_argument_count;
				error.source_location = node->get_source_location();
				error.message = "Too few arguments for module call '" + module_declaration->get_name() +
					"': expected " + std::to_string(module_declaration->get_argument_count()) +
					", got " + std::to_string(node->get_argument_count());
				context->errors->push_back(error);
			} else if (node->get_argument_count() > module_declaration->get_argument_count()) {
				s_compiler_result error;
				error.result = k_compiler_result_incorrect_argument_count;
				error.source_location = node->get_source_location();
				error.message = "Too many arguments for module call '" + module_declaration->get_name() +
					"': expected " + std::to_string(module_declaration->get_argument_count()) +
					", got " + std::to_string(node->get_argument_count());
				context->errors->push_back(error);

				// Push default expectations for the extra arguments at the end
				for (size_t arg = module_declaration->get_argument_count(); arg < required_expectations; arg++) {
					context->expression_expectation_stack.push(s_ast_validator_context::k_expression_expectation_value);
				}

				required_expectations = module_declaration->get_argument_count();
			}

			// Push expectations in reverse, up to the number of arguments actually provided
			for (size_t arg = 0; arg < required_expectations; arg++) {
				size_t argument_index = required_expectations - arg - 1;

				// For in arguments, we expect a value. For out arguments, we expect an assignment.
				c_ast_node_named_value_declaration::e_qualifier qualifier =
					module_declaration->get_argument(argument_index)->get_qualifier();
				wl_assert(qualifier == c_ast_node_named_value_declaration::k_qualifier_in ||
					qualifier == c_ast_node_named_value_declaration::k_qualifier_out);

				s_ast_validator_context::e_expression_expectation expectation =
					(qualifier == c_ast_node_named_value_declaration::k_qualifier_in) ?
					s_ast_validator_context::k_expression_expectation_value :
					s_ast_validator_context::k_expression_expectation_assignment;
				context->expression_expectation_stack.push(expectation);
			}

			push_default_expectations = false;
		}

		if (push_default_expectations) {
			// This wasn't a valid module call, so just push a "default" expectation for each argument. We will say the
			// default expectation is a value, since that's probably the most common usage.
			for (size_t index = 0; index < node->get_argument_count(); index++) {
				context->expression_expectation_stack.push(s_ast_validator_context::k_expression_expectation_value);
			}
		}

		return true;
	}

	virtual void end_visit(const c_ast_node_module_call *node) {}
};

s_compiler_result c_ast_validator::validate(const c_ast_node *ast, std::vector<s_compiler_result> &out_errors) {
	wl_assert(ast != nullptr);

	s_compiler_result result;
	result.clear();

	s_ast_validator_context context;
	context.errors = &out_errors;

	c_ast_validator_visitor visitor;
	visitor.context = &context;
	visitor.pass = k_ast_validator_pass_register_global_identifiers;
	ast->iterate(&visitor);

	visitor.pass = k_ast_validator_pass_validate_syntax;
	ast->iterate(&visitor);

	// Validate that module calls aren't cyclic by attempting to find a topological ordering
	while (true) {
		// Find an unvisited module
		size_t index;
		for (index = 0; index < context.module_calls.size(); index++) {
			if (!context.module_calls[index].visited) {
				break;
			}
		}

		if (index < context.module_calls.size()) {
			if (!visit_module(context, index)) {
				s_compiler_result error;
				error.result = k_compiler_result_cyclic_module_call;
				error.message = "Cyclic module call(s) detected";
				context.errors->push_back(error);
				break;
			}
		} else {
			break;
		}
	}

	// Validate that an entry point was found
	if (!context.entry_point_found) {
		s_compiler_result error;
		error.result = k_compiler_result_no_entry_point;
		error.message = "No entry point '" + std::string(k_entry_point_name) + "' found";
		context.errors->push_back(error);
	}

	if (!out_errors.empty()) {
		// Don't associate the error with a particular file, those errors are collected through out_errors
		result.result = k_compiler_result_syntax_error;
		result.message = "Syntax error(s) detected";
	}

	return result;
}