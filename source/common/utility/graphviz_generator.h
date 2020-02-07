#pragma once

#include "common/common.h"

#include <vector>

extern const char *k_graphviz_file_extension;

class c_graphviz_node {
public:
	c_graphviz_node();

	void set_name(const char *name);
	void set_shape(const char *shape);
	void set_style(const char *style);
	void set_peripheries(uint32 peripheries);
	void set_orientation(real32 orientation);
	void set_margin(real32 margin_x, real32 margin_y);
	void set_label(const char *label);

	const char *get_name() const;
	const char *get_shape() const;
	const char *get_style() const;
	uint32 get_peripheries() const;
	real32 get_orientation() const;
	real32 get_margin_x() const;
	real32 get_margin_y() const;
	const char *get_label() const;

private:
	std::string m_name;
	std::string m_shape;
	std::string m_style;
	uint32 m_peripheries;
	real32 m_orientation;
	real32 m_margin_x;
	real32 m_margin_y;
	std::string m_label;
};

class c_graphviz_generator {
public:
	void set_graph_name(const char *graph_name);
	void add_node(const c_graphviz_node &node);
	void add_edge(const char *from_node_name, const char *to_node_name);

	bool output_to_file(const char *fname);

	static std::string escape_string(const char *str);

private:
	std::string m_graph_name;
	std::vector<c_graphviz_node> m_nodes;
	std::vector<std::pair<std::string, std::string>> m_edges;
};

