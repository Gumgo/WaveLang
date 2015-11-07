#ifndef WAVELANG_AST_H__
#define WAVELANG_AST_H__

#include "common/common.h"
#include "compiler/compiler_utility.h"
#include <vector>

// $TODO array support:
// Arrays must be of fixed size
// Each element is a separately-tracked basic type
// Constant-dereference resolves to a no-op and directly accesses that value
// Variable-dereference:
//   Create a new argument type in execution/task graph
//   Its value consists of an array of node indices
//   The node indices are known constants at compile time, so these objects can be precomputed
//   Select sub-value
// Pass arrays to modules
//   Pass by value - copy-on-write semantics (just like all "tracked" values currently are)
//   i.e. a different "array" value exists for each dereference call

extern const char *k_entry_point_name;

// List of types
enum e_ast_data_type {
	k_ast_data_type_void,
	k_ast_data_type_module,
	k_ast_data_type_real,
	k_ast_data_type_bool,
	k_ast_data_type_string,
	k_ast_data_type_real_array,
	k_ast_data_type_bool_array,
	k_ast_data_type_string_array,

	k_ast_data_type_count
};

enum e_ast_qualifier {
	k_ast_qualifier_none,
	k_ast_qualifier_in,
	k_ast_qualifier_out,

	k_ast_qualifier_count
};

const char *get_ast_data_type_string(e_ast_data_type data_type);
bool is_ast_data_type_array(e_ast_data_type data_type);
e_ast_data_type get_element_from_array_ast_data_type(e_ast_data_type array_data_type);
e_ast_data_type get_array_from_element_ast_data_type(e_ast_data_type element_data_type);

// List of all AST nodes
enum e_ast_node_type {
	k_ast_node_type_scope,						// A list of expressions
	k_ast_node_type_module_declaration,			// Declaration of a module
	k_ast_node_type_named_value_declaration,	// Declaration of a named value
	k_ast_node_type_named_value_assignment,		// Assignment of an expression to a named value
	k_ast_node_type_repeat_loop,				// Loop which repeats a constant number of times
	k_ast_node_type_return_statement,			// Return statement of a module
	k_ast_node_type_expression,					// An expression, which is either a constant or a module call
	k_ast_node_type_constant,					// A constant value
	k_ast_node_type_named_value,				// A named value
	k_ast_node_type_module_call,				// A call to a module, which includes built in operators

	k_ast_node_type_count
};

// Forward declare types
class c_ast_node_visitor;
class c_ast_node_const_visitor;
class c_ast_node;
class c_ast_node_scope;
class c_ast_node_module_declaration;
class c_ast_node_named_value_declaration;
class c_ast_node_return_statement;
class c_ast_node_repeat_loop;
class c_ast_node_expression;
class c_ast_node_constant;
class c_ast_node_named_value;
class c_ast_node_module_call;

class c_ast_node {
public:
	virtual ~c_ast_node();
	virtual void iterate(c_ast_node_visitor *visitor) = 0;
	virtual void iterate(c_ast_node_const_visitor *visitor) const = 0;
	e_ast_node_type get_type() const;

	void set_source_location(const s_compiler_source_location &source_location);
	s_compiler_source_location get_source_location() const;

protected:
	c_ast_node(e_ast_node_type type);

private:
	// Location in the source code
	s_compiler_source_location m_source_location;

	// Subclass type
	e_ast_node_type m_type;
};

class c_ast_node_scope : public c_ast_node {
public:
	c_ast_node_scope();
	virtual ~c_ast_node_scope();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void add_child(c_ast_node *child);
	size_t get_child_count() const;
	c_ast_node *get_child(size_t index);
	const c_ast_node *get_child(size_t index) const;

private:
	// List of nodes within this scope
	std::vector<c_ast_node *> m_children;
};

class c_ast_node_module_declaration : public c_ast_node {
public:
	c_ast_node_module_declaration();
	virtual ~c_ast_node_module_declaration();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void set_is_native(bool is_native);
	bool get_is_native() const;

	void set_native_module_index(uint32 native_module_index);
	uint32 get_native_module_index() const;

	void set_name(const std::string &name);
	const std::string &get_name() const;

	void set_return_type(e_ast_data_type return_type);
	e_ast_data_type get_return_type() const;

	void add_argument(c_ast_node_named_value_declaration *argument);
	size_t get_argument_count() const;
	c_ast_node_named_value_declaration *get_argument(size_t index);
	const c_ast_node_named_value_declaration *get_argument(size_t index) const;

	void set_scope(c_ast_node_scope *scope);
	c_ast_node_scope *get_scope();
	const c_ast_node_scope *get_scope() const;

private:
	bool m_is_native;												// Whether this module is native
	uint32 m_native_module_index;									// Native module index, if native
	std::string m_name;												// Name of this module
	e_ast_data_type m_return_type;									// Module return type
	std::vector<c_ast_node_named_value_declaration *> m_arguments;	// List of arguments for this module
	c_ast_node_scope *m_scope;										// The scope of this module
};

