#include "common/utility/graphviz_generator.h"

#include <fstream>

const char *k_graphviz_file_extension = "gv";

static const real32 k_default_margin_x = 0.11f;
static const real32 k_default_margin_y = 0.055f;

c_graphviz_node::c_graphviz_node() {
	m_peripheries = 1;
	m_orientation = 0.0f;
	m_margin_x = k_default_margin_x;
	m_margin_y = k_default_margin_y;
}

void c_graphviz_node::set_name(const char *name) {
	m_name = name;
}

void c_graphviz_node::set_shape(const char *shape) {
	m_shape = shape;
}

void c_graphviz_node::set_style(const char *style) {
	m_style = style;
}

void c_graphviz_node::set_peripheries(uint32 peripheries) {
	m_peripheries = peripheries;
}

void c_graphviz_node::set_orientation(real32 orientation) {
	m_orientation = orientation;
}

void c_graphviz_node::set_margin(real32 margin_x, real32 margin_y) {
	m_margin_x = margin_x;
	m_margin_y = margin_y;
}

void c_graphviz_node::set_label(const char *label) {
	m_label = label;
}

const char *c_graphviz_node::get_name() const {
	return m_name.c_str();
}

const char *c_graphviz_node::get_shape() const {
	return m_shape.c_str();
}

const char *c_graphviz_node::get_style() const {
	return m_style.c_str();
}

uint32 c_graphviz_node::get_peripheries() const {
	return m_peripheries;
}

real32 c_graphviz_node::get_orientation() const {
	return m_orientation;
}

real32 c_graphviz_node::get_margin_x() const {
	return m_margin_x;
}

real32 c_graphviz_node::get_margin_y() const {
	return m_margin_y;
}

const char *c_graphviz_node::get_label() const {
	return m_label.c_str();
}

void c_graphviz_generator::set_graph_name(const char *graph_name) {
	m_graph_name = graph_name;
}

void c_graphviz_generator::add_node(const c_graphviz_node &node) {
	m_nodes.push_back(node);
}

void c_graphviz_generator::add_edge(const char *from_node_name, const char *to_node_name) {
	m_edges.push_back(std::make_pair<std::string, std::string>(from_node_name, to_node_name));
}

bool c_graphviz_generator::output_to_file(const char *fname) {
	std::ofstream out(fname);

	out << "digraph " << m_graph_name << " {\n";

	// Declare defaults
	out << "node [shape=\"ellipse\", style=\"\", peripheries=1, orientation=0, "
		"margin=\"" << k_default_margin_x << "," << k_default_margin_y << "\"];\n";

	// Declare all nodes first
	for (size_t node_index = 0; node_index < m_nodes.size(); node_index++) {
		const c_graphviz_node &node = m_nodes[node_index];

		out << node.get_name() << " [";
		bool first = true;

		if (node.get_shape()) {
			out << (first ? "" : ", ") << "shape=\"" << node.get_shape() << "\"";
			first = false;
		}

		if (node.get_style()) {
			out << (first ? "" : ", ") << "style=\"" << node.get_style() << "\"";
			first = false;
		}

		if (node.get_peripheries() != 1) {
			out << (first ? "" : ", ") << "peripheries=" << node.get_peripheries();
			first = false;
		}

		if (node.get_orientation() != 0.0f) {
			out << (first ? "" : ", ") << "orientation=" << node.get_orientation();
			first = false;
		}

		// Shouldn't really be comparing floats for equality but I think it's okay in this case
		if (node.get_margin_x() != k_default_margin_x ||
			node.get_margin_y() != k_default_margin_y) {
			out << (first ? "" : ", ") << "margin=\"" << node.get_margin_x() << "," << node.get_margin_y() << "\"";
			first = false;
		}

		out << (first ? "" : ", ") << "label=\"" << node.get_label() << "\"];\n";
	}

	// Declare all the edges
	for (size_t edge_index = 0; edge_index < m_edges.size(); edge_index++) {
		const std::pair<std::string, std::string> &edge = m_edges[edge_index];

		out << edge.first << " -> " << edge.second << ";\n";
	}

	out << "}\n";

	if (out.fail()) {
		return false;
	}

	return true;
}

std::string c_graphviz_generator::escape_string(const char *str) {
	std::string result;

	for (const char *c = str; *c != '\0'; c++) {
		char ch = *c;

		if (ch == '"') {
			result += "&quot;";
		} else if (ch == '&') {
			result += "&amp;";
		} else if (ch == '<') {
			result += "&lt;";
		} else if (ch == '>') {
			result += "&gt;";
		} else if (ch >= 32 && ch <= 126) {
			result.push_back(ch);
		} else {
			// Use the numeric ASCII code
			result.push_back('&');
			result += std::to_string(static_cast<uint8>(ch));
			result.push_back(';');
		}
	}

	return result;
}
