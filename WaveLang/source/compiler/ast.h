#ifndef WAVELANG_AST_H__
#define WAVELANG_AST_H__

#include "common/common.h"
#include "compiler/compiler_utility.h"
#include <vector>

extern const char *k_entry_point_name;

// List of types
enum e_ast_data_type {
	k_ast_data_type_void,
	k_ast_data_type_module,
	k_ast_data_type_real,
	k_ast_data_type_bool,
	k_ast_data_type_string,

	k_ast_data_type_count
};

enum e_ast_qualifier {
	k_ast_qualifier_none,
	k_ast_qualifier_in,
	k_ast_qualifier_out,

	k_ast_qualifier_count
};

const char *get_ast_data_type_string(e_ast_data_type data_type);

// List of all AST nodes
enum e_ast_node_type {
	k_ast_node_type_scope,						// A list of expressions
	k_ast_node_type_module_declaration,			// Declaration of a module
	k_ast_node_type_named_value_declaration,	// Declaration of a named value
	k_ast_node_type_assignment,					// Assignment of an expression to a named value
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
	c_ast_node_named_value_assignment();
	virtual ~c_ast_node_named_value_assignment();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void set_named_value(const std::string &named_value);
	const std::string &get_named_value() const;

	void set_expression(c_ast_node_expression *expression);
	c_ast_node_expression *get_expression();
	const c_ast_node_expression *get_expression() const;

private:
	std::string m_named_value;				// Named value to be assigned to
	c_ast_node_expression *m_expression;	// Expression to be assigned
};

class c_ast_node_return_statement : public c_ast_node {
public:
	c_ast_node_return_statement();
	virtual ~c_ast_node_return_statement();
	virtual void iterate(c_ast_node_visitor *visitor);
	virtual void iterate(c_ast_node_const_visitor *visitor) const;

	void set_expression(c_ast_node_expression *assignment);
	c_ast_node_expression *get_expression();
	const c_ast_node_expression *get_expression() const;

private:
	// Expression to be returned
	c_ast_node_expression *m_expression;
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

public:
	e_ast_data_type m_data_type;	// Data type
	union {
		real32 m_real_value;		// The value, if a real
		bool m_bool_value;			// The value, if a bool
	};
	std::string m_string_value;		// The value, if a string (can't be in the union since it's not POD)
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

	void set_name(const std::string &name);
	const std::string &get_name() const;

	void add_argument(c_ast_node_expression *argument);
	size_t get_argument_count() const;
	c_ast_node_expression *get_argument(size_t index);
	const c_ast_node_expression *get_argument(size_t index) const;

private:
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