class c_ast_node_named_value_declaration : public c_ast_node {
public:
	c_ast_node_named_value_declaration();
	virtual ~c_ast_node_named_value_declaration();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void set_name(const std::string &name);
	const std::string &get_name() const;

	void set_qualifier(e_ast_qualifier qualifier);
	e_ast_qualifier get_qualifier() const;

	void set_data_type(e_ast_data_type data_type);
	e_ast_data_type get_data_type() const;

private:
	std::string m_name;				// Name of the value
	e_ast_qualifier m_qualifier;	// Value's qualifier
	e_ast_data_type m_data_type;	// Type of data
};

class c_ast_node_named_value_assignment : public c_ast_node {
public:
	// Note: these nodes can also be created as "anonymous" (without a name) in order to reference existing expressions
	// which didn't have real assignments - e.g. a repeat loop references an "anonymous" named value assignment for its
	// expression

	c_ast_node_named_value_assignment();
	virtual ~c_ast_node_named_value_assignment();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void set_is_valid_named_value(bool is_valid_named_value);
	bool get_is_valid_named_value() const;

	void set_named_value(const std::string &named_value);
	const std::string &get_named_value() const;

	void set_array_index_expression(c_ast_node_expression *array_index_expression);
	c_ast_node_expression *get_array_index_expression();
	const c_ast_node_expression *get_array_index_expression() const;

	void set_expression(c_ast_node_expression *expression);
	c_ast_node_expression *get_expression();
	const c_ast_node_expression *get_expression() const;

private:
	// The named value being assigned is expressed with a general purpose expression. This is necessary because we allow
	// assignment of either direct named values or of array indices, and I was unable to get simple parser rules working
	// for both of those cases (they conflicted with isolated expression statements). Therefore, the parser rule for
	// assignment looks like "expression := expression". For a valid named value assignment node, the LHS expression
	// must be of one of the two forms:
	// identifier
	// identifier[expr]
	// If this isn't true, m_is_valid_named_value should be set to false.

	bool m_is_valid_named_value;			// Whether the named value expression was valid
	std::string m_named_value;				// Named value to be assigned to, or empty string if anonymous
	c_ast_node_expression *m_expression;	// Expression to be assigned

	// Expression resolving to an array index, if this named value is an array index assignment; null otherwise
	c_ast_node_expression *m_array_index_expression;
};

class c_ast_node_return_statement : public c_ast_node {
public:
	c_ast_node_return_statement();
	virtual ~c_ast_node_return_statement();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void set_expression(c_ast_node_expression *expression);
	c_ast_node_expression *get_expression();
	const c_ast_node_expression *get_expression() const;

private:
	// Expression to be returned
	c_ast_node_expression *m_expression;
};

class c_ast_node_repeat_loop : public c_ast_node {
public:
	c_ast_node_repeat_loop();
	virtual ~c_ast_node_repeat_loop();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void set_expression(c_ast_node_named_value_assignment *named_value);
	c_ast_node_named_value_assignment *get_expression();
	const c_ast_node_named_value_assignment *get_expression() const;

	void set_scope(c_ast_node_scope *scope);
	c_ast_node_scope *get_scope();
	const c_ast_node_scope *get_scope() const;

private:
	// Expression holding the loop count, stored inside of an existing named value assignment node
	c_ast_node_named_value_assignment *m_expression;

	// The scope of the repeat loop
	c_ast_node_scope *m_scope;
};

class c_ast_node_expression : public c_ast_node {
public:
	c_ast_node_expression();
	virtual ~c_ast_node_expression();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void set_expression_value(c_ast_node *expression_value);
	c_ast_node *get_expression_value();
	const c_ast_node *get_expression_value() const;

private:
	// Points to the actual type of the expression
	c_ast_node *m_expression_value;
};

class c_ast_node_constant : public c_ast_node {
public:
	c_ast_node_constant();
	virtual ~c_ast_node_constant();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	e_ast_data_type get_data_type() const;

	void set_real_value(real32 value);
	real32 get_real_value() const;

	void set_bool_value(bool value);
	bool get_bool_value() const;

	void set_string_value(const std::string &value);
	const std::string &get_string_value() const;

	void set_array(e_ast_data_type element_data_type);
	void add_array_value(c_ast_node_expression *value);
	size_t get_array_count() const;
	c_ast_node_expression *get_array_value(size_t index);
	const c_ast_node_expression *get_array_value(size_t index) const;

public:
	e_ast_data_type m_data_type;	// Data type
	union {
		real32 m_real_value;		// The value, if a real
		bool m_bool_value;			// The value, if a bool
	};
	std::string m_string_value;		// The value, if a string (can't be in the union since it's not POD)

	// The list of values, if an array
	std::vector<c_ast_node_expression *> m_array_values;
};

class c_ast_node_named_value : public c_ast_node {
public:
	c_ast_node_named_value();
	virtual ~c_ast_node_named_value();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void set_name(const std::string &name);
	const std::string &get_name() const;

public:
	// The name of this value
	std::string m_name;
};

class c_ast_node_module_call : public c_ast_node {
public:
	c_ast_node_module_call();
	virtual ~c_ast_node_module_call();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void set_is_invoked_using_operator(bool is_invoked_using_operator);
	bool get_is_invoked_using_operator() const;

	void set_name(const std::string &name);
	const std::string &get_name() const;

	void add_argument(c_ast_node_expression *argument);
	size_t get_argument_count() const;
	c_ast_node_expression *get_argument(size_t index);
	const c_ast_node_expression *get_argument(size_t index) const;

private:
	bool m_is_invoked_using_operator;					// Whether this module was invoked by using an operator
	std::string m_name;									// Name of the module to call
	std::vector<c_ast_node_expression *> m_arguments;	// An expression for each argument
};

class c_ast_node_visitor {
public:
	virtual bool begin_visit(c_ast_node_scope *node) = 0;
	virtual void end_visit(c_ast_node_scope *node) = 0;

	virtual bool begin_visit(c_ast_node_module_declaration *node) = 0;
	virtual void end_visit(c_ast_node_module_declaration *node) = 0;

	virtual bool begin_visit(c_ast_node_named_value_declaration *node) = 0;
	virtual void end_visit(c_ast_node_named_value_declaration *node) = 0;

	virtual bool begin_visit(c_ast_node_named_value_assignment *node) = 0;
	virtual void end_visit(c_ast_node_named_value_assignment *node) = 0;

	virtual bool begin_visit(c_ast_node_return_statement *node) = 0;
	virtual void end_visit(c_ast_node_return_statement *node) = 0;

	virtual bool begin_visit(c_ast_node_repeat_loop *node) = 0;
	virtual void end_visit(c_ast_node_repeat_loop *node) = 0;

	virtual bool begin_visit(c_ast_node_expression *node) = 0;
	virtual void end_visit(c_ast_node_expression *node) = 0;

	virtual bool begin_visit(c_ast_node_constant *node) = 0;
	virtual void end_visit(c_ast_node_constant *node) = 0;

	virtual bool begin_visit(c_ast_node_named_value *node) = 0;
	virtual void end_visit(c_ast_node_named_value *node) = 0;

	virtual bool begin_visit(c_ast_node_module_call *node) = 0;
	virtual void end_visit(c_ast_node_module_call *node) = 0;
};

#define UNIMPLEMENTED_AST_NODE(type)						\
	virtual bool begin_visit(type *node) { return true; }	\
	virtual void end_visit(type *node) {}

class c_ast_node_const_visitor {
public:
	virtual bool begin_visit(const c_ast_node_scope *node) = 0;
	virtual void end_visit(const c_ast_node_scope *node) = 0;

	virtual bool begin_visit(const c_ast_node_module_declaration *node) = 0;
	virtual void end_visit(const c_ast_node_module_declaration *node) = 0;

	virtual bool begin_visit(const c_ast_node_named_value_declaration *node) = 0;
	virtual void end_visit(const c_ast_node_named_value_declaration *node) = 0;

	virtual bool begin_visit(const c_ast_node_named_value_assignment *node) = 0;
	virtual void end_visit(const c_ast_node_named_value_assignment *node) = 0;

	virtual bool begin_visit(const c_ast_node_return_statement *node) = 0;
	virtual void end_visit(const c_ast_node_return_statement *node) = 0;

	virtual bool begin_visit(const c_ast_node_repeat_loop *node) = 0;
	virtual void end_visit(const c_ast_node_repeat_loop *node) = 0;

	virtual bool begin_visit(const c_ast_node_expression *node) = 0;
	virtual void end_visit(const c_ast_node_expression *node) = 0;

	virtual bool begin_visit(const c_ast_node_constant *node) = 0;
	virtual void end_visit(const c_ast_node_constant *node) = 0;

	virtual bool begin_visit(const c_ast_node_named_value *node) = 0;
	virtual void end_visit(const c_ast_node_named_value *node) = 0;

	virtual bool begin_visit(const c_ast_node_module_call *node) = 0;
	virtual void end_visit(const c_ast_node_module_call *node) = 0;
};

#define UNIMPLEMENTED_AST_NODE_CONST(type)						\
	virtual bool begin_visit(const type *node) { return true; }	\
	virtual void end_visit(const type *node) {}

#endif // WAVELANG_AST_H